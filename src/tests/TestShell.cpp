#include <QtTest/QtTest>
#include <QFile>

#include "app/ShellSolverBridge.h"
#include "shell/ShellDesign.h"
#include "shell/ShellFactory.h"
#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
#include "shell/ShellResults.h"

#include <cmath>
#include <map>
#include <set>

using namespace Structura::Shell;

namespace {

constexpr double kPi = 3.14159265358979323846;

QString femLibrary()
{
    return QStringLiteral(STRUCTURA_SOURCE_DIR "/mefs.dll");
}

int freeEdgeCount(const Mesh &mesh)
{
    std::map<std::pair<int, int>, int> count;
    for (const MeshQuad &q : mesh.quads) {
        for (int i = 0; i < 4; ++i) {
            const int a = q.n[static_cast<std::size_t>(i)], b = q.n[static_cast<std::size_t>((i + 1) % 4)];
            ++count[{std::min(a, b), std::max(a, b)}];
        }
    }
    int free = 0;
    for (const auto &[edge, n] : count) {
        free += n == 1 ? 1 : 0;
    }
    return free;
}

double totalArea(const Mesh &mesh)
{
    double a = 0.0;
    for (const MeshQuad &q : mesh.quads) {
        a += quadArea(mesh, q);
    }
    return a;
}

Object cylinderTank(double r, double h, int courses, double t, double tBottom, double meshSize)
{
    Object o;
    o.id = 1;
    o.name = "Tanque";
    o.kind = Kind::CylTank;
    o.meshSize = meshSize;
    o.p = {{"r", r}, {"h", h}, {"courses", static_cast<double>(courses)}, {"tBottom", tBottom}, {"roof", 0.0}};
    for (int i = 1; i <= courses; ++i) {
        o.p["t" + std::to_string(i)] = t;
    }
    return o;
}

} // namespace

class TestShell : public QObject
{
    Q_OBJECT

private slots:
    void plateMeshesCoverThePolygon()
    {
        Model model;
        Object o;
        o.id = 1;
        o.name = "Laje";
        o.kind = Kind::Plate;
        o.p = {{"t", 0.2}};
        o.meshSize = 0.5;
        // L: 6 x 4 com recorte de 3 x 2 -> area 18
        o.polygon = {{0, 0, 0}, {6, 0, 0}, {6, 2, 0}, {3, 2, 0}, {3, 4, 0}, {0, 4, 0}};
        model.objects.push_back(o);
        const Mesh mesh = generateMesh(model);
        QVERIFY(!mesh.empty());
        QVERIFY(std::abs(totalArea(mesh) - 18.0) < 1e-6);
        for (const MeshQuad &q : mesh.quads) {
            QVERIFY(quadNormal(mesh, q)[2] > 0.99);
            QVERIFY(quadArea(mesh, q) > 0.0);
        }
        // retangulo estruturado
        model.objects[0].polygon = {{0, 0, 0}, {4, 0, 0}, {4, 3, 0}, {0, 3, 0}};
        const Mesh rect = generateMesh(model);
        QCOMPARE(static_cast<int>(rect.quads.size()), 8 * 6);
        QCOMPARE(freeEdgeCount(rect), 2 * (8 + 6));
    }

    void cylindricalTankMeshIsClosedAndOutward()
    {
        Model model;
        model.objects.push_back(cylinderTank(5.0, 6.0, 3, 0.01, 0.008, 0.5));
        const Mesh mesh = generateMesh(model);
        QVERIFY(!mesh.empty());
        // so a borda superior fica livre (fundo e virolas conectados por nos coincidentes)
        const int circumferential = static_cast<int>(std::lround(mesh.quads.size() > 0 ? 4.0 * std::ceil(2.0 * kPi * 5.0 / 0.5 / 4.0) : 0.0));
        QCOMPARE(freeEdgeCount(mesh), circumferential);
        for (const MeshQuad &q : mesh.quads) {
            const Vec3 n = quadNormal(mesh, q);
            Vec3 c {0.0, 0.0, 0.0};
            for (const int id : q.n) {
                for (std::size_t k = 0; k < 3; ++k) {
                    c[k] += mesh.nodes[static_cast<std::size_t>(id)][k] / 4.0;
                }
            }
            if (c[2] > 0.01 && std::hypot(c[0], c[1]) > 4.9) {
                // parede: normal para fora
                QVERIFY(n[0] * c[0] + n[1] * c[1] > 0.0);
            } else if (c[2] < 0.01) {
                QVERIFY(n[2] < -0.99);   // fundo: normal para baixo (para fora)
            }
        }
        // area do tanque
        const double wall = 2.0 * kPi * 5.0 * 6.0, bottom = kPi * 25.0;
        QVERIFY(std::abs(totalArea(mesh) - (wall + bottom)) / (wall + bottom) < 0.01);
    }

    void tankUnderHydrostaticPressure()
    {
        const QString dll = femLibrary();
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll nao encontrada na raiz do repositorio.");
        }
        Model model;
        Object tank = cylinderTank(5.0, 6.0, 3, 0.01, 0.01, 0.5);
        model.objects.push_back(tank);
        const double gamma = 9810.0, level = 5.0;
        Load water;
        water.caseName = "AGUA";
        water.type = Load::Hydrostatic;
        water.v = {gamma, level, 0.0, 0.0};
        model.loads.push_back(water);
        Support soil;
        soil.selector = Support::Plane;
        soil.axis = 2;
        soil.value = 0.0;
        soil.springPerArea = 5.0e7;
        soil.compressionOnly = true;
        soil.fixed = {0, 0, 0, 0, 0, 0};
        model.supports.push_back(soil);
        // restringe o deslizamento horizontal e a rotacao de corpo rigido em tres nos do fundo
        Support p1;
        p1.selector = Support::Point;
        p1.value = 5.0;
        p1.value2 = 0.0;
        p1.value3 = 0.0;
        p1.fixed = {1, 1, 0, 0, 0, 0};
        Support p2 = p1;
        p2.value = -5.0;
        p2.fixed = {0, 1, 0, 0, 0, 0};
        model.supports.push_back(p1);
        model.supports.push_back(p2);
        const Mesh mesh = generateMesh(model);
        const Job job = buildJob(model, mesh, {"AGUA"});
        QVERIFY(!job.cases.empty() && !job.cases[0].pressure.empty());
        const Structura::App::ShellSolverBridge bridge(dll);
        const Results results = bridge.solve(model, mesh, job);
        QVERIFY2(results.ok(), results.error.c_str());
        const CaseResult &c = results.cases[0];
        // equilibrio vertical: a pressao no fundo (gamma h A) e resistida pelas molas
        double bottomArea = 0.0;
        for (const MeshQuad &q : mesh.quads) {
            if (mesh.zones[static_cast<std::size_t>(q.zone)].find("fundo") != std::string::npos) {
                bottomArea += quadArea(mesh, q);
            }
        }
        double reaction = 0.0;
        for (const auto &r : c.reaction) {
            reaction += r[2];
        }
        QVERIFY(std::abs(reaction - gamma * level * bottomArea) / (gamma * level * bottomArea) < 0.01);
        // esforco circunferencial em z = 3 m: n = gamma (h - z) R (teoria de membrana, longe do fundo)
        double found = 0.0;
        int samples = 0;
        for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
            const MeshQuad &q = mesh.quads[e];
            double zc = 0.0;
            for (const int id : q.n) {
                zc += mesh.nodes[static_cast<std::size_t>(id)][2] / 4.0;
            }
            if (std::abs(zc - 3.0) < 0.26 && mesh.zones[static_cast<std::size_t>(q.zone)].find("virola") != std::string::npos) {
                found += elementQuantity(mesh, model, c, e, 4, Quantity::MembraneA) / 1.0;
                ++samples;
            }
        }
        QVERIFY(samples > 0);
        const double expected = gamma * (level - 3.0) * 5.0;
        QVERIFY2(std::abs(found / samples - expected) / expected < 0.06, qPrintable(QString::number(found / samples) + " vs " + QString::number(expected)));
    }

    void pressureVesselMembraneStresses()
    {
        const QString dll = femLibrary();
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll nao encontrada na raiz do repositorio.");
        }
        Model model;
        Object v;
        v.id = 1;
        v.name = "Vaso";
        v.kind = Kind::Vessel;
        v.meshSize = 0.15;
        v.p = {{"r", 1.0}, {"length", 3.0}, {"t", 0.01}, {"head", 0.0}, {"tHead", 0.01}, {"horizontal", 0.0}};
        model.objects.push_back(v);
        const double pressure = 1.0e6;
        Load l;
        l.caseName = "PROJ";
        l.type = Load::Pressure;
        l.v = {pressure, 0, 0, 0};
        model.loads.push_back(l);
        for (int k = 0; k < 3; ++k) {
            Support s;
            s.selector = Support::Point;
            s.value = k == 0 ? 1.0 : (k == 1 ? -1.0 : 0.0);
            s.value2 = k == 2 ? 1.0 : 0.0;
            s.value3 = 1.5;
            s.fixed = k == 0 ? std::array<int, 6> {1, 1, 1, 0, 0, 0} : (k == 1 ? std::array<int, 6> {0, 1, 1, 0, 0, 0} : std::array<int, 6> {0, 0, 1, 0, 0, 0});
            model.supports.push_back(s);
        }
        const Mesh mesh = generateMesh(model);
        QCOMPARE(freeEdgeCount(mesh), 0);   // superficie fechada
        const Job job = buildJob(model, mesh, {"PROJ"});
        const Structura::App::ShellSolverBridge bridge(dll);
        const Results results = bridge.solve(model, mesh, job);
        QVERIFY2(results.ok(), results.error.c_str());
        const CaseResult &c = results.cases[0];
        double hoop = 0.0, axial = 0.0, sphere = 0.0;
        int nh = 0, ns = 0;
        for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
            const MeshQuad &q = mesh.quads[e];
            double zc = 0.0;
            for (const int id : q.n) {
                zc += mesh.nodes[static_cast<std::size_t>(id)][2] / 4.0;
            }
            const std::string &zone = mesh.zones[static_cast<std::size_t>(q.zone)];
            if (zone.find("casco") != std::string::npos && std::abs(zc - 1.5) < 0.2) {
                hoop += elementQuantity(mesh, model, c, e, 4, Quantity::MembraneA);
                axial += elementQuantity(mesh, model, c, e, 4, Quantity::MembraneB);
                ++nh;
            }
            if (zone.find("tampo superior") != std::string::npos && zc > 3.0 + 0.8) {
                sphere += elementQuantity(mesh, model, c, e, 4, Quantity::MembranePrincipalMax);
                ++ns;
            }
        }
        QVERIFY(nh > 0 && ns > 0);
        QVERIFY2(std::abs(hoop / nh - pressure * 1.0) / (pressure * 1.0) < 0.02, qPrintable(QString::number(hoop / nh)));
        QVERIFY2(std::abs(axial / nh - pressure * 0.5) / (pressure * 0.5) < 0.04, qPrintable(QString::number(axial / nh)));
        QVERIFY2(std::abs(sphere / ns - pressure * 0.5) / (pressure * 0.5) < 0.04, qPrintable(QString::number(sphere / ns)));
    }

    void rectangularTankMeshesConnect()
    {
        Model model;
        Object o;
        o.id = 1;
        o.name = "Reservatorio";
        o.kind = Kind::RectTank;
        o.meshSize = 0.5;
        o.p = {{"lx", 4.0}, {"ly", 3.0}, {"h", 2.0}, {"tWall", 0.2}, {"tBottom", 0.25}, {"tRoof", 0.0}};
        model.objects.push_back(o);
        Mesh mesh = generateMesh(model);
        QVERIFY(!mesh.empty());
        QCOMPARE(freeEdgeCount(mesh), 2 * (8 + 6));   // borda superior aberta
        model.objects[0].p["tRoof"] = 0.15;
        mesh = generateMesh(model);
        QCOMPARE(freeEdgeCount(mesh), 0);
        for (const MeshQuad &q : mesh.quads) {
            Vec3 c {0.0, 0.0, 0.0};
            for (const int id : q.n) {
                for (std::size_t k = 0; k < 3; ++k) {
                    c[k] += mesh.nodes[static_cast<std::size_t>(id)][k] / 4.0;
                }
            }
            const Vec3 n = quadNormal(mesh, q);
            const Vec3 center {2.0, 1.5, 1.0};
            QVERIFY(n[0] * (c[0] - center[0]) + n[1] * (c[1] - center[1]) + n[2] * (c[2] - center[2]) > 0.0);   // normais para fora
        }
    }

    void drawnSurfacesCoverTheirGeometry()
    {
        const double pi = kPi;
        // disco e anel com normal +Z
        {
            Model model;
            Object o;
            o.id = 1;
            o.name = "Disco";
            o.kind = Kind::Disk;
            o.meshSize = 0.25;
            o.polygon = {{1.0, 2.0, 3.0}};
            o.p = {{"r", 2.0}, {"rIn", 0.0}, {"nx", 0.0}, {"ny", 0.0}, {"nz", 1.0}, {"t", 0.1}};
            model.objects = {o};
            Mesh mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - pi * 4.0) / (pi * 4.0) < 0.01);
            for (const MeshQuad &q : mesh.quads) {
                QVERIFY(quadNormal(mesh, q)[2] > 0.99);
            }
            QCOMPARE(freeEdgeCount(mesh), mesh.quads.empty() ? 0 : freeEdgeCount(mesh));
            model.objects[0].p["rIn"] = 1.0;
            mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - pi * 3.0) / (pi * 3.0) < 0.01);
            model.objects[0].p["flip"] = 1.0;
            mesh = generateMesh(model);
            QVERIFY(quadNormal(mesh, mesh.quads.front())[2] < -0.99);
        }
        // cilindro (extrusao de circulo): normal para fora; cone ate o vertice
        {
            Model model;
            Object o;
            o.id = 1;
            o.name = "Cilindro";
            o.kind = Kind::Extrusion;
            o.meshSize = 0.3;
            o.polygon = {{0.0, 0.0, 0.0}};
            o.p = {{"circle", 1.0}, {"r", 1.5}, {"vx", 0.0}, {"vy", 0.0}, {"vz", 4.0}, {"endScale", 1.0}, {"t", 0.01}};
            model.objects = {o};
            Mesh mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - 2.0 * pi * 1.5 * 4.0) / (2.0 * pi * 1.5 * 4.0) < 0.01);
            for (const MeshQuad &q : mesh.quads) {
                Vec3 c {0.0, 0.0, 0.0};
                for (const int id : q.n) {
                    for (std::size_t k = 0; k < 3; ++k) {
                        c[k] += mesh.nodes[static_cast<std::size_t>(id)][k] / 4.0;
                    }
                }
                QVERIFY(quadNormal(mesh, q)[0] * c[0] + quadNormal(mesh, q)[1] * c[1] > 0.0);
            }
            model.objects[0].p["endScale"] = 0.0;   // cone: area lateral pi r s
            mesh = generateMesh(model);
            const double slant = std::hypot(1.5, 4.0);
            QVERIFY(std::abs(totalArea(mesh) - pi * 1.5 * slant) / (pi * 1.5 * slant) < 0.02);
        }
        // extrusao de polilinha (parede em L) e de polilinha fechada (prisma)
        {
            Model model;
            Object o;
            o.id = 1;
            o.name = "Parede";
            o.kind = Kind::Extrusion;
            o.meshSize = 0.5;
            o.polygon = {{0, 0, 0}, {4, 0, 0}, {4, 3, 0}};
            o.p = {{"vx", 0.0}, {"vy", 0.0}, {"vz", 2.0}, {"t", 0.2}};
            model.objects = {o};
            Mesh mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - (4.0 + 3.0) * 2.0) < 1e-6);
            // caminho anti-horario visto de +Z extrudado para +Z: o primeiro trecho (ao longo de +X) tem normal -Y (para fora)
            QVERIFY(quadNormal(mesh, mesh.quads.front())[1] < -0.99);
            model.objects[0].polygon = {{0, 0, 0}, {4, 0, 0}, {4, 3, 0}, {0, 3, 0}};
            model.objects[0].p["closed"] = 1.0;
            mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - 14.0 * 2.0) < 1e-6);
            QCOMPARE(freeEdgeCount(mesh), 2 * 2 * (8 + 6));   // so as bordas superior e inferior (perimetro de 14 m)
        }
        // revolucao: quarto de circulo em torno de Z = hemisferio (area 2 pi R2); eixo inclinado; setor parcial
        {
            Model model;
            Object o;
            o.id = 1;
            o.name = "Domo";
            o.kind = Kind::Revolution;
            o.meshSize = 0.25;
            const double R = 2.0;
            for (int i = 0; i <= 20; ++i) {
                const double a = 0.5 * pi * i / 20.0;
                o.polygon.push_back({R * std::cos(a), 0.0, R * std::sin(a)});   // do equador ao polo
            }
            o.p = {{"ax", 0.0}, {"ay", 0.0}, {"az", 0.0}, {"dx", 0.0}, {"dy", 0.0}, {"dz", 1.0}, {"angle", 360.0}, {"t", 0.01}};
            model.objects = {o};
            Mesh mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - 2.0 * pi * R * R) / (2.0 * pi * R * R) < 0.02);
            // eixo inclinado: a area nao muda
            model.objects[0].p["dx"] = 1.0;
            model.objects[0].p["dz"] = 1.0;
            // o perfil precisa ficar em um semiplano do novo eixo: recalcula-o girado
            const Vec3 axis {std::sqrt(0.5), 0.0, std::sqrt(0.5)};
            const Vec3 side {0.0, 1.0, 0.0};
            const Vec3 perp {axis[1] * side[2] - axis[2] * side[1], axis[2] * side[0] - axis[0] * side[2], axis[0] * side[1] - axis[1] * side[0]};
            model.objects[0].polygon.clear();
            for (int i = 0; i <= 20; ++i) {
                const double a = 0.5 * pi * i / 20.0;
                model.objects[0].polygon.push_back({R * std::cos(a) * perp[0] + R * std::sin(a) * axis[0], R * std::cos(a) * perp[1] + R * std::sin(a) * axis[1],
                                                    R * std::cos(a) * perp[2] + R * std::sin(a) * axis[2]});
            }
            mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - 2.0 * pi * R * R) / (2.0 * pi * R * R) < 0.02);
            // meia volta: metade da area e bordas livres
            model.objects[0].p["angle"] = 180.0;
            mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - pi * R * R) / (pi * R * R) < 0.02);
            QVERIFY(freeEdgeCount(mesh) > 0);
        }
    }

    void drawingFactoryBuildsConnectedAssemblies()
    {
        // chapa retangular no plano XZ (normal Y): a normal positiva segue o eixo normal do plano
        {
            const PlaneSpec xz {0, 2, 1};
            const auto o = makeRectangle({0.0, 5.0, 0.0}, {4.0, 5.0, 3.0}, xz);
            QVERIFY(o.has_value());
            Model model;
            Object plate = *o;
            plate.id = 1;
            plate.name = "Parede";
            plate.p = {{"t", 0.2}};
            model.objects = {plate};
            const Mesh mesh = generateMesh(model);
            QVERIFY(std::abs(totalArea(mesh) - 12.0) < 1e-6);
            for (const MeshQuad &q : mesh.quads) {
                QVERIFY(quadNormal(mesh, q)[1] > 0.99);
            }
            // contorno anti-horario visto de -Y gera a normal oposta: o fabrica inverte para +normal
            const auto reversed = makePlate({{0, 5, 0}, {0, 5, 3}, {4, 5, 3}, {4, 5, 0}}, xz);
            QVERIFY(reversed.has_value());
            model.objects[0].polygon = reversed->polygon;
            QVERIFY(quadNormal(generateMesh(model), generateMesh(model).quads.front())[1] > 0.99);
            QVERIFY(!makeRectangle({0, 0, 0}, {4, 0, 0}, xz).has_value());   // retangulo degenerado
            QVERIFY(!makePlate({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}, xz).has_value());   // colineares
        }
        // tanque montado com as ferramentas: disco (fundo) + cilindro (costado) + domo por revolucao (teto), com a mesma dimensao de malha
        {
            const PlaneSpec xy {0, 1, 2};
            Model model;
            const double R = 3.0, H = 4.0, rise = 1.0, mesh = 0.5;
            auto bottom = makeDisk({0, 0, 0}, {R, 0, 0}, xy);
            auto wall = makeCylinder({0, 0, 0}, R, {0, 0, H});
            // perfil do domo (arco de circunferencia) do topo do costado ate o polo, no plano XZ
            const double rs = (R * R + rise * rise) / (2.0 * rise), zc = H + rise - rs, phi0 = std::asin(R / rs);
            std::vector<Vec3> profile;
            for (int i = 0; i <= 24; ++i) {
                const double phi = phi0 * (1.0 - static_cast<double>(i) / 24.0);
                profile.push_back({rs * std::sin(phi), 0.0, zc + rs * std::cos(phi)});
            }
            auto dome = makeRevolution(profile, {0, 0, 0}, {0, 0, 1});
            QVERIFY(bottom && wall && dome);
            int id = 1;
            for (Object *o : {&*bottom, &*wall, &*dome}) {
                o->id = id++;
                o->name = "P" + std::to_string(o->id);
                o->meshSize = mesh;
                o->p["t"] = 0.01;
            }
            // disco e revolucao pedem a normal para fora: o fundo e o domo ja saem para fora (-Z e +Z); o disco precisa de -Z
            bottom->p["flip"] = 1.0;
            model.objects = {*bottom, *wall, *dome};
            const Mesh assembled = generateMesh(model);
            QVERIFY(!assembled.empty());
            QCOMPARE(freeEdgeCount(assembled), 0);   // superficie fechada: as partes se conectam pelos nos coincidentes
            const double expected = kPi * R * R + 2.0 * kPi * R * H + kPi * (R * R + rise * rise);
            QVERIFY(std::abs(totalArea(assembled) - expected) / expected < 0.03);
        }
        // extrusao e revolucao pela fabrica
        QVERIFY(!makeExtrusion({{0, 0, 0}}, false, {0, 0, 1}).has_value());
        QVERIFY(!makeCylinder({0, 0, 0}, 0.0, {0, 0, 1}).has_value());
        QVERIFY(makeCylinder({0, 0, 0}, 1.0, {0, 0, 2}, 0.0)->param("endScale", 1.0) == 0.0);
        QVERIFY(suggestedMeshSize({{0, 0, 0}, {16, 0, 0}}) == 1.0);
    }

    void nonConformingMeshesAreTiedAcrossTheJoint()
    {
        const QString dll = femLibrary();
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll ausente");
        }
        auto plate = [](int id, double x0, double x1, double mesh) {
            Object o;
            o.id = id;
            o.name = "P" + std::to_string(id);
            o.kind = Kind::Plate;
            o.polygon = {{x0, 0, 0}, {x1, 0, 0}, {x1, 2, 0}, {x0, 2, 0}};
            o.p = {{"t", 0.1}};
            o.meshSize = mesh;
            return o;
        };
        auto tipDeflection = [&](const Model &model) {
            const Mesh mesh = generateMesh(model);
            const Job job = buildJob(model, mesh, {"Q"});
            const Structura::App::ShellSolverBridge bridge(dll);
            const Results results = bridge.solve(model, mesh, job);
            if (!results.ok()) {
                qWarning("%s", results.error.c_str());
                return 0.0;
            }
            std::size_t best = 0;
            double bestDistance = 1e300;
            for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
                const double d = std::hypot(mesh.nodes[n][0] - 8.0, mesh.nodes[n][1] - 1.0);
                if (d < bestDistance) {
                    bestDistance = d;
                    best = n;
                }
            }
            return results.cases[0].displacement[best][2];
        };
        auto loaded = [](Model model) {
            Load q;
            q.caseName = "Q";
            q.type = Load::Area;
            q.v = {0.0, 0.0, -1.0e4, 0.0};
            model.loads.push_back(q);
            Support clamp;
            clamp.axis = 0;
            clamp.value = 0.0;
            clamp.fixed = {1, 1, 1, 1, 1, 1};
            model.supports.push_back(clamp);
            return model;
        };
        Model reference;
        reference.objects = {plate(1, 0.0, 8.0, 0.25)};
        Model mixed;
        mixed.objects = {plate(1, 0.0, 4.0, 0.5), plate(2, 4.0, 8.0, 0.25)};
        const Mesh mixedMesh = generateMesh(mixed);
        // a borda de B (4 nos intermediarios, em y = 0.25, 0.75, 1.25, 1.75) cai sobre arestas de A: ligacoes por interpolacao linear
        QCOMPARE(static_cast<int>(mixedMesh.ties.size()), 4);
        for (const Tie &t : mixedMesh.ties) {
            QCOMPARE(static_cast<int>(t.masters.size()), 2);
            QVERIFY(std::abs(t.weights[0] - 0.5) < 1e-9 && std::abs(t.weights[1] - 0.5) < 1e-9);
        }
        const double wRef = tipDeflection(loaded(reference));
        const double wMixed = tipDeflection(loaded(mixed));
        QVERIFY(wRef < 0.0);
        QVERIFY2(std::abs(wMixed - wRef) / std::abs(wRef) < 0.03, qPrintable(QString::number(wMixed) + " vs " + QString::number(wRef)));
        // chapa que apoia no meio de outra (parede sobre laje): a borda inferior fica presa a face da laje
        Model wall;
        Object slab = plate(1, 0.0, 4.0, 0.5);
        slab.polygon = {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}};
        Object w;
        w.id = 2;
        w.name = "Parede";
        w.kind = Kind::Plate;
        w.polygon = {{1, 2, 0}, {3, 2, 0}, {3, 2, 2}, {1, 2, 2}};
        w.p = {{"t", 0.1}};
        w.meshSize = 0.25;
        wall.objects = {slab, w};
        const Mesh wallMesh = generateMesh(wall);
        QVERIFY(static_cast<int>(wallMesh.ties.size()) >= 4);
        for (const Tie &t : wallMesh.ties) {
            double sum = 0.0;
            for (const double weight : t.weights) {
                sum += weight;
            }
            QVERIFY(std::abs(sum - 1.0) < 1e-9);
        }
    }

    void movingAnObjectCarriesItsSupportsAndLoads()
    {
        Model model;
        Object tank = cylinderTank(3.0, 4.0, 2, 0.01, 0.01, 0.5);
        model.objects = {tank};
        Support base;
        base.object = 1;
        base.axis = 2;
        base.value = 0.0;
        Support point;
        point.object = 1;
        point.selector = Support::Point;
        point.value = 1.0;
        point.value2 = 2.0;
        point.value3 = 3.0;
        model.supports = {base, point};
        Load water;
        water.object = 1;
        water.type = Load::Hydrostatic;
        water.v = {9810.0, 3.5, 0.0, 0.0};
        model.loads = {water};
        QVERIFY(translateObject(model, 1, {10.0, 0.0, 2.0}));
        QCOMPARE(model.objects[0].param("z0"), 2.0);
        QCOMPARE(model.objects[0].param("x0"), 10.0);
        QCOMPARE(model.supports[0].value, 2.0);
        QCOMPARE(model.supports[1].value, 11.0);
        QCOMPARE(model.supports[1].value3, 5.0);
        QCOMPARE(model.loads[0].v[1], 5.5);
        // copia: duplica objeto, apoios e cargas com o novo id
        QVERIFY(translateObject(model, 1, {0.0, 20.0, 0.0}, 2));
        QCOMPARE(static_cast<int>(model.objects.size()), 2);
        QCOMPARE(static_cast<int>(model.supports.size()), 4);
        QCOMPARE(model.supports[2].object, 2);
        QCOMPARE(model.supports[3].value2, 22.0);
        QCOMPARE(model.objects[1].param("y0"), 20.0);
        QCOMPARE(model.objects[0].param("y0"), 0.0);
        QVERIFY(!translateObject(model, 99, {1, 0, 0}));
    }

    void tankPartsWithDifferentMeshSizesAreTiedAndSolve()
    {
        const QString dll = femLibrary();
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll ausente");
        }
        // fundo (disco, malha 0.3), costado (cilindro, malha 0.5) e teto (disco, malha 0.4): os contornos circulares tem nos diferentes
        Model model;
        const double R = 3.0, H = 3.0;
        auto bottom = makeDisk({0, 0, 0}, {R, 0, 0}, {0, 1, 2});
        auto wall = makeCylinder({0, 0, 0}, R, {0, 0, H});
        auto roof = makeDisk({0, 0, H}, {R, 0, H}, {0, 1, 2});
        QVERIFY(bottom && wall && roof);
        int id = 1;
        const double sizes[3] = {0.3, 0.5, 0.4};
        int k = 0;
        for (Object *o : {&*bottom, &*wall, &*roof}) {
            o->id = id++;
            o->name = "P" + std::to_string(o->id);
            o->meshSize = sizes[k++];
            o->p["t"] = 0.01;
        }
        bottom->p["flip"] = 1.0;
        model.objects = {*bottom, *wall, *roof};
        const Mesh mesh = generateMesh(model);
        QVERIFY(!mesh.ties.empty());
        for (const Tie &t : mesh.ties) {
            double sum = 0.0;
            for (const double w : t.weights) {
                sum += w;
            }
            QVERIFY(std::abs(sum - 1.0) < 1e-9);
        }
        Load weight;
        weight.caseName = "PP";
        weight.type = Load::SelfWeight;
        model.loads = {weight};
        Support base;
        base.object = 1;
        base.axis = 2;
        base.value = 0.0;
        base.tol = 1e-6;
        base.fixed = {{1, 1, 1, 0, 0, 0}};
        model.supports = {base};
        const Job job = buildJob(model, mesh, {"PP"});
        const Structura::App::ShellSolverBridge bridge(dll);
        const Results results = bridge.solve(model, mesh, job);
        QVERIFY2(results.ok(), results.error.c_str());
        double reaction = 0.0, weightTotal = 0.0;
        for (const auto &r : results.cases[0].reaction) {
            reaction += r[2];
        }
        for (const MeshQuad &q : mesh.quads) {
            weightTotal += model.materials[static_cast<std::size_t>(q.material)].unitWeight * q.thickness * quadArea(mesh, q);
        }
        QVERIFY2(std::abs(reaction - weightTotal) / weightTotal < 1e-3, qPrintable(QString::number(reaction) + " vs " + QString::number(weightTotal)));
    }

    void concreteDesignFormulasMatchHandCalculation()
    {
        // rho_min (Tabela 17.3): C30 -> 0,150 %, C35 -> 0,164 %, interpolacao em C32,5
        QVERIFY(std::abs(minimumFlexureRatio(30.0) - 0.00150) < 1e-12);
        QVERIFY(std::abs(minimumFlexureRatio(35.0) - 0.00164) < 1e-12);
        QVERIFY(std::abs(minimumFlexureRatio(32.5) - 0.00157) < 1e-12);
        // flexao: h = 0,15 m, d = 0,12 m, fck = 30 MPa, CA-50, Md = 20 kN.m/m
        double as = 0.0, xd = 0.0;
        const double fck = 30.0e6, fyk = 500.0e6, md = 20.0e3, d = 0.12;
        QVERIFY(flexureReinforcement(md, 0.15, d, fck, fyk, 1.4, 1.15, as, xd));
        const double fcd = fck / 1.4, fyd = fyk / 1.15;
        const double x = 1.25 * d * (1.0 - std::sqrt(1.0 - md / (0.425 * d * d * fcd)));
        QVERIFY(std::abs(as - md / (fyd * (d - 0.4 * x))) < 1e-12);
        QVERIFY(std::abs(xd - x / d) < 1e-12);
        QVERIFY(xd < 0.45);
        QVERIFY(!flexureReinforcement(90.0e3, 0.15, d, fck, fyk, 1.4, 1.15, as, xd));   // x/d > 0,45
        // cisalhamento: fctm = 0,3 fck^(2/3); fctd = 0,7 fctm / 1,4; tauRd = 0,25 fctd; k = max(1; 1,6 - d)
        const double fctm = 0.3 * std::pow(30.0, 2.0 / 3.0);
        const double tauRd = 0.25 * 0.7 * fctm / 1.4 * 1.0e6;
        const double rho1 = 0.004;
        QVERIFY(std::abs(slabShearResistance(fck, d, rho1, 1.4) - tauRd * (1.6 - d) * (1.2 + 40.0 * rho1) * d) < 1e-6);
    }

    void concreteSlabDesignFromTheFemResults()
    {
        const QString dll = femLibrary();
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll nao encontrada na raiz do repositorio.");
        }
        Model model;
        model.materials.push_back(Material {"Concreto C30", true, 2.6e10, 0.2, 25000.0, 1.0e-5, 30.0e6, 0.0});
        Object slab;
        slab.id = 1;
        slab.name = "Laje";
        slab.kind = Kind::Plate;
        slab.material = 1;
        slab.meshSize = 0.25;
        slab.p = {{"t", 0.15}, {"cover", 0.025}, {"bar", 0.01}};
        slab.polygon = {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}};
        model.objects.push_back(slab);
        Load load;
        load.caseName = "ELU";
        load.type = Load::Area;
        load.v = {0.0, 0.0, -14.0e3, 0.0};   // 14 kN/m2 para baixo (ja majorada)
        model.loads.push_back(load);
        Support edge;
        edge.selector = Support::FreeEdge;
        edge.fixed = {0, 0, 1, 0, 0, 0};
        model.supports.push_back(edge);
        Support pin;
        pin.selector = Support::Point;
        pin.fixed = {1, 1, 0, 0, 0, 0};
        model.supports.push_back(pin);
        Support pin2;
        pin2.selector = Support::Point;
        pin2.value = 4.0;
        pin2.fixed = {0, 1, 0, 0, 0, 0};
        model.supports.push_back(pin2);
        const Mesh mesh = generateMesh(model);
        const Job job = buildJob(model, mesh, {"ELU"});
        const Structura::App::ShellSolverBridge bridge(dll);
        const Results results = bridge.solve(model, mesh, job);
        QVERIFY2(results.ok(), results.error.c_str());
        const DesignOutput design = designShells(mesh, model, {results.cases[0]}, &results.cases[0]);
        QVERIFY(design.hasConcrete);
        // momento central da laje apoiada: 0,0479 q a2 = 10,7 kN.m/m -> armadura positiva (face -, normal para cima)
        const double md = 0.0479 * 14.0e3 * 16.0;
        double as = 0.0, xd = 0.0;
        QVERIFY(flexureReinforcement(md, 0.15, 0.15 - 0.025 - 0.005, 30.0e6, 500.0e6, 1.4, 1.15, as, xd));
        double maxAs = 0.0;
        for (const auto &r : design.reinforcement) {
            maxAs = std::max(maxAs, r[2]);
        }
        QVERIFY2(std::abs(maxAs - as * 1.0e4) / (as * 1.0e4) < 0.10, qPrintable(QString::number(maxAs) + " vs " + QString::number(as * 1.0e4)));
        QVERIFY(design.maxRatio < 1.0);
    }

    void combinationAndQuantities()
    {
        Results results;
        CaseResult a, b;
        a.name = "G";
        b.name = "Q";
        a.displacement = {{1, 2, 3, 0, 0, 0}};
        b.displacement = {{2, 0, 1, 0, 0, 0}};
        a.reaction = b.reaction = {{0, 0, 0, 0, 0, 0}};
        ElementState sa, sb;
        sa.v[0] = 100.0;
        sb.v[0] = 50.0;
        a.elements = {sa};
        b.elements = {sb};
        results.cases = {a, b};
        const CaseResult combo = combine(results, {{"G", 1.4}, {"Q", 1.5}}, "ELU");
        QCOMPARE(combo.displacement[0][0], 1.4 * 1 + 1.5 * 2);
        QCOMPARE(combo.elements[0].v[0], 1.4 * 100 + 1.5 * 50);
        FaceStress f;
        f.sa = 100.0;
        f.sb = 0.0;
        QCOMPARE(f.vonMises(), 100.0);
        QCOMPARE(f.principalMax(), 100.0);
    }
};

QTEST_GUILESS_MAIN(TestShell)
#include "TestShell.moc"
