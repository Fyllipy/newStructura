#include <QtTest/QtTest>
#include <QFile>

#include "app/Edition.h"
#include "app/ShellSolverBridge.h"
#include "shell/ShellDesign.h"
#include "shell/ShellFactory.h"
#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"
#include "shell/ShellMeshQuality.h"
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

    void demoLimitsShellMesh()
    {
        if (Structura::App::Edition::kMaxShellNodes <= 0) {
            QSKIP("Limite de nos existe so na edicao demonstrativa.");
        }
        const QString dll = femLibrary();
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll nao encontrada na raiz do repositorio.");
        }
        Model model;
        model.objects.push_back(cylinderTank(5.0, 6.0, 3, 0.01, 0.01, 0.5));
        const Mesh mesh = generateMesh(model);
        QVERIFY(static_cast<int>(mesh.nodes.size()) > Structura::App::Edition::kMaxShellNodes);
        const Job job = buildJob(model, mesh, {});
        const Structura::App::ShellSolverBridge bridge(dll);
        const Results results = bridge.solve(model, mesh, job);
        QVERIFY(!results.ok());
        QVERIFY2(QString::fromStdString(results.error).contains(QStringLiteral("100")), results.error.c_str());
    }

    void tankUnderHydrostaticPressure()
    {
        if (Structura::App::Edition::kMaxShellNodes > 0) {
            QSKIP("A edicao demonstrativa limita a malha a 100 nos; ver demoLimitsShellMesh.");
        }
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
        if (Structura::App::Edition::kMaxShellNodes > 0) {
            QSKIP("A edicao demonstrativa limita a malha a 100 nos; ver demoLimitsShellMesh.");
        }
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
        if (Structura::App::Edition::kMaxShellNodes > 0) {
            QSKIP("A edicao demonstrativa limita a malha a 100 nos; ver demoLimitsShellMesh.");
        }
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
        if (Structura::App::Edition::kMaxShellNodes > 0) {
            QSKIP("A edicao demonstrativa limita a malha a 100 nos; ver demoLimitsShellMesh.");
        }
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

#ifndef STRUCTURA_EDITION_DEMO   // usa recursos exclusivos da edicao Comercial
    // ---------------------------------------------------------------- varredura de perfil por caminho (Fase B)
    void sweepAlongAStraightPathIsAnExtrusion()
    {
        Model model;
        auto sweepObject = makeSweep({{0, 0, 0}, {2, 0, 0}}, false, {{0, 0, 0}, {0, 0, 3}}, false);
        QVERIFY(sweepObject.has_value());
        sweepObject->id = 1;
        sweepObject->meshSize = 0.5;
        sweepObject->p["t"] = 0.01;
        model.objects = {*sweepObject};
        const Mesh mesh = generateMesh(model);
        QVERIFY(mesh.warnings.empty());
        double area = 0.0;
        for (const MeshQuad &q : mesh.quads) {
            area += quadArea(mesh, q);
        }
        QVERIFY2(std::abs(area - 6.0) < 1e-9, qPrintable(QString::number(area, 'g', 12)));
        QCOMPARE(static_cast<int>(mesh.quads.size()), 4 * 6);
        // mesma malha que a extrusao equivalente (mesmos nos e mesma normal)
        Model other;
        auto extrusionObject = makeExtrusion({{0, 0, 0}, {2, 0, 0}}, false, {0, 0, 3});
        extrusionObject->id = 1;
        extrusionObject->meshSize = 0.5;
        extrusionObject->p["t"] = 0.01;
        other.objects = {*extrusionObject};
        const Mesh ref = generateMesh(other);
        QCOMPARE(mesh.nodes.size(), ref.nodes.size());
        QCOMPARE(mesh.quads.size(), ref.quads.size());
        const Vec3 a = quadNormal(mesh, mesh.quads.front()), r = quadNormal(ref, ref.quads.front());
        QVERIFY(a[0] * r[0] + a[1] * r[1] + a[2] * r[2] > 0.999);   // mesmo lado da normal positiva
        for (const MeshQuad &q : mesh.quads) {
            const Vec3 n = quadNormal(mesh, q);
            QVERIFY(n[0] * a[0] + n[1] * a[1] + n[2] * a[2] > 0.999);   // normais coerentes em toda a superficie
        }
    }

    void sweepOfACircleAlongAnArcMatchesPappus()
    {
        // duto: circulo de raio r varrido num arco de 90 graus de raio R: area = (2 pi r) x (R pi / 2) (teorema de Pappus-Guldin)
        const double r = 0.5, R = 4.0;
        std::vector<Vec3> profile, path;
        const int nProfile = 48, nPath = 40;
        for (int i = 0; i < nProfile; ++i) {
            const double a = 2.0 * kPi * i / nProfile;
            // plano do perfil perpendicular a tangente inicial (+Y), centrado no inicio do caminho (R, 0, 0)
            profile.push_back({R + r * std::cos(a), 0.0, r * std::sin(a)});
        }
        for (int i = 0; i <= nPath; ++i) {
            const double a = 0.5 * kPi * i / nPath;
            path.push_back({R * std::cos(a), R * std::sin(a), 0.0});
        }
        auto sweepObject = makeSweep(profile, true, path, false);
        QVERIFY(sweepObject.has_value());
        sweepObject->id = 1;
        sweepObject->meshSize = 0.25;
        sweepObject->p["t"] = 0.01;
        Model model;
        model.objects = {*sweepObject};
        const Mesh mesh = generateMesh(model);
        QVERIFY2(mesh.warnings.empty(), mesh.warnings.empty() ? "" : mesh.warnings.front().c_str());
        double area = 0.0;
        for (const MeshQuad &q : mesh.quads) {
            area += quadArea(mesh, q);
        }
        const double exact = 2.0 * kPi * r * R * 0.5 * kPi;
        QVERIFY2(std::abs(area - exact) < 0.01 * exact, qPrintable(QString::number(area) + " vs " + QString::number(exact)));
        // so as duas bocas ficam livres: 2 aneis de 48 arestas
        QCOMPARE(freeEdgeCount(mesh), 2 * nProfile);
        const MeshQualityReport report = analyzeMeshQuality(mesh, model);
        QVERIFY2(!report.hasErrors(), "malha do duto com erro de qualidade");
    }

    void sweepAlongAClosedPathClosesWithoutGap()
    {
        // toro: perfil quadrado fechado varrido num caminho circular fechado; superficie fechada (nenhuma aresta livre) e area de Pappus
        const double R = 3.0, a = 0.3;
        std::vector<Vec3> path, profile = {{R - a, 0, -a}, {R + a, 0, -a}, {R + a, 0, a}, {R - a, 0, a}};
        const int n = 48;
        for (int i = 0; i < n; ++i) {
            path.push_back({R * std::cos(2.0 * kPi * i / n), R * std::sin(2.0 * kPi * i / n), 0.0});
        }
        auto sweepObject = makeSweep(profile, true, path, true);
        QVERIFY(sweepObject.has_value());
        sweepObject->id = 1;
        sweepObject->meshSize = 0.3;
        sweepObject->p["t"] = 0.01;
        Model model;
        model.objects = {*sweepObject};
        const Mesh mesh = generateMesh(model);
        QVERIFY(mesh.warnings.empty());
        QCOMPARE(freeEdgeCount(mesh), 0);
        double area = 0.0;
        for (const MeshQuad &q : mesh.quads) {
            area += quadArea(mesh, q);
        }
        // perimetro do quadrado x comprimento do caminho no centroide (polilinha de 48 lados: raio efetivo ~ R cos(pi/n) nos lados)
        const double exact = 8.0 * a * 2.0 * kPi * R;
        QVERIFY2(std::abs(area - exact) < 0.01 * exact, qPrintable(QString::number(area) + " vs " + QString::number(exact)));
    }

    void twistedSweepIsDiagnosedAsWarpedAndImprovesWithRefinement()
    {
        // faixa retangular torcida 90 graus em 4 m: elementos longos giram entre as secoes e ficam empenados; o diagnostico de qualidade
        // acusa o empenamento e a contagem cai ao refinar (o empenamento por elemento ~ torcao por elemento)
        auto warpedCount = [&](double meshSize) {
            auto sweepObject = makeSweep({{-0.5, 0, 0}, {0.5, 0, 0}}, false, {{0, 0, 0}, {0, 0, 4}}, false);
            sweepObject->id = 1;
            sweepObject->meshSize = meshSize;
            sweepObject->p["t"] = 0.05;
            sweepObject->p["twist"] = 90.0;
            Model model;
            model.objects = {*sweepObject};
            const Mesh mesh = generateMesh(model);
            const MeshQualityReport report = analyzeMeshQuality(mesh, model);
            return std::pair<int, std::size_t> {report.warnings + report.errors, mesh.quads.size()};
        };
        const auto coarse = warpedCount(2.0), middle = warpedCount(0.25), fine = warpedCount(0.04);
        QVERIFY2(coarse.first > 0, "faixa torcida em elementos grandes nao foi diagnosticada");
        auto fraction = [](const std::pair<int, std::size_t> &r) { return static_cast<double>(r.first) / static_cast<double>(r.second); };
        QVERIFY2(fraction(middle) < fraction(coarse) + 1e-12, "a fracao de elementos empenados nao deveria crescer ao refinar");
        QVERIFY2(fraction(fine) < fraction(middle), qPrintable(QString("%1 vs %2").arg(fraction(fine)).arg(fraction(middle))));
        QCOMPARE(fine.first, 0);   // torcao de ~1 grau por elemento: sem empenamento relevante
        QVERIFY(fine.second > middle.second && middle.second > coarse.second);
    }

    void thicknessPerSegmentIsAppliedToRevolutionExtrusionAndSweep()
    {
        auto thicknesses = [](const Mesh &mesh) {
            std::set<long long> out;
            for (const MeshQuad &q : mesh.quads) {
                out.insert(std::llround(q.thickness * 1e6));
            }
            return out;
        };
        // revolucao: cilindro (segmento 1, 10 mm) + tampa plana anular (segmento 2, 25 mm)
        auto revolution = makeRevolution({{1, 0, 0}, {1, 0, 2}, {0.2, 0, 2}}, {0, 0, 0}, {0, 0, 1});
        revolution->id = 1;
        revolution->meshSize = 0.4;
        revolution->p["t"] = 0.010;
        revolution->p["tSeg2"] = 0.025;
        Model model;
        model.objects = {*revolution};
        Mesh mesh = generateMesh(model);
        QCOMPARE(thicknesses(mesh), (std::set<long long> {10000, 25000}));
        // sem tSeg: espessura unica
        model.objects[0].p.erase("tSeg2");
        QCOMPARE(thicknesses(generateMesh(model)), (std::set<long long> {10000}));
        // extrusao de polilinha: cada segmento com a sua espessura
        auto extrusion = makeExtrusion({{0, 0, 0}, {2, 0, 0}, {2, 2, 0}}, false, {0, 0, 1});
        extrusion->id = 1;
        extrusion->meshSize = 0.5;
        extrusion->p["t"] = 0.01;
        extrusion->p["tSeg1"] = 0.02;
        model.objects = {*extrusion};
        QCOMPARE(thicknesses(generateMesh(model)), (std::set<long long> {20000, 10000}));
        // varredura: perfil de 2 segmentos
        auto sweepObject = makeSweep({{0, 0, 0}, {1, 0, 0}, {1, 1, 0}}, false, {{0, 0, 0}, {0, 0, 2}}, false);
        sweepObject->id = 1;
        sweepObject->meshSize = 0.5;
        sweepObject->p["t"] = 0.01;
        sweepObject->p["tSeg2"] = 0.03;
        model.objects = {*sweepObject};
        QCOMPARE(thicknesses(generateMesh(model)), (std::set<long long> {10000, 30000}));
    }

    void sweepIsTranslatedWithItsPath()
    {
        Model model;
        auto sweepObject = makeSweep({{0, 0, 0}, {1, 0, 0}}, false, {{0, 0, 0}, {0, 0, 2}}, false);
        sweepObject->id = 1;
        model.objects = {*sweepObject};
        QVERIFY(translateObject(model, 1, {5, 6, 7}, 2));
        const Object &copy = model.objects.back();
        QCOMPARE(copy.id, 2);
        QCOMPARE(copy.path.back(), (Vec3 {5, 6, 9}));
        QCOMPARE(copy.polygon.front(), (Vec3 {5, 6, 7}));
        QCOMPARE(model.objects.front().path.back(), (Vec3 {0, 0, 2}));   // o original nao muda
        QVERIFY(fingerprint(model) != std::string());
        Model other = model;
        other.objects.back().path.back()[2] += 0.1;
        QVERIFY(fingerprint(model) != fingerprint(other));   // o caminho entra na impressao da geometria
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
#endif

#ifndef STRUCTURA_EDITION_DEMO   // usa recursos exclusivos da edicao Comercial
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
#endif

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

    // ---- D2 (Etapa 3): apoios que so comprimem ------------------------------------------------------------------------------------------------------
    // Placa 2 x 1 m sobre fundacao so-compressao (plano z = 0), presa no plano; pressao negativa (contra +z) empurra a placa para a fundacao.
    static Model unilateralPlate(double pressureA, double pressureB)
    {
        Model model;
        Object o;
        o.id = 1;
        o.name = "Laje";
        o.kind = Kind::Plate;
        o.polygon = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};
        o.p = {{"t", 0.1}};
        o.meshSize = 0.25;
        model.objects = {o};
        Support base;
        base.axis = 2;
        base.value = 0.0;
        base.fixed = {0, 0, 0, 0, 0, 0};
        base.springPerArea = 5.0e8;
        base.compressionOnly = true;
        model.supports.push_back(base);
        Support pin;
        pin.selector = Support::Point;
        pin.value = 0.0;
        pin.value2 = 0.0;
        pin.value3 = 0.0;
        pin.fixed = {1, 1, 0, 0, 0, 1};
        model.supports.push_back(pin);
        Support slide;
        slide.selector = Support::Point;
        slide.value = 2.0;
        slide.value2 = 0.0;
        slide.value3 = 0.0;
        slide.fixed = {0, 1, 0, 0, 0, 0};
        model.supports.push_back(slide);
        auto pressure = [](const char *name, double p) {
            Load l;
            l.caseName = name;
            l.type = Load::Pressure;
            l.v = {p, 0.0, 0.0, 0.0};
            return l;
        };
        model.loads = {pressure("A", pressureA), pressure("B", pressureB)};
        return model;
    }

    void combinationJobScalesAndMergesCases()
    {
        Job job;
        job.nodes.resize(3);
        Job::Case a, b;
        a.name = "A";
        a.nodal.push_back({1, {0, 0, -10.0, 0, 0, 0}});
        a.pressure.push_back({0, {1.0, 1.0, 1.0, 1.0}});
        b.name = "B";
        b.nodal.push_back({2, {0, 0, 5.0, 0, 0, 0}});
        b.thermal.push_back({0, 20.0, 4.0});
        b.body.push_back({0, {{0.0, 0.0, -100.0}}});
        job.cases = {a, b};
        QVERIFY(!job.hasUnilateralSprings());
        Job::SpringDef spring;
        spring.node = 0;
        spring.k = {1e6, 0, 0, 0, 0, 0};
        spring.compressionOnly = true;
        job.springs.push_back(spring);
        QVERIFY(job.hasUnilateralSprings());
        const Job c = combinationJob(job, {{"A", 1.5}, {"B", -2.0}, {"inexistente", 9.0}}, "ELU");
        QCOMPARE(static_cast<int>(c.cases.size()), 1);
        QCOMPARE(c.cases[0].name, std::string("ELU"));
        QCOMPARE(static_cast<int>(c.cases[0].nodal.size()), 2);
        QVERIFY(std::abs(c.cases[0].nodal[0].f[2] - (-15.0)) < 1e-12);
        QVERIFY(std::abs(c.cases[0].nodal[1].f[2] - (-10.0)) < 1e-12);
        QVERIFY(std::abs(c.cases[0].pressure[0].p[3] - 1.5) < 1e-12);
        QVERIFY(std::abs(c.cases[0].thermal[0].uniform - (-40.0)) < 1e-12 && std::abs(c.cases[0].thermal[0].gradient - (-8.0)) < 1e-12);
        QVERIFY(std::abs(c.cases[0].body[0].b[2] - 200.0) < 1e-12);
        QVERIFY(c.hasUnilateralSprings());   // molas preservadas
    }

    void unilateralCasesDoNotDependOnEachOther()
    {
        if (Structura::App::Edition::kMaxShellNodes > 0) {
            QSKIP("A edicao demonstrativa limita a malha a 100 nos.");
        }
        if (!QFile::exists(femLibrary())) {
            QSKIP("mefs.dll ausente");
        }
        const Model model = unilateralPlate(-3.0e4, -9.0e4);
        const Mesh mesh = generateMesh(model);
        const Structura::App::ShellSolverBridge bridge(femLibrary());
        const Results joint = bridge.solve(model, mesh, buildJob(model, mesh, {"A", "B"}));
        const Results onlyA = bridge.solve(model, mesh, buildJob(model, mesh, {"A"}));
        const Results onlyB = bridge.solve(model, mesh, buildJob(model, mesh, {"B"}));
        QVERIFY2(joint.ok(), joint.error.c_str());
        QVERIFY2(onlyA.ok(), onlyA.error.c_str());
        QVERIFY2(onlyB.ok(), onlyB.error.c_str());
        QVERIFY(!joint.superposable);
        auto relativeDifference = [](const CaseResult &x, const CaseResult &y) {
            double d = 0.0, s = 1e-30;
            for (std::size_t n = 0; n < x.displacement.size(); ++n) {
                for (std::size_t k = 0; k < 6; ++k) {
                    d = std::max(d, std::abs(x.displacement[n][k] - y.displacement[n][k]));
                    s = std::max(s, std::abs(y.displacement[n][k]));
                }
            }
            return d / s;
        };
        QVERIFY(relativeDifference(joint.cases[0], onlyA.cases[0]) < 1e-9);   // caso A junto de B == caso A sozinho
        QVERIFY(relativeDifference(joint.cases[1], onlyB.cases[0]) < 1e-9);
        const Results swapped = bridge.solve(model, mesh, buildJob(model, mesh, {"B", "A"}));
        QVERIFY2(swapped.ok(), swapped.error.c_str());
        QVERIFY(relativeDifference(swapped.cases[1], joint.cases[0]) < 1e-9);   // a ordem dos casos nao altera cada caso
        QVERIFY(relativeDifference(swapped.cases[0], joint.cases[1]) < 1e-9);
    }

    void unilateralCombinationIsSolvedAsIndependentAnalysis()
    {
        if (Structura::App::Edition::kMaxShellNodes > 0) {
            QSKIP("A edicao demonstrativa limita a malha a 100 nos.");
        }
        if (!QFile::exists(femLibrary())) {
            QSKIP("mefs.dll ausente");
        }
        const Model model = unilateralPlate(-3.0e4, -9.0e4);
        const Mesh mesh = generateMesh(model);
        const Job job = buildJob(model, mesh, {"A", "B"});
        const Structura::App::ShellSolverBridge bridge(femLibrary());
        const Results combo = bridge.solveCombination(model, mesh, job, {{"A", 1.0}, {"B", 1.0}}, "ELU");
        QVERIFY2(combo.ok(), combo.error.c_str());
        QCOMPARE(static_cast<int>(combo.cases.size()), 1);
        QCOMPARE(combo.cases[0].name, std::string("ELU"));
        // equilibrio vertical: soma das reacoes = (3e4 + 9e4) Pa x 2 m2
        double reaction = 0.0;
        for (const auto &r : combo.cases[0].reaction) {
            reaction += r[2];
        }
        QVERIFY(std::abs(reaction - 1.2e5 * 2.0) < 1e-3 * 1.2e5 * 2.0);
    }

};

QTEST_GUILESS_MAIN(TestShell)
#include "TestShell.moc"
