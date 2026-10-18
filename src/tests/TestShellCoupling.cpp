#include <QtTest/QtTest>

#include "app/SolverBridge.h"
#include "app/services/CombinationService.h"
#include "app/services/ShellCouplingService.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"

#include <cmath>
#include <map>

using namespace Structura;
using Structura::App::SolverBridge;
using Structura::App::Services::ShellCouplingService;

namespace {

QString solverLibrary()
{
    return QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
}

constexpr double kE = 2.0e11;   // Pa (modelo em N e m)
constexpr double kNu = 0.3;

SolverBridge::ModelData baseModel()
{
    SolverBridge::ModelData model;
    SolverBridge::MaterialData material;
    material.id = 1;
    material.e = kE;
    material.g = kE / (2.0 * (1.0 + kNu));
    model.materials.push_back(material);
    SolverBridge::SectionData section;
    section.id = 1;
    section.area = 1.0e-2;
    section.iy = 2.0e-5;
    section.iz = 2.0e-5;
    section.j = 1.0e-5;
    model.sections.push_back(section);
    return model;
}

SolverBridge::NodeData node(int id, double x, double y, double z, bool fixed = false)
{
    SolverBridge::NodeData n;
    n.id = id;
    n.x = x;
    n.y = y;
    n.z = z;
    if (fixed) {
        n.restraints = {{1, 1, 1, 1, 1, 1}};
    }
    return n;
}

SolverBridge::MemberData member(int id, int a, int b)
{
    SolverBridge::MemberData m;
    m.id = id;
    m.nodeI = a;
    m.nodeJ = b;
    m.materialId = 1;
    m.sectionId = 1;
    return m;
}

Shell::Object plate(int id, const std::vector<Shell::Vec3> &polygon, double t, double mesh)
{
    Shell::Object o;
    o.id = id;
    o.name = "Chapa" + std::to_string(id);
    o.kind = Shell::Kind::Plate;
    o.polygon = polygon;
    o.p = {{"t", t}};
    o.meshSize = mesh;
    return o;
}

} // namespace

class TestShellCoupling : public QObject
{
    Q_OBJECT

private slots:
    void barsAloneAreUnchangedByTheShellExtension()
    {
        // balanco de barra: flecha PL^3/(3EI)
        SolverBridge::ModelData model = baseModel();
        model.nodes = {node(1, 0, 0, 0, true), node(2, 4, 0, 0)};
        model.members = {member(1, 1, 2)};
        SolverBridge::NodalLoadData load;
        load.nodeId = 2;
        load.values = {{0, 0, -1.0e4, 0, 0, 0}};
        model.nodalLoads.push_back(load);
        const SolverBridge::Result r = SolverBridge(solverLibrary()).solve(model);
        const double expected = -1.0e4 * 64.0 / (3.0 * kE * 2.0e-5);
        QVERIFY(std::abs(r.nodeResults[1].displacements[2] - expected) / std::abs(expected) < 1e-9);
        QVERIFY(r.shellNodeResults.empty() && r.shellResults.empty());
    }

    void barAndPlateShareNodesAndStiffness()
    {
        // viga em balanco (barra) ao longo da borda de uma chapa em balanco: rigidez somada (mesmos nos e mesma linha neutra)
        const double L = 4.0, W = 0.5, t = 0.1, P = 1.0e4;
        SolverBridge::ModelData model = baseModel();
        const int n = 8;   // nos da barra a cada 0.5 m (coincidem com a malha da chapa)
        for (int i = 0; i <= n; ++i) {
            model.nodes.push_back(node(i + 1, L * i / n, 0.0, 0.0, i == 0));
        }
        for (int i = 0; i < n; ++i) {
            model.members.push_back(member(i + 1, i + 1, i + 2));
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {L, 0, 0}, {L, W, 0}, {0, W, 0}}, t, 0.5)};
        Shell::Support clamp;
        clamp.axis = 0;
        clamp.value = 0.0;
        clamp.fixed = {{1, 1, 1, 1, 1, 1}};
        shell.supports = {clamp};
        shell.coupled = true;
        // carga na ponta da barra (no da barra = no da malha)
        SolverBridge::NodalLoadData load;
        load.nodeId = n + 1;
        load.values = {{0, 0, -P, 0, 0, 0}};
        model.nodalLoads.push_back(load);
        SolverBridge::ModelData coupled = model;
        const auto mapping = ShellCouplingService::attach(coupled, shell, "Q", 1.0);
        QVERIFY(mapping != nullptr);
        QCOMPARE(mapping->mergedNodes, n + 1);
        QVERIFY(coupled.shell && !coupled.shell->empty());
        const SolverBridge solver(solverLibrary());
        const SolverBridge::Result alone = solver.solve(model);
        const SolverBridge::Result both = solver.solve(coupled);
        QVERIFY(!both.shellResults.empty());
        QVERIFY(!both.shellNodeResults.empty());
        const double wBar = alone.nodeResults[static_cast<std::size_t>(n)].displacements[2];
        const double wBoth = both.nodeResults[static_cast<std::size_t>(n)].displacements[2];
        const double iBar = 2.0e-5, iPlate = W * t * t * t / 12.0;   // faixa estreita com bordas livres: EI = E W t^3 / 12
        // a flecha acoplada aproxima PL^3/3(EI_barra + EI_chapa)
        const double expected = -P * L * L * L / (3.0 * kE * (iBar + iPlate));
        QVERIFY2(std::abs(wBoth - expected) / std::abs(expected) < 0.06, qPrintable(QString::number(wBoth) + " vs " + QString::number(expected)));
        QVERIFY(std::abs(wBoth) < std::abs(wBar));   // a chapa enrijece
        // resultados da casca na malha: reacao total = P
        double reaction = 0.0;
        for (const auto &r : both.nodeResults) {
            reaction += r.reactions[2];
        }
        for (const auto &r : both.shellNodeResults) {
            reaction += r.reactions[2];
        }
        QVERIFY(std::abs(reaction - P) / P < 1e-6);
        const auto result = ShellCouplingService::caseResult(*mapping, both, "Q");
        QCOMPARE(result.displacement.size(), mapping->mesh.nodes.size());
        QCOMPARE(result.elements.size(), mapping->mesh.quads.size());
    }

    void columnTiedToThePlateInterior()
    {
        // pilar em um ponto interno de um elemento de chapa (no de barra nao coincide com no da malha): ligacao por interpolacao
        const double a = 2.0, t = 0.1, q = 1.0e4;
        SolverBridge::ModelData model = baseModel();
        model.nodes = {node(1, 0.25, 0.25, -1.0, true), node(2, 0.25, 0.25, 0.0)};
        model.members = {member(1, 1, 2)};
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {a, 0, 0}, {a, a, 0}, {0, a, 0}}, t, 0.5)};
        Shell::Load pressure;
        pressure.caseName = "Q";
        pressure.type = Shell::Load::Pressure;
        pressure.v = {-q, 0, 0, 0};   // contra a normal (+z): para baixo
        shell.loads = {pressure};
        shell.coupled = true;
        const auto mapping = ShellCouplingService::attach(model, shell, "Q", 1.0);
        QVERIFY(mapping != nullptr);
        QCOMPARE(mapping->mergedNodes, 0);
        QCOMPARE(static_cast<int>(mapping->tiedBarNodes.size()), 1);   // o no 2 fica preso ao elemento
        SolverBridge::Result r;
        try {
            r = SolverBridge(solverLibrary()).solve(model);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
        // o pilar recebe toda a carga da chapa: N = q a^2 (compressao)
        const auto &forces = r.memberResults[0].localForces;
        QVERIFY2(std::abs(std::abs(forces[6]) - q * a * a) / (q * a * a) < 1e-3, qPrintable(QString::number(forces[6])));
        // a chapa deflete para baixo nas bordas livres em relacao ao pilar
        double minW = 0.0;
        for (const auto &nr : r.shellNodeResults) {
            minW = std::min(minW, nr.displacements[2]);
        }
        QVERIFY(minW < -1e-6);
    }

    void splittingPointsAreFoundAlongBars()
    {
        // barra ao longo da borda de uma chapa: nos da malha sobre o eixo (fora das extremidades) pedem a divisao da barra
        SolverBridge::ModelData model = baseModel();
        model.nodes = {node(1, 0, 0, 0, true), node(2, 2, 0, 0)};
        model.members = {member(1, 1, 2)};
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}}, 0.1, 0.5)};
        const Shell::Mesh mesh = Shell::generateMesh(shell);
        const auto cuts = ShellCouplingService::barCuts(mesh, model);
        QCOMPARE(static_cast<int>(cuts.size()), 1);
        QCOMPARE(static_cast<int>(cuts[0].parameters.size()), 3);   // x = 0.5, 1.0, 1.5
        QVERIFY(std::abs(cuts[0].parameters[0] - 0.25) < 1e-9);
        shell.coupled = true;
        const auto mapping = ShellCouplingService::mapping(shell, model, 1.0);
        QCOMPARE(mapping->interiorShellNodes, 3);
        QVERIFY(!mapping->warnings.empty());
    }

    void beamWithRigidOffsetBelowThePlateActsCompositely()
    {
        // barra no eixo da faixa, com o eixo 10 cm abaixo do no (que esta no plano medio da chapa): secao composta pelo deslocamento de secao
        const double L = 4.0, W = 0.5, t = 0.1, P = 1.0e4, e = 0.1;
        const int n = 16;
        SolverBridge::ModelData model = baseModel();
        for (int i = 0; i <= n; ++i) {
            model.nodes.push_back(node(i + 1, L * i / n, W / 2.0, 0.0, i == 0));
        }
        for (int i = 0; i < n; ++i) {
            SolverBridge::MemberData m = member(i + 1, i + 1, i + 2);
            m.offsetY1 = m.offsetY2 = -e;   // y local da barra horizontal = Z global
            model.members.push_back(m);
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {L, 0, 0}, {L, W, 0}, {0, W, 0}}, t, 0.25)};
        Shell::Support clamp;
        clamp.axis = 0;
        clamp.value = 0.0;
        clamp.fixed = {{1, 1, 1, 1, 1, 1}};
        shell.supports = {clamp};
        shell.coupled = true;
        SolverBridge::NodalLoadData load;
        load.nodeId = n + 1;
        load.values = {{0, 0, -P, 0, 0, 0}};
        model.nodalLoads.push_back(load);
        ShellCouplingService::attach(model, shell, "Q", 1.0);
        const auto r = SolverBridge(solverLibrary()).solve(model);
        const double ap = W * t, ab = 1.0e-2, ip = W * t * t * t / 12.0, ib = 2.0e-5;
        const double zn = -ab * e / (ap + ab);
        const double ei = kE * (ip + ap * zn * zn + ib + ab * (-e - zn) * (-e - zn));
        const double expected = -P * L * L * L / (3.0 * ei);
        const double w = r.nodeResults[static_cast<std::size_t>(n)].displacements[2];
        QVERIFY2(std::abs(w - expected) / std::abs(expected) < 0.03, qPrintable(QString::number(w) + " vs " + QString::number(expected)));
    }

    void slabLoadReachesTheSupportingBeams()
    {
        // laje 4 x 4 m sobre quatro vigas de borda apoiadas nos cantos: a carga da laje chega as vigas (quinhoes), que fletem
        const double a = 4.0, t = 0.12, q = 1.0e4;
        const int n = 8;   // 0.5 m
        SolverBridge::ModelData model = baseModel();
        model.sections[0].area = 0.05;   // vigas rigidas em relacao a laje: a laje trabalha apoiada nas vigas
        model.sections[0].iy = model.sections[0].iz = 3.0e-3;
        int nextId = 1;
        std::map<std::pair<int, int>, int> ids;
        auto at = [&](int i, int j) {
            const auto key = std::make_pair(i, j);
            if (!ids.count(key)) {
                const bool corner = (i == 0 || i == n) && (j == 0 || j == n);
                ids[key] = nextId;
                SolverBridge::NodeData nd = node(nextId++, a * i / n, a * j / n, 0.0);
                if (corner) {
                    nd.restraints = {{1, 1, 1, 0, 0, 0}};
                }
                model.nodes.push_back(nd);
            }
            return ids[key];
        };
        int memberId = 1;
        for (int k = 0; k < n; ++k) {
            model.members.push_back(member(memberId++, at(k, 0), at(k + 1, 0)));
            model.members.push_back(member(memberId++, at(k, n), at(k + 1, n)));
            model.members.push_back(member(memberId++, at(0, k), at(0, k + 1)));
            model.members.push_back(member(memberId++, at(n, k), at(n, k + 1)));
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {a, 0, 0}, {a, a, 0}, {0, a, 0}}, t, 0.5)};
        shell.objects[0].material = 0;
        Shell::Load pressure;
        pressure.caseName = "Q";
        pressure.type = Shell::Load::Pressure;
        pressure.v = {-q, 0, 0, 0};
        shell.loads = {pressure};
        shell.coupled = true;
        const auto mapping = ShellCouplingService::attach(model, shell, "Q", 1.0);
        QVERIFY(mapping != nullptr);
        QCOMPARE(mapping->mergedNodes, 4 * n);   // os nos de borda da malha sao os nos das vigas
        const auto r = SolverBridge(solverLibrary()).solve(model);
        // flecha no meio de uma viga e momento fletor nela: a viga recebe carga da laje
        double midMoment = 0.0;
        for (const auto &m : r.memberResults) {
            midMoment = std::max(midMoment, std::abs(m.localForces[5]) + std::abs(m.localForces[4]));
            midMoment = std::max(midMoment, std::abs(m.localForces[11]) + std::abs(m.localForces[10]));
        }
        const double triangular = q * a * a * a / 24.0;   // viga carregada por quinhao triangular (pico q a / 2)
        QVERIFY2(midMoment > 0.5 * triangular && midMoment < 1.2 * triangular, qPrintable(QString::number(midMoment) + " vs " + QString::number(triangular)));
        // equilibrio: reacoes nos cantos = carga total
        double reaction = 0.0;
        for (const auto &nr : r.nodeResults) {
            reaction += nr.reactions[2];
        }
        QVERIFY(std::abs(reaction - q * a * a) / (q * a * a) < 1e-6);
        // forca cortante total absorvida pelas vigas = reacoes (a carga passa pela viga): soma das forcas verticais nas barras que chegam aos cantos
        qWarning("momento maximo nas vigas %g (quinhao triangular %g)", midMoment, triangular);
    }

    void plateBucklingUsesTheShellGeometricStiffness()
    {
        // placa quadrada simplesmente apoiada sob compressao uniforme: Ncr = 4 pi^2 D / a^2 por unidade de largura
        const double a = 1.0, t = 0.01, P = 1.0e5;
        const int n = 8;
        SolverBridge::ModelData model = baseModel();
        auto part = std::make_shared<SolverBridge::ShellPart>();
        auto id = [n](int i, int j) { return 1 + j * (n + 1) + i; };
        for (int j = 0; j <= n; ++j) {
            for (int i = 0; i <= n; ++i) {
                SolverBridge::ShellNodeData node;
                node.id = id(i, j);
                node.x = a * i / n;
                node.y = a * j / n;
                node.z = 0.0;
                if (i == 0 || i == n || j == 0 || j == n) {
                    node.restraints[2] = 1;
                }
                if (i == 0) {
                    node.restraints[0] = 1;
                }
                if (i == 0 && j == 0) {
                    node.restraints[1] = 1;
                }
                part->nodes.push_back(node);
            }
        }
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) {
                SolverBridge::ShellElementData e;
                e.id = j * n + i + 1;
                e.nodes = {id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)};
                e.e = kE;
                e.nu = kNu;
                e.thickness = t;
                part->elements.push_back(e);
            }
        }
        model.shell = part;
        model.materials.clear();
        model.sections.clear();
        for (int j = 0; j <= n; ++j) {
            SolverBridge::NodalLoadData load;
            load.nodeId = id(n, j);
            load.values[0] = -P / n * ((j == 0 || j == n) ? 0.5 : 1.0);
            model.nodalLoads.push_back(load);
        }
        model.bucklingModes = 1;
        const SolverBridge::Result r = SolverBridge(solverLibrary()).solve(model);
        QVERIFY(!r.bucklingFactors.empty());
        const double D = kE * t * t * t / (12.0 * (1.0 - kNu * kNu));
        const double critical = 4.0 * 3.14159265358979323846 * 3.14159265358979323846 * D / a;   // N (largura a)
        QVERIFY2(std::abs(r.bucklingFactors[0] * P - critical) / critical < 0.12, qPrintable(QString::number(r.bucklingFactors[0] * P) + " vs " + QString::number(critical)));
    }

    void combinationsSuperposeShellResults()
    {
        SolverBridge::Result a, b;
        for (SolverBridge::Result *r : {&a, &b}) {
            r->nodeResults.resize(1);
            r->shellNodeResults.resize(1);
            r->shellResults.resize(1);
        }
        a.shellNodeResults[0].displacements[2] = 1.0;
        b.shellNodeResults[0].displacements[2] = 3.0;
        a.shellResults[0].values[0] = 10.0;
        b.shellResults[0].values[0] = 20.0;
        const auto combined = App::Services::CombinationService::combine({{&a, 1.5}, {&b, 0.5}});
        QVERIFY(std::abs(combined.shellNodeResults[0].displacements[2] - 3.0) < 1e-12);
        QVERIFY(std::abs(combined.shellResults[0].values[0] - 25.0) < 1e-12);
    }
};

QTEST_GUILESS_MAIN(TestShellCoupling)
#include "TestShellCoupling.moc"
