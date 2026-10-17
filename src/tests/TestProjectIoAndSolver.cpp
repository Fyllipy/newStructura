#include <QtTest/QtTest>

#include "app/ISceneModelGateway.h"
#include "app/SolverBridge.h"
#include "app/services/SheetBuilder.h"
#include "drawing/HiddenLine.h"
#include "drawing/SheetPainter.h"
#include "viz/structure/SectionShapes.h"

#include "app/services/ConnectionService.h"
#include "app/services/ResultsPersistence.h"
#include "app/services/ModalService.h"
#include "app/services/DynamicsService.h"
#include "app/services/NotionalLoadService.h"
#include "app/services/DxfExporter.h"
#include "app/services/QuantityService.h"
#include "app/services/BucklingService.h"
#include "app/services/CombinationGenerator.h"
#include "app/services/ModelTemplateService.h"
#include "app/services/ModelCheckService.h"
#include "app/services/MaterialLibrary.h"
#include "app/services/UserLibrary.h"
#include "app/services/AnalysisService.h"
#include "app/services/CombinationService.h"
#include "app/services/DatModelIoService.h"
#include "app/services/SectionPlacementService.h"

#include <QFile>
#include <QTemporaryDir>

#include <deque>

using namespace Structura::App;
using namespace Structura::App::Services;

namespace {

/// Gateway minimo em memoria para exercitar o DatModelIoService sem VTK.
class MemoryGateway : public ISceneModelGateway
{
public:
    std::vector<SceneNodeInfo> nodeInfos() const override
    {
        std::vector<SceneNodeInfo> result;
        for (const auto &node : m_nodes) {
            SceneNodeInfo info;
            info.id = node.id();
            info.externalId = node.externalId();
            info.x = node.x();
            info.y = node.y();
            info.z = node.z();
            result.push_back(info);
        }
        return result;
    }
    std::vector<SceneBarInfo> bars() const override { return m_bars; }
    QUuid addPointWithId(double x, double y, double z, int externalId) override
    {
        m_nodes.emplace_back(QUuid::createUuid(), externalId, x, y, z);
        return m_nodes.back().id();
    }
    Structura::Model::Node *findNode(const QUuid &id) override
    {
        for (auto &node : m_nodes) {
            if (node.id() == id) {
                return &node;
            }
        }
        return nullptr;
    }
    QUuid addBar(const QUuid &startNodeId, const QUuid &endNodeId, const QUuid &materialId, const QUuid &sectionId) override
    {
        SceneBarInfo info;
        info.id = QUuid::createUuid();
        info.startNodeId = startNodeId;
        info.endNodeId = endNodeId;
        info.materialId = materialId;
        info.sectionId = sectionId;
        m_bars.push_back(info);
        return info.id;
    }
    void setBarExternalId(const QUuid &barId, int externalId) override
    {
        for (auto &bar : m_bars) {
            if (bar.id == barId) {
                bar.externalId = externalId;
            }
        }
    }

private:
    std::deque<Structura::Model::Node> m_nodes;
    std::vector<SceneBarInfo> m_bars;
};

const char *kSample = R"(
[MATERIALS]
1 2.1e11 8.1e10

[MATERIAL_PROPS]
1 78.5

[SECTIONS]
1 0.0049 3.4e-5 7.2e-6 1.0e-7

[SECTION_GEOMETRY]
1 W_Laminado 1 d=310 bf=165 tw=5.8 tf=9.7 r=10

[NODES]
1 0 0 0 1 1 1 1 1 1
2 4 0 0 0 0 0 0 0 0

[MEMBERS]
1 1 2 1 1

[LOAD_CASES]
CP PP 1
SC SC 0

[COMBINATIONS]
ELU CP=1.35 SC=1.5

[NODAL_LOADS]
2 0 0 -20 0 0 0 SC

[MEMBER_LOADS]
1 GLOBAL 0 0 -15 CP

[MEMBER_MOMENTS]
1 LOCAL 0 0 3 SC

[MEMBER_PLACEMENT]
1 90.0 3 0.01 -0.02

[AXES]
LEVEL N0 0
X A 0
X B 4.5
Y 1 2
LEVEL N1 3
X A 0

[VIEW_OPTIONS]
grid_spacing 0.5
solids 1
)";

struct Loaded {
    QVector<ProjectData::MaterialInfo> materials;
    QVector<ProjectData::SectionInfo> sections;
    QVector<ProjectData::NodeSupport> supports;
    QVector<ProjectData::NodalLoad> nodal;
    QVector<ProjectData::MemberLoad> member;
    ProjectData::ProjectExtras extras;
    QUuid lastMaterial;
    QUuid lastSection;
};

bool load(DatModelIoService &service, MemoryGateway &gateway, const QString &path, Loaded &out)
{
    QString error;
    QStringList warnings;
    return service.load(path, &gateway, out.materials, out.sections, out.supports, out.nodal, out.member,
                        out.extras, out.lastMaterial, out.lastSection, &error, &warnings);
}

} // namespace

class TestProjectIoAndSolver : public QObject
{
    Q_OBJECT

private slots:
    void datExtensionsRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("a.dat"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(kSample);
        file.close();

        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, first, loaded));

        QCOMPARE(loaded.materials.size(), 1);
        QCOMPARE(loaded.materials.first().unitWeight, 78.5);
        QCOMPARE(loaded.sections.first().family, QStringLiteral("W Laminado"));
        QVERIFY(loaded.sections.first().mirrored);
        QCOMPARE(loaded.sections.first().geometry.value(QStringLiteral("tw")), 5.8);
        QCOMPARE(loaded.extras.loadCases.size(), 2);
        QVERIFY(loaded.extras.loadCases.first().selfWeight);
        QCOMPARE(loaded.extras.combinations.size(), 1);
        QCOMPARE(loaded.extras.combinations.first().factors.size(), 2);
        QCOMPARE(loaded.nodal.first().loadCase, QStringLiteral("SC"));
        QCOMPARE(loaded.member.first().loadCase, QStringLiteral("CP"));
        QCOMPARE(loaded.extras.memberMoments.first().mz, 3.0);
        QCOMPARE(loaded.extras.placements.size(), 1);
        QCOMPARE(loaded.extras.placements.first().rotationDeg, 90.0);
        QCOMPARE(loaded.extras.placements.first().reference, 3);
        QCOMPARE(loaded.extras.placements.first().offsetA, 0.01);
        QCOMPARE(loaded.extras.placements.first().offsetB, -0.02);
        QCOMPARE(loaded.extras.axes.levels.size(), 2);
        QCOMPARE(loaded.extras.axes.levels.first().x.size(), 2);
        QCOMPARE(loaded.extras.axes.levels.first().x.at(1).value, 4.5);
        QCOMPARE(loaded.extras.axes.levels.first().y.first().label, QStringLiteral("1"));
        QCOMPARE(loaded.extras.axes.levels.at(1).elevation, 3.0);
        QCOMPARE(loaded.extras.axes.levels.at(1).x.size(), 1);
        // Em XY a 3 m so aparecem os eixos do nivel N1; em XZ os eixos sao a uniao.
        QVector<ProjectData::ReferenceAxis> u;
        QVector<ProjectData::ReferenceAxis> v;
        loaded.extras.axes.planeAxes(2, 3.0, u, v);
        QCOMPARE(u.size(), 1);
        QCOMPARE(v.size(), 0);
        loaded.extras.axes.planeAxes(1, 0.0, u, v);
        QCOMPARE(u.size(), 2);
        QCOMPARE(v.size(), 2);
        QCOMPARE(loaded.extras.viewOptions.value(QStringLiteral("grid_spacing")), 0.5);

        // Salva e recarrega: nada pode ser perdido.
        const QString second = dir.filePath(QStringLiteral("b.dat"));
        QString error;
        QVERIFY(service.save(second, &gateway, loaded.materials, loaded.sections, loaded.supports,
                             loaded.nodal, loaded.member, loaded.extras, &error));
        MemoryGateway gateway2;
        Loaded again;
        QVERIFY(load(service, gateway2, second, again));
        QCOMPARE(again.sections.first().family, loaded.sections.first().family);
        QCOMPARE(again.sections.first().geometry, loaded.sections.first().geometry);
        QCOMPARE(again.extras.loadCases, loaded.extras.loadCases);
        QCOMPARE(again.extras.combinations, loaded.extras.combinations);
        QCOMPARE(again.extras.axes, loaded.extras.axes);
        QCOMPARE(again.extras.placements, loaded.extras.placements);
        QCOMPARE(again.nodal.first().loadCase, QStringLiteral("SC"));
        QCOMPARE(again.materials.first().unitWeight, 78.5);
    }

    void legacyFileGetsDefaultCase()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("legacy.dat"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("[MATERIALS]\n1 2.1e11 8.1e10\n[SECTIONS]\n1 0.01 1e-4 1e-4 1e-4\n"
                   "[NODES]\n1 0 0 0 1 1 1 1 1 1\n2 3 0 0 0 0 0 0 0 0\n[MEMBERS]\n1 1 2 1 1\n"
                   "[NODAL_LOADS]\n2 0 0 -5 0 0 0\n[MEMBER_LOADS]\n1 GLOBAL 0 0 -2\n");
        file.close();

        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, path, loaded));
        QCOMPARE(loaded.extras.loadCases.size(), 1);
        QCOMPARE(loaded.nodal.first().loadCase, loaded.extras.loadCases.first().name);
        QCOMPARE(loaded.member.first().loadCase, loaded.extras.loadCases.first().name);
    }

    void combinationIsLinear()
    {
        SolverBridge::Result a;
        a.nodeResults.resize(1);
        a.memberResults.resize(1);
        a.nodeResults[0].displacements[2] = 2.0;
        a.memberResults[0].localForces[4] = 10.0;
        a.memberResults[0].auxK = {0.0, 0.0, 1.0};
        SolverBridge::Result b = a;
        b.nodeResults[0].displacements[2] = -1.0;
        b.memberResults[0].localForces[4] = 4.0;

        const auto combined = CombinationService::combine({{&a, 1.5}, {&b, 2.0}});
        QCOMPARE(combined.nodeResults[0].displacements[2], 1.0);
        QCOMPARE(combined.memberResults[0].localForces[4], 23.0);
        QCOMPARE(combined.memberResults[0].auxK[2], 1.0);
    }

    void effectiveCombinationDefaultsToAllCases()
    {
        ProjectData::ProjectExtras extras;
        extras.loadCases = {{QStringLiteral("A"), QStringLiteral("A"), false}, {QStringLiteral("B"), QStringLiteral("B"), true}};
        const auto combos = CombinationService::effectiveCombinations(extras);
        QCOMPARE(combos.size(), 1);
        QCOMPARE(combos.first().factors.size(), 2);
    }

    void distributedMomentCantileverReaction()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.1e11, 8.1e10});
        model.sections.push_back({1, 0.005, 4e-6, 8e-6, 1.2e-5});
        SolverBridge::NodeData fixed;
        fixed.id = 1;
        fixed.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData tip;
        tip.id = 2;
        tip.x = 2.0;
        model.nodes = {fixed, tip};
        model.members.push_back({1, 1, 2, 1, 1});
        SolverBridge::MemberMomentData moment;
        moment.memberId = 1;
        moment.coordSystem = SolverBridge::MemberLoadCoordSystem::Global;
        moment.my = 100.0;
        model.memberMoments.push_back(moment);

        SolverBridge solver(dll);
        const auto result = solver.solve(model);
        // Equilibrio: momento total aplicado = 100 * 2 = 200 => reacao My = -200.
        QVERIFY(std::abs(result.nodeResults[0].reactions[4] + 200.0) < 1e-6);
        QVERIFY(std::abs(result.nodeResults[0].reactions[2]) < 1e-6);
    }

    void placementAxisOffsetFromReferencePoint()
    {
        ProjectData::SectionInfo section;
        section.family = QStringLiteral("Barra Retangular");
        section.geometry = {{QStringLiteral("b"), 20.0}, {QStringLiteral("h"), 40.0}};
        ProjectData::MemberPlacement placement;
        const auto centered = SectionPlacementService::axisOffset(&section, placement);
        QCOMPARE(centered.dy, 0.0);
        QCOMPARE(centered.dz, 0.0);

        // Ponto superior esquerdo sobre o no: o centroide fica b/2 a direita (z) e h/2 abaixo (y).
        placement.reference = static_cast<int>(ProjectData::SectionReference::TopLeft);
        const auto corner = SectionPlacementService::axisOffset(&section, placement);
        QVERIFY(std::abs(corner.dz - 0.010) < 1e-9);
        QVERIFY(std::abs(corner.dy + 0.020) < 1e-9);

        // O deslocamento adicional soma-se (desloca a secao em relacao ao no).
        placement.offsetA = 0.005;
        placement.offsetB = 0.002;
        const auto shifted = SectionPlacementService::axisOffset(&section, placement);
        QVERIFY(std::abs(shifted.dz - 0.015) < 1e-9);
        QVERIFY(std::abs(shifted.dy + 0.018) < 1e-9);

        // Mohr: giro de 90 graus troca Iy e Iz; 45 graus gera produto de inercia.
        const auto swapped = SectionPlacementService::inertiaInReferenceAxes(8e-6, 4e-6, 90.0);
        QVERIFY(std::abs(swapped.iy - 4e-6) < 1e-15);
        QVERIFY(std::abs(swapped.iz - 8e-6) < 1e-15);
        const auto diagonal = SectionPlacementService::inertiaInReferenceAxes(8e-6, 4e-6, 45.0);
        QVERIFY(std::abs(diagonal.iy - 6e-6) < 1e-15);
        QVERIFY(std::abs(std::abs(diagonal.iyz) - 2e-6) < 1e-15);
    }

    void solverSectionRotationChangesBendingStiffness()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        auto tipDeflectionY = [&dll](double rotationDeg) {
            SolverBridge::ModelData model;
            model.materials.push_back({1, 2.0e11, 8.0e10});
            model.sections.push_back({1, 0.01, 4e-6, 8e-6, 1e-5});
            SolverBridge::NodeData fixed;
            fixed.id = 1;
            fixed.restraints = {{1, 1, 1, 1, 1, 1}};
            SolverBridge::NodeData tip;
            tip.id = 2;
            tip.x = 5.0;
            model.nodes = {fixed, tip};
            SolverBridge::MemberData member {1, 1, 2, 1, 1};
            member.sectionRotationDeg = rotationDeg;
            model.members.push_back(member);
            SolverBridge::NodalLoadData load;
            load.nodeId = 2;
            load.values = {{0.0, -1000.0, 0.0, 0.0, 0.0, 0.0}};
            model.nodalLoads.push_back(load);
            return SolverBridge(dll).solve(model).nodeResults[1].displacements[1];
        };
        // Barra ao longo de X: carga em Y atua na flexao de Iy (8e-6) sem giro e de Iz (4e-6) com giro de 90 graus.
        const double base = tipDeflectionY(0.0);
        const double rotated = tipDeflectionY(90.0);
        QVERIFY(std::abs(base + 1000.0 * 125.0 / (3.0 * 2.0e11 * 8e-6)) < 1e-9);
        QVERIFY(std::abs(rotated + 1000.0 * 125.0 / (3.0 * 2.0e11 * 4e-6)) < 1e-9);
        // A 45 graus a rigidez efetiva e a media das flexibilidades (Iy e Iz principais).
        const double diagonal = tipDeflectionY(45.0);
        QVERIFY(std::abs(diagonal - 0.5 * (base + rotated)) < 1e-9);
    }

    void solverRigidOffsetCreatesEccentricMoment()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.0e11, 8.0e10});
        model.sections.push_back({1, 0.01, 4e-6, 8e-6, 1e-5});
        SolverBridge::NodeData fixed;
        fixed.id = 1;
        fixed.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData tip;
        tip.id = 2;
        tip.x = 5.0;
        model.nodes = {fixed, tip};
        SolverBridge::MemberData member {1, 1, 2, 1, 1};
        // Eixo da barra 0,1 m acima (y local = Z global) da linha dos nos.
        member.offsetY1 = member.offsetY2 = 0.1;
        model.members.push_back(member);
        SolverBridge::NodalLoadData load;
        load.nodeId = 2;
        load.values = {{1000.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
        model.nodalLoads.push_back(load);

        const auto result = SolverBridge(dll).solve(model);
        // Carga axial 1000 N no no, 0,1 m abaixo do eixo => M = -100 N*m em torno de Y;
        // rotacao na ponta = M L / (E I) com I = 4e-6 (flexao em y local).
        const double rotationY = result.nodeResults[1].displacements[4];
        QVERIFY(std::abs(rotationY + 100.0 * 5.0 / (2.0e11 * 4e-6)) < 1e-9);
        QVERIFY(std::abs(result.nodeResults[1].displacements[2] - 100.0 * 25.0 / (2.0 * 2.0e11 * 4e-6)) < 1e-9);
    }
    void solverSecondOrderColumnMatchesClosedForm()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        const double E = 2.0e11, I = 4e-6, L = 5.0;
        const double pcr = 3.14159265358979 * 3.14159265358979 * E * I / (4.0 * L * L);
        const double P = 0.3 * pcr, H = 100.0;
        auto tip = [&](bool secondOrder) {
            SolverBridge::ModelData model;
            model.secondOrder = secondOrder;
            model.materials.push_back({1, E, 8.0e10});
            model.sections.push_back({1, 0.01, I, I, 1e-5});
            SolverBridge::NodeData fixed;
            fixed.id = 1;
            fixed.restraints = {{1, 1, 1, 1, 1, 1}};
            SolverBridge::NodeData end;
            end.id = 2;
            end.x = L;
            model.nodes = {fixed, end};
            model.members.push_back(SolverBridge::MemberData {1, 1, 2, 1, 1});
            SolverBridge::NodalLoadData load;
            load.nodeId = 2;
            load.values = {{-P, 0.0, H, 0.0, 0.0, 0.0}};
            model.nodalLoads.push_back(load);
            return SolverBridge(dll).solve(model).nodeResults[1].displacements[2];
        };
        const double first = tip(false);
        double second = 0.0;
        try {
            second = tip(true);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
        const double k = std::sqrt(P / (E * I));
        const double exact = H / (P * k) * (std::tan(k * L) - k * L);
        QVERIFY(std::abs(first - H * L * L * L / (3.0 * E * I)) / first < 1e-9);
        QVERIFY(std::abs(second - exact) / exact < 0.03);
        QVERIFY(second > first * 1.2);
    }
    void solverTensionOnlyTiesDeactivateWhenShortening()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        auto build = [&](bool tensionOnly) {
            SolverBridge::ModelData model;
            model.materials.push_back({1, 2.0e11, 8.0e10});
            model.sections.push_back({1, 1e-3, 1e-8, 1e-8, 1e-8});
            SolverBridge::NodeData p1;
            p1.id = 1;
            p1.x = -3.0;
            p1.restraints = {{1, 1, 1, 1, 1, 1}};
            SolverBridge::NodeData p2;
            p2.id = 2;
            p2.x = 3.0;
            p2.restraints = {{1, 1, 1, 1, 1, 1}};
            SolverBridge::NodeData apex;
            apex.id = 3;
            apex.z = 4.0;
            apex.restraints = {{0, 1, 1, 1, 1, 1}};   // livre so em X; Z impedido por um apoio vertical
            model.nodes = {p1, p2, apex};
            SolverBridge::MemberData left {1, 1, 3, 1, 1};
            SolverBridge::MemberData right {2, 2, 3, 1, 1};
            for (auto *m : {&left, &right}) {
                m->releases = {{0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1}};
                m->tensionOnly = tensionOnly;
            }
            model.members = {left, right};
            SolverBridge::NodalLoadData load;
            load.nodeId = 3;
            load.values = {{1000.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
            model.nodalLoads.push_back(load);
            return SolverBridge(dll).solve(model);
        };
        const auto linear = build(false);
        const auto tied = build(true);
        auto axial = [](const SolverBridge::Result &r, int id) {
            for (const auto &m : r.memberResults) {
                if (m.memberId == id) {
                    return m.localForces[6];
                }
            }
            return 0.0;
        };
        // Treliça comum: as duas barras trabalham (uma traciona, a outra comprime)
        QVERIFY(axial(linear, 1) > 100.0);
        QVERIFY(axial(linear, 2) < -100.0);
        // Tirantes: a barra que encurta e desativada e a outra recebe todo o esforco: T = 5H/3
        QVERIFY(std::abs(axial(tied, 2)) < 1.0);
        QVERIFY(std::abs(axial(tied, 1) - 5000.0 / 3.0) < 5.0);
    }
    void newExtrasRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("a.dat"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(kSample);
        file.close();
        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, first, loaded));

        ProjectData::BarGroup group;
        group.id = 3;
        group.name = QStringLiteral("Tercas cobertura");
        group.members = {1, 2, 5};
        loaded.extras.groups.append(group);
        ProjectData::AreaLoadPanel panel;
        panel.id = 1;
        panel.loadCase = QStringLiteral("Vento X");
        panel.kind = QStringLiteral("WIND");
        panel.pressure = -0.72;
        panel.direction = {0.0, 0.6, -0.8};
        panel.polygon = {{0, 0, 0}, {6, 0, 0}, {6, 3, 4}, {0, 3, 4}};
        panel.members = {7, 8};
        loaded.extras.areaLoads.append(panel);
        ProjectData::MemberPlacement tie;
        tie.memberId = 9;
        tie.memberType = ProjectData::MemberType::kTie;
        tie.releaseMask = ProjectData::MemberType::kReleaseRotations;
        loaded.extras.placements.append(tie);

        const QString second = dir.filePath(QStringLiteral("b.dat"));
        QString error;
        QVERIFY(service.save(second, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, &error));
        MemoryGateway gateway2;
        Loaded again;
        QVERIFY(load(service, gateway2, second, again));
        QCOMPARE(again.extras.groups.size(), 1);
        QCOMPARE(again.extras.groups.first().name, group.name);
        QCOMPARE(again.extras.groups.first().members, group.members);
        QCOMPARE(again.extras.areaLoads.size(), 1);
        QCOMPARE(again.extras.areaLoads.first().loadCase, panel.loadCase);
        QCOMPARE(again.extras.areaLoads.first().kind, panel.kind);
        QVERIFY(std::abs(again.extras.areaLoads.first().pressure - panel.pressure) < 1e-9);
        QCOMPARE(again.extras.areaLoads.first().polygon.size(), 4);
        QVERIFY(std::abs(again.extras.areaLoads.first().polygon.at(2)[2] - 4.0) < 1e-9);
        QCOMPARE(again.extras.areaLoads.first().members, panel.members);
        bool foundTie = false;
        for (const auto &p : again.extras.placements) {
            foundTie = foundTie || (p.memberId == 9 && p.memberType == ProjectData::MemberType::kTie);
        }
        QVERIFY(foundTie);
    }

    void userLibraryPersistsProfilesAndMaterials()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Structura::App::Services::UserLibrary::setDirectory(dir.path());
        ProjectData::SectionInfo section;
        section.name = QStringLiteral("Ue 200x75x25 #3");
        section.family = QStringLiteral("C Formado");
        section.geometry = {{"d", 200}, {"bf", 75}, {"c", 25}, {"t", 3.0}};
        section.area = 1.0e-3;
        QVERIFY(Structura::App::Services::UserLibrary::saveProfile(section));
        section.area = 2.0e-3;   // substitui pelo par familia + nome
        QVERIFY(Structura::App::Services::UserLibrary::saveProfile(section));
        const auto profiles = Structura::App::Services::UserLibrary::loadProfiles();
        QCOMPARE(profiles.size(), 1);
        QCOMPARE(profiles.first().geometry.value(QStringLiteral("bf")), 75.0);
        QCOMPARE(profiles.first().area, 2.0e-3);

        Structura::App::Services::MaterialLibrary::Entry material;
        material.name = QStringLiteral("Aco especial");
        material.category = Structura::App::Services::MaterialLibrary::Category::StructuralSteel;
        material.fy = 380;
        material.fu = 500;
        QVERIFY(Structura::App::Services::UserLibrary::saveMaterial(material));
        const auto materials = Structura::App::Services::UserLibrary::loadMaterials();
        QCOMPARE(materials.size(), 1);
        QCOMPARE(materials.first().fy, 380.0);
        Structura::App::Services::MaterialLibrary::setUserEntries(materials);
        QVERIFY(Structura::App::Services::MaterialLibrary::find(QStringLiteral("Aco especial")) != nullptr);
        QVERIFY(Structura::App::Services::UserLibrary::removeMaterial(QStringLiteral("Aco especial")));
        QVERIFY(Structura::App::Services::UserLibrary::loadMaterials().isEmpty());
        Structura::App::Services::MaterialLibrary::setUserEntries({});
        Structura::App::Services::UserLibrary::setDirectory(QString());
    }
    void modelCheckFindsCommonProblems()
    {
        using Structura::App::Services::ModelCheckService;
        ModelCheckService::Input in;
        in.nodes = {{1, 0, 0, 0}, {2, 6, 0, 0}, {3, 3, 0, 0}, {4, 20, 20, 0}};   // 3 esta sobre a barra 1-2; 4 solto
        ModelCheckService::Bar a;
        a.id = 1;
        a.nodeI = 1;
        a.nodeJ = 2;
        a.hasMaterial = true;
        a.hasSection = true;
        a.family = QStringLiteral("W Laminado");
        ModelCheckService::Bar dup = a;
        dup.id = 2;
        in.bars = {a, dup};
        ProjectData::MaterialInfo material;
        material.name = QStringLiteral("aco");
        material.youngModulus = 2e8;
        material.shearModulus = 8e7;
        in.materials = {material};
        auto has = [](const QVector<ModelCheckService::Issue> &issues, const char *code) {
            for (const auto &i : issues) {
                if (i.code == QLatin1String(code)) {
                    return true;
                }
            }
            return false;
        };
        auto issues = ModelCheckService::run(in);
        QVERIFY(has(issues, "NODE_ORPHAN"));
        QVERIFY(has(issues, "NODE_ON_BAR"));
        QVERIFY(has(issues, "BAR_DUPLICATE"));
        QVERIFY(has(issues, "NO_SUPPORTS"));
        QVERIFY(has(issues, "NO_LOADS"));
        ProjectData::NodeSupport fixed;
        fixed.nodeId = 1;
        for (bool &r : fixed.restraints) {
            r = true;
        }
        in.supports = {fixed};
        issues = ModelCheckService::run(in);
        QVERIFY(!has(issues, "NO_SUPPORTS"));
        QVERIFY(!has(issues, "COMPONENT_UNSUPPORTED"));
        // trecho solto: segunda barra ligada so aos nos 4-5 sem apoio
        in.nodes.append({5, 20, 25, 0});
        ModelCheckService::Bar floating = a;
        floating.id = 3;
        floating.nodeI = 4;
        floating.nodeJ = 5;
        in.bars.append(floating);
        QVERIFY(has(ModelCheckService::run(in), "COMPONENT_UNSUPPORTED"));
    }
    void templatesGenerateLoadableModels()
    {
        using Structura::App::Services::ModelTemplateService;
        if (ModelTemplateService::catalogNames(QStringLiteral("W Laminado")).isEmpty()) {
            QSKIP("Catalogo de perfis (recurso do executavel) indisponivel nos testes.");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto loadText = [&](const QString &text, Loaded &out, MemoryGateway &gateway) {
            const QString path = dir.filePath(QStringLiteral("t.dat"));
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly)) {
                return false;
            }
            f.write(text.toUtf8());
            f.close();
            DatModelIoService service;
            return load(service, gateway, path, out);
        };
        ModelTemplateService::ShedParams shed;
        shed.frames = 3;
        shed.panelsPerSlope = 2;
        QString error;
        const QString shedText = ModelTemplateService::buildShed(shed, &error);
        QVERIFY2(!shedText.isEmpty(), qPrintable(error));
        MemoryGateway g1;
        Loaded shedModel;
        QVERIFY(loadText(shedText, shedModel, g1));
        QCOMPARE(g1.nodeInfos().size(), static_cast<std::size_t>(21));
        QCOMPARE(g1.bars().size(), static_cast<std::size_t>(44));
        QVERIFY(!shedModel.member.isEmpty());
        QCOMPARE(shedModel.extras.loadCases.size(), 3);
        QCOMPARE(shedModel.extras.combinations.size(), 2);
        QVERIFY(!shedModel.extras.groups.isEmpty());
        int ties = 0;
        for (const auto &p : shedModel.extras.placements) {
            ties += p.memberType == ProjectData::MemberType::kTie ? 1 : 0;
        }
        QCOMPARE(ties, 16);

        ModelTemplateService::BuildingParams building;
        MemoryGateway g2;
        Loaded buildingModel;
        const QString buildingText = ModelTemplateService::buildBuilding(building, &error);
        QVERIFY2(!buildingText.isEmpty(), qPrintable(error));
        QVERIFY(loadText(buildingText, buildingModel, g2));
        QCOMPARE(g2.nodeInfos().size(), static_cast<std::size_t>(3 * 5 * 4));
        QVERIFY(!buildingModel.member.isEmpty());

        ModelTemplateService::ShedParams bad;
        bad.columnProfile = QStringLiteral("W 999 x 1");
        QVERIFY(ModelTemplateService::buildShed(bad, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
    void combinationGeneratorFollowsNbr8681()
    {
        using Structura::App::Services::CombinationGenerator;
        using K = CombinationGenerator::Kind;
        QVector<CombinationGenerator::CaseSetup> cases {
            CombinationGenerator::defaults(QStringLiteral("PP"), K::SteelSelfWeight),
            CombinationGenerator::defaults(QStringLiteral("SC"), K::Variable),
            CombinationGenerator::defaults(QStringLiteral("VT1"), K::Wind),
            CombinationGenerator::defaults(QStringLiteral("VT2"), K::Wind)};
        const auto combos = CombinationGenerator::generate(cases, {});
        auto factor = [](const ProjectData::LoadCombination &c, const QString &name) {
            for (const auto &f : c.factors) {
                if (f.first == name) {
                    return f.second;
                }
            }
            return 0.0;
        };
        int ultimate = 0, service = 0;
        bool sawSuction = false;
        for (const auto &c : combos) {
            // vento nunca coexiste com outro vento
            QVERIFY(factor(c, QStringLiteral("VT1")) == 0.0 || factor(c, QStringLiteral("VT2")) == 0.0);
            if (c.name.startsWith(QLatin1String("ELU"))) {
                ++ultimate;
                if (factor(c, QStringLiteral("SC")) == 1.5 && factor(c, QStringLiteral("VT1")) + factor(c, QStringLiteral("VT2")) > 0) {            // sobrecarga principal com vento secundario
                    QCOMPARE(factor(c, QStringLiteral("PP")), 1.25);
                    QVERIFY(qAbs(factor(c, QStringLiteral("VT1")) + factor(c, QStringLiteral("VT2")) - 1.4 * 0.6) < 1e-9);
                }
                if (factor(c, QStringLiteral("PP")) == 1.0) {            // permanente favoravel com vento principal
                    sawSuction = true;
                    QVERIFY(factor(c, QStringLiteral("VT1")) == 1.4 || factor(c, QStringLiteral("VT2")) == 1.4);
                }
            } else {
                ++service;
            }
        }
        QCOMPARE(ultimate, 3 + 2 + 2);   // SC (sem vento, VT1, VT2) + VT1 + VT2 + 2 favoraveis
        QVERIFY(sawSuction);
        QVERIFY(service >= 2);
    }
    void bucklingMatchesEulerLoad()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        const double E = 2.0e11, I = 4e-6, L = 5.0, P = 1000.0;
        const double pcr = 3.14159265358979 * 3.14159265358979 * E * I / (4.0 * L * L);   // pilar engastado-livre
        SolverBridge::ModelData model;
        model.materials.push_back({1, E, 8.0e10});
        model.sections.push_back({1, 0.01, I, I, 1e-5});
        const int segments = 4;
        for (int i = 0; i <= segments; ++i) {
            SolverBridge::NodeData node;
            node.id = i + 1;
            node.x = L * i / segments;
            if (i == 0) {
                node.restraints = {{1, 1, 1, 1, 1, 1}};
            }
            model.nodes.push_back(node);
        }
        for (int i = 0; i < segments; ++i) {
            model.members.push_back(SolverBridge::MemberData {i + 1, i + 1, i + 2, 1, 1});
        }
        SolverBridge::NodalLoadData load;
        load.nodeId = segments + 1;
        load.values = {{-P, 0.0, 0.0, 0.0, 0.0, 0.0}};
        model.nodalLoads.push_back(load);
        try {
            const auto result = Structura::App::Services::BucklingService::run(model, 3);
            QVERIFY(result.bucklingFactors.size() >= 2);
            QVERIFY(std::abs(result.bucklingFactors[0] * P - pcr) / pcr < 0.01);
            QVERIFY(std::abs(result.bucklingFactors[1] - result.bucklingFactors[0]) / result.bucklingFactors[0] < 1e-6);   // I igual nos dois eixos
            const auto lines = Structura::App::Services::BucklingService::shapeLines(model, result.bucklingModes[0], 0.5);
            QCOMPARE(static_cast<int>(lines.size()), segments);
            QVERIFY(std::abs(std::abs(lines.back().back()[1]) + std::abs(lines.back().back()[2]) - 0.5) < 0.05);   // ponta desloca ~0.5 em um dos eixos
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void quantitiesAndDxfExport()
    {
        using Structura::App::Services::QuantityService;
        using Structura::App::Services::DxfExporter;
        ProjectData::MaterialInfo material;
        material.uuid = QUuid::createUuid();
        material.name = QStringLiteral("aco");
        material.youngModulus = 2e8;   // kN/m2
        material.unitWeight = 78.5;    // kN/m3
        ProjectData::SectionInfo section;
        section.uuid = QUuid::createUuid();
        section.name = QStringLiteral("S1");
        section.area = 0.01;           // m2
        QVector<QuantityService::Bar> bars;
        for (int i = 0; i < 3; ++i) {
            QuantityService::Bar b;
            b.id = i + 1;
            b.length = 4.0;
            b.sectionId = section.uuid;
            b.materialId = material.uuid;
            bars.append(b);
        }
        const auto summary = QuantityService::compute(bars, {section}, {material});
        QCOMPARE(summary.rows.size(), 1);
        QCOMPARE(summary.barCount, 3);
        // 0.01 m2 * 78.5 kN/m3 = 0.785 kN/m = 80.04 kg/m ; 12 m
        QVERIFY(std::abs(summary.totalMass - 12.0 * 0.785 * 1000.0 / 9.80665) < 1e-6);
        QVERIFY(QuantityService::toCsv(summary).contains(QStringLiteral("TOTAL")));
        DxfExporter::Options opt;
        opt.view = DxfExporter::View::ElevationXZ;
        DxfExporter::Bar b;
        b.id = 1;
        b.x2 = 3;
        b.y2 = 9;
        b.z2 = 4;
        b.section = QStringLiteral("W 250 x 17,9");
        const QString dxf = DxfExporter::build({{1, 0, 0, 0}}, {b}, opt);
        QVERIFY(dxf.contains(QStringLiteral("LINE")));
        QVERIFY(dxf.contains(QStringLiteral("W_250_X_17_9")));
        QVERIFY(dxf.endsWith(QStringLiteral("EOF\r\n")));
        QVERIFY(dxf.contains(QStringLiteral("11\r\n3.000000")) && dxf.contains(QStringLiteral("21\r\n4.000000")));   // x, z projetados
    }
    void dynamicsMatchesSingleDofReference()
    {
        using Structura::App::Services::DynamicsService;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        // balanco horizontal com massa de 100 kN no extremo: k = 3EI/L3, m = P/g
        const double E = 2.0e8, I = 1.0e-4, A = 0.01, L = 4.0, P = 100.0;
        SolverBridge::ModelData model;
        model.materials.push_back({1, E, 8.0e7});
        model.sections.push_back({1, A, I, I, 1e-5});
        const int segments = 8;
        for (int i = 0; i <= segments; ++i) {
            SolverBridge::NodeData node;
            node.id = i + 1;
            node.x = L * i / segments;
            if (i == 0) {
                node.restraints = {{1, 1, 1, 1, 1, 1}};
            }
            model.nodes.push_back(node);
        }
        for (int i = 0; i < segments; ++i) {
            model.members.push_back(SolverBridge::MemberData {i + 1, i + 1, i + 2, 1, 1});
        }
        SolverBridge::NodalLoadData weight;
        weight.nodeId = segments + 1;
        weight.values = {{0.0, 0.0, -P, 0.0, 0.0, 0.0}};
        model.nodalLoads.push_back(weight);
        const double k = 3.0 * E * I / (L * L * L);
        const double m = P / 9.80665;
        const double omega = std::sqrt(k / m);
        try {
            const auto modal = Structura::App::Services::ModalService::run(model, 3, false);
            DynamicsService::Basis basis;
            basis.model = &model;
            basis.modal = &modal;
            basis.mass = DynamicsService::nodalMasses(model, false);
            QVERIFY(std::abs(modal.modalOmega[0] - omega) / omega < 0.03);
            // espectro plano de 0,5 g: cortante basal = massa efetiva x Sa (~ m x 0,5 g)
            DynamicsService::SpectrumOptions options;
            options.spectrum.points = {{0.0, 0.5}, {10.0, 0.5}};
            options.directions = {{false, false, true}};
            const auto spectrum = DynamicsService::responseSpectrum(basis, options);
            QVERIFY2(spectrum.warning.isEmpty() || spectrum.baseShear[2] > 0.0, qPrintable(spectrum.warning));
            QVERIFY(std::abs(spectrum.baseShear[2] - 0.5 * P) / (0.5 * P) < 0.03);
            QVERIFY(std::abs(spectrum.reactions[0][2] - 0.5 * P) / (0.5 * P) < 0.04);
            QVERIFY(!spectrum.members.empty());
            // piso: frequencia e massa modal do modo vertical
            DynamicsService::FloorInput floor;
            floor.mode = DynamicsService::firstVerticalMode(basis);
            const auto vib = DynamicsService::floorVibration(basis, floor);
            QVERIFY(vib.ok);
            QVERIFY(std::abs(vib.frequency - omega / (2.0 * 3.14159265358979)) / vib.frequency < 0.03);
            QVERIFY(std::abs(vib.modalMass - m * 1000.0) / (m * 1000.0) < 0.03);
            QCOMPARE(vib.controlNode, segments + 1);
            QVERIFY(vib.peak > 0.0 && vib.limitPeak > 0.0);
            // harmonica na ressonancia: amplitude estacionaria F / (2 zeta k)
            DynamicsService::TimeInput time;
            time.amplitude = 1.0;
            time.frequency = modal.modalOmega[0] / (2.0 * 3.14159265358979);
            time.duration = 30.0;
            time.damping = 0.05;
            time.loadNode = segments + 1;
            time.responseNode = segments + 1;
            time.modeCount = 3;
            const auto history = DynamicsService::timeHistory(basis, time);
            QVERIFY(history.ok);
            const double expected = 1.0 / (2.0 * 0.05 * k);
            QVERIFY(std::abs(history.peakDisplacement - expected) / expected < 0.06);
            QVERIFY(std::abs(history.staticDisplacement - 1.0 / k) / (1.0 / k) < 0.04);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }

    void notionalLoadsAreProportionalToGravityAndExpandCombinations()
    {
        using Structura::App::Services::NotionalLoadService;
        SolverBridge::ModelData model;
        model.nodes.push_back({1, 0, 0, 0});
        model.nodes.push_back({2, 0, 0, 3});
        SolverBridge::NodalLoadData gravity;
        gravity.nodeId = 2;
        gravity.values = {{5.0, 0.0, -200.0, 0.0, 0.0, 0.0}};
        model.nodalLoads.push_back(gravity);
        const auto x = NotionalLoadService::notionalModel(model, 0, 0.003);
        QCOMPARE(static_cast<int>(x.nodalLoads.size()), 1);
        QVERIFY(std::abs(x.nodalLoads[0].values[0] - 0.6) < 1e-9);   // 0,3 % de 200 kN
        QCOMPARE(x.nodalLoads[0].values[1], 0.0);
        QCOMPARE(x.nodalLoads[0].values[2], 0.0);
        const auto y = NotionalLoadService::notionalModel(model, 1, 0.003);
        QVERIFY(std::abs(y.nodalLoads[0].values[1] - 0.6) < 1e-9);
        QVector<ProjectData::LoadCombination> combos;
        combos.append({QStringLiteral("ELU1"), {{QStringLiteral("PP"), 1.25}, {QStringLiteral("SC"), 1.5}}});
        combos.append({QStringLiteral("ELS1"), {{QStringLiteral("PP"), 1.0}}});
        const auto all = NotionalLoadService::withVariants(combos);
        QCOMPARE(all.size(), 2 + 4);   // so a combinacao ultima ganha variantes
        QCOMPARE(all[2].name, QStringLiteral("ELU1 +NX"));
        QCOMPARE(all[3].name, QStringLiteral("ELU1 -NX"));
        QCOMPARE(all[2].factors.size(), 4);
        QCOMPARE(all[2].factors[2].first, NotionalLoadService::key(0, QStringLiteral("PP")));
        QVERIFY(std::abs(all[3].factors[2].second + 1.25) < 1e-12);
        QVERIFY(NotionalLoadService::isNotionalKey(all[2].factors[3].first));
    }

    void modalMatchesCantileverFrequency()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        const double E = 2.0e11, I = 4e-6, A = 0.01, L = 5.0, rho = 7850.0;
        SolverBridge::ModelData model;
        SolverBridge::MaterialData material {1, E, 8.0e10};
        material.density = rho;
        model.materials.push_back(material);
        model.sections.push_back({1, A, I, I, 1e-5});
        const int segments = 8;
        for (int i = 0; i <= segments; ++i) {
            SolverBridge::NodeData node;
            node.id = i + 1;
            node.x = L * i / segments;
            if (i == 0) {
                node.restraints = {{1, 1, 1, 1, 1, 1}};
            }
            model.nodes.push_back(node);
        }
        for (int i = 0; i < segments; ++i) {
            model.members.push_back(SolverBridge::MemberData {i + 1, i + 1, i + 2, 1, 1});
        }
        try {
            const auto result = Structura::App::Services::ModalService::run(model, 4, true);
            QVERIFY(result.modalOmega.size() >= 2);
            const double expected = 1.875104 * 1.875104 * std::sqrt(E * I / (rho * A * L * L * L * L));
            QVERIFY(std::abs(result.modalOmega[0] - expected) / expected < 0.005);
            QVERIFY(std::abs(result.modalOmega[1] - result.modalOmega[0]) / expected < 1e-6);   // flexao nos dois planos
            // nenhuma massa efetiva em X para flexao; Y ou Z domina
            QVERIFY(result.modalMassRatio[0][0] < 1e-6);
            QVERIFY(result.modalMassRatio[0][1] + result.modalMassRatio[0][2] > 0.5);
            // massa adicional concentrada no topo reduz a frequencia
            auto loaded = model;
            SolverBridge::NodalLoadData weight;
            weight.nodeId = segments + 1;
            weight.values = {{0.0, 0.0, -10.0 * 9.80665 * 1000.0, 0.0, 0.0, 0.0}};   // 10 t
            loaded.nodalLoads.push_back(weight);
            const auto heavy = Structura::App::Services::ModalService::run(loaded, 1, true);
            QVERIFY(heavy.modalOmega[0] < result.modalOmega[0] * 0.9);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void resultsAndEffectiveLengthsRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("r1.dat"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(kSample);
        file.close();
        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, first, loaded));

        ProjectData::DesignMemberParams params;
        params.memberId = 2;
        params.kx = 0.7;
        params.lx = 3.25;
        params.ly = 1.5;
        loaded.extras.design.members.append(params);
        loaded.extras.design.defaults.lz = 2.0;

        ProjectData::StoredRun run;
        run.name = QStringLiteral("Vento X");
        ProjectData::StoredNodeResult node;
        node.nodeId = 1;
        node.displacements = {1.5e-3, -2.25e-4, 0.0, 1e-5, 0.0, -3.5e-6};
        node.reactions = {10.5, 0.0, -123.456789012, 0.0, 7.25, 0.0};
        run.nodes.append(node);
        ProjectData::StoredMemberResult member;
        member.memberId = 1;
        for (int i = 0; i < 12; ++i) {
            member.forces[static_cast<std::size_t>(i)] = 0.5 * i - 2.0;
        }
        member.auxK = {0.0, 1.0, 0.0};
        run.members.append(member);
        loaded.extras.results.runs.append(run);
        loaded.extras.results.fingerprint = QStringLiteral("abc123");
        ProjectData::StoredDesignResult design;
        design.memberId = 1;
        design.section = QStringLiteral("W 310 x 32.7");
        design.eta = 0.8125;
        design.check = QStringLiteral("flexure_ltb");
        design.combination = QStringLiteral("ELU 1");
        design.lengthM = 5.5;
        loaded.extras.results.design.append(design);
        loaded.extras.results.bucklingCombination = QStringLiteral("ELU1");
        loaded.extras.results.bucklingFactors = {4.25, 6.5};
        loaded.extras.results.modalSource = QStringLiteral("ELS-QP1");
        loaded.extras.results.modalOmega = {5.5, 11.0};
        loaded.extras.results.modalMassRatio = {{0.0, 0.9, 0.0}, {0.5, 0.0, 0.0}};

        const QString second = dir.filePath(QStringLiteral("r2.dat"));
        QString error;
        QVERIFY(service.save(second, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, &error));
        MemoryGateway gateway2;
        Loaded again;
        QVERIFY(load(service, gateway2, second, again));
        QCOMPARE(again.extras.design.members.size(), 1);
        QCOMPARE(again.extras.design.members.first().lx, 3.25);
        QCOMPARE(again.extras.design.members.first().ly, 1.5);
        QCOMPARE(again.extras.design.members.first().lz, 0.0);
        QCOMPARE(again.extras.design.defaults.lz, 2.0);
        const auto &r = again.extras.results;
        QCOMPARE(r.fingerprint, QStringLiteral("abc123"));
        QCOMPARE(r.runs.size(), 1);
        QCOMPARE(r.runs.first().name, run.name);
        QVERIFY(std::abs(r.runs.first().nodes.first().reactions[2] - (-123.456789012)) < 1e-9);
        QCOMPARE(r.runs.first().nodes.first().displacements[0], 1.5e-3);
        QCOMPARE(r.runs.first().members.first().forces[11], 0.5 * 11 - 2.0);
        QCOMPARE(r.design.size(), 1);
        QCOMPARE(r.design.first().section, design.section);
        QCOMPARE(r.design.first().combination, design.combination);
        QVERIFY(std::abs(r.design.first().eta - 0.8125) < 1e-9);
        QCOMPARE(r.bucklingFactors.size(), 2);
        QCOMPARE(r.bucklingCombination, QStringLiteral("ELU1"));
        QCOMPARE(r.modalOmega.size(), 2);
        QVERIFY(std::abs(r.modalMassRatio[0][1] - 0.9) < 1e-9);

        // conversao solver <-> gravado
        SolverBridge::ModelData model;
        SolverBridge::NodeData n1;
        n1.id = 1;
        model.nodes.push_back(n1);
        model.members.push_back(SolverBridge::MemberData {1, 1, 1, 1, 1});
        bool ok = false;
        const auto restored = Structura::App::Services::ResultsPersistence::fromStored(r.runs.first(), model, &ok);
        QVERIFY(ok);
        QCOMPARE(restored.nodeResults.front().displacements[0], 1.5e-3);
        const QString fp1 = Structura::App::Services::ResultsPersistence::fingerprint({&model}, false, 0.8);
        model.nodes.front().x = 1.0;
        QVERIFY(fp1 != Structura::App::Services::ResultsPersistence::fingerprint({&model}, false, 0.8));
    }
    void nodeConnectionsEvaluateAndPersist()
    {
        using Structura::App::Services::ConnectionService;
        ConnectionService::BeamInfo beam;
        beam.name = QStringLiteral("W 310 x 32.7");
        beam.depth = 313;
        beam.tw = 6.6;
        auto connection = ConnectionService::makeDefault(ProjectData::NodeConnection::kShearTab, beam, 250.0, 400.0);
        connection.id = 1;
        connection.nodeId = 7;
        connection.memberId = 12;
        connection.atEnd = true;
        ConnectionService::Forces forces;
        forces.shear = 80000.0;
        forces.axial = 10000.0;
        QVERIFY(ConnectionService::propose(connection, forces));
        const auto evaluation = ConnectionService::evaluate(connection, forces);
        QVERIFY(evaluation.maxRatio <= 1.0 + 1e-9);
        QVERIFY(!evaluation.parts.empty());
        QVERIFY(!evaluation.governing.isEmpty());
        QVERIFY(!ConnectionService::describe(connection).isEmpty());
        // mais esforco -> mais razao com a mesma geometria
        forces.shear = 400000.0;
        QVERIFY(ConnectionService::evaluate(connection, forces).maxRatio > evaluation.maxRatio);
        connection.eta = evaluation.maxRatio;
        connection.verified = true;
        connection.governing = evaluation.governing;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("c1.dat"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(kSample);
        file.close();
        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, first, loaded));
        loaded.extras.connections.append(connection);
        auto angle = ConnectionService::makeDefault(ProjectData::NodeConnection::kDoubleAngle, beam, 250.0, 400.0);
        angle.id = 2;
        angle.nodeId = 8;
        angle.memberId = 13;
        loaded.extras.connections.append(angle);
        const QString second = dir.filePath(QStringLiteral("c2.dat"));
        QString error;
        QVERIFY(service.save(second, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, &error));
        MemoryGateway gateway2;
        Loaded again;
        QVERIFY(load(service, gateway2, second, again));
        QCOMPARE(again.extras.connections.size(), 2);
        const auto &c = again.extras.connections.first();
        QCOMPARE(c.nodeId, 7);
        QCOMPARE(c.memberId, 12);
        QVERIFY(c.atEnd);
        QCOMPARE(c.type, ProjectData::NodeConnection::kShearTab);
        QCOMPARE(c.grade, connection.grade);
        QVERIFY(c.verified);
        QVERIFY(std::abs(c.eta - connection.eta) < 1e-6);
        QCOMPARE(c.governing, connection.governing);
        QCOMPARE(c.values.value(QStringLiteral("plateT")), connection.values.value(QStringLiteral("plateT")));
        QCOMPARE(again.extras.connections.at(1).type, ProjectData::NodeConnection::kDoubleAngle);
    }
    void newConnectionTypesEvaluateProposeAndPersist()
    {
        using Structura::App::Services::ConnectionService;
        using NC = ProjectData::NodeConnection;
        ConnectionService::BeamInfo member;
        member.name = QStringLiteral("W 310 x 32.7");
        member.family = QStringLiteral("W Laminado");
        member.depth = 313;
        member.bf = 102;
        member.tw = 6.6;
        member.tf = 10.8;
        member.area = 4190;
        member.fy = 345;
        member.fu = 450;
        QVector<NC> list;
        // chapa de no
        {
            ConnectionService::BeamInfo diagonal = member;
            diagonal.family = QStringLiteral("L Laminado");
            diagonal.geometry = {{"b", 64.0}, {"t", 6.35}};
            diagonal.area = 770;
            auto c = ConnectionService::makeDefault(NC::kGusset, diagonal, 250.0, 400.0);
            QCOMPARE(static_cast<int>(c.values.value(QStringLiteral("bars"))), 2);
            QVERIFY(std::abs(c.values.value(QStringLiteral("ec")) - 18.34) < 0.05);
            ConnectionService::Forces f;
            f.tension = 150000.0;
            f.compression = 100000.0;
            QVERIFY(ConnectionService::propose(c, f));
            const auto e = ConnectionService::evaluate(c, f);
            QVERIFY(e.maxRatio <= 1.0 + 1e-9);
            QVERIFY(!e.parts.empty());
            c.id = 1;
            list.append(c);
        }
        // emenda: a combinacao mais desfavoravel governa
        {
            auto c = ConnectionService::makeDefault(NC::kSplice, member, 250.0, 400.0);
            ConnectionService::Forces f;
            ConnectionService::ForceCase light, heavy;
            light.name = QStringLiteral("ELU1");
            light.moment = 10.0e6;
            light.shear = 20000.0;
            heavy.name = QStringLiteral("ELU2");
            heavy.moment = 45.0e6;
            heavy.shear = 60000.0;
            f.cases = {light, heavy};
            QVERIFY(ConnectionService::propose(c, f));
            const auto e = ConnectionService::evaluate(c, f);
            QVERIFY(e.maxRatio <= 1.0 + 1e-9);
            ConnectionService::Forces onlyLight, onlyHeavy;
            onlyLight.cases = {light};
            onlyHeavy.cases = {heavy};
            QVERIFY(std::abs(e.maxRatio - std::max(ConnectionService::evaluate(c, onlyLight).maxRatio, ConnectionService::evaluate(c, onlyHeavy).maxRatio)) < 1e-12);
            QVERIFY(ConnectionService::evaluate(c, onlyHeavy).maxRatio >= ConnectionService::evaluate(c, onlyLight).maxRatio);
            QVERIFY(!e.governingCase.isEmpty());
            c.id = 2;
            list.append(c);
        }
        // base de pilar
        {
            auto c = ConnectionService::makeDefault(NC::kBasePlate, member, 250.0, 400.0);
            ConnectionService::Forces f;
            ConnectionService::ForceCase combo;
            combo.name = QStringLiteral("ELU1");
            combo.axial = -250000.0;
            combo.moment = 20.0e6;
            combo.shear = 25000.0;
            f.cases = {combo};
            QVERIFY(ConnectionService::propose(c, f));
            QVERIFY(ConnectionService::evaluate(c, f).maxRatio <= 1.0 + 1e-9);
            c.id = 3;
            list.append(c);
        }
        // ligacao tubular: sem proposta automatica; razao cresce com a forca
        {
            ConnectionService::BeamInfo chord = member;
            chord.geometry = {{"D", 168.3}, {"t", 6.0}};
            QVector<ConnectionService::BraceInfo> braces(2);
            braces[0].d = 88.9; braces[0].t = 4.0; braces[0].angleDeg = 45.0; braces[0].fy = 345.0;
            braces[1].d = 88.9; braces[1].t = 4.0; braces[1].angleDeg = 45.0; braces[1].fy = 345.0;
            auto c = ConnectionService::makeTubular(chord, braces);
            QCOMPARE(c.type, static_cast<int>(NC::kTubular));
            QVERIFY(!ConnectionService::hasProposal(NC::kTubular));
            ConnectionService::Forces f;
            ConnectionService::ForceCase combo;
            combo.name = QStringLiteral("ELU1");
            combo.axial = -100000.0;
            combo.braceAxial = {80000.0, 80000.0};
            f.cases = {combo};
            const auto e1 = ConnectionService::evaluate(c, f);
            f.cases[0].braceAxial = {160000.0, 160000.0};
            QVERIFY(ConnectionService::evaluate(c, f).maxRatio > e1.maxRatio);
            c.id = 4;
            list.append(c);
        }
        // persistencia dos novos tipos
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("n1.dat"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(kSample);
        file.close();
        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, first, loaded));
        for (auto &c : list) {
            c.nodeId = 1;
            c.memberId = c.id;
            loaded.extras.connections.append(c);
        }
        const QString second = dir.filePath(QStringLiteral("n2.dat"));
        QString error;
        QVERIFY(service.save(second, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, &error));
        MemoryGateway gateway2;
        Loaded again;
        QVERIFY(load(service, gateway2, second, again));
        QCOMPARE(again.extras.connections.size(), 4);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(again.extras.connections.at(i).type, list.at(i).type);
            QCOMPARE(again.extras.connections.at(i).values.size(), list.at(i).values.size());
        }
        QCOMPARE(again.extras.connections.at(3).values.value(QStringLiteral("kindT")), list.at(3).values.value(QStringLiteral("kindT")));
    }
    void sheetLayoutsPersistAndManualPlacementApplies()
    {
        using NC = ProjectData::SheetLayoutDef;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("s1.dat"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(kSample);
        file.close();
        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, first, loaded));
        NC layout;
        layout.name = QStringLiteral("Planta e elevação / A");
        layout.views.append({QStringLiteral("plan-proj"), 100.0, false, 0.0, 0.0, 0.5, 0.5});
        layout.views.append({QStringLiteral("elev-xz-0"), 50.0, true, 0.1, 0.55, 0.6, 0.4});
        loaded.extras.sheetLayouts.append(layout);
        const QString second = dir.filePath(QStringLiteral("s2.dat"));
        QString error;
        QVERIFY(service.save(second, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, &error));
        MemoryGateway gateway2;
        Loaded again;
        QVERIFY(load(service, gateway2, second, again));
        QCOMPARE(again.extras.sheetLayouts.size(), 1);
        const auto &back = again.extras.sheetLayouts.first();
        QCOMPARE(back.name, layout.name);
        QCOMPARE(back.views.size(), 2);
        QCOMPARE(back.views.at(1).viewId, QStringLiteral("elev-xz-0"));
        QVERIFY(back.views.at(1).manual && !back.views.at(0).manual);
        QCOMPARE(back.views.at(1).scale, 50.0);
        QVERIFY(std::abs(back.views.at(1).y - 0.55) < 1e-6 && std::abs(back.views.at(1).w - 0.6) < 1e-6);
    }

    void sheetsBuildMarksListAndExports()
    {
        using Structura::App::Services::SheetBuilder;
        SheetBuilder::Input in;
        // portico plano no plano y = 0: dois pilares e uma viga; mais um segundo portico em y = 6
        for (int f = 0; f < 2; ++f) {
            const double y = 6.0 * f;
            in.nodes.append({f * 10 + 1, 0, y, 0});
            in.nodes.append({f * 10 + 2, 0, y, 4});
            in.nodes.append({f * 10 + 3, 8, y, 4});
            in.nodes.append({f * 10 + 4, 8, y, 0});
        }
        auto bar = [&](int id, int a, int b, const char *section, int type = 0) {
            SheetBuilder::Bar x;
            x.id = id;
            x.nodeI = a;
            x.nodeJ = b;
            x.section = QString::fromLatin1(section);
            x.memberType = type;
            x.massPerMeter = 40.0;
            x.widthMm = 150.0;
            x.depthMm = 300.0;
            x.flangeMm = 10.0;
            x.webMm = 6.0;
            x.iShape = true;
            return x;
        };
        for (int f = 0; f < 2; ++f) {
            in.bars.append(bar(f * 10 + 1, f * 10 + 1, f * 10 + 2, "W 310 x 38.7"));
            in.bars.append(bar(f * 10 + 2, f * 10 + 2, f * 10 + 3, "W 360 x 44.6"));
            in.bars.append(bar(f * 10 + 3, f * 10 + 3, f * 10 + 4, "W 310 x 38.7"));
        }
        in.bars.append(bar(31, 2, 12, "W 250 x 25.3"));   // viga em Y
        in.bars.append(bar(32, 2, 4, "L 76 x 7.3", 2));    // tirante diagonal (no plano y = 0)
        in.info.project = QStringLiteral("Teste");
        const auto marks = SheetBuilder::assignMarks(in);
        QCOMPARE(marks.value(1), QStringLiteral("P1"));
        QCOMPARE(marks.value(1), marks.value(3));      // mesmo perfil e comprimento
        QCOMPARE(marks.value(1), marks.value(11));
        QVERIFY(marks.value(2).startsWith(QLatin1Char('V')));
        QCOMPARE(marks.value(2), marks.value(12));
        QVERIFY(marks.value(32).startsWith(QLatin1Char('T')));
        QVector<SheetBuilder::MarkRow> rows;
        SheetBuilder::assignMarks(in, &rows);
        int pillars = 0;
        for (const auto &r : rows) {
            if (r.mark == QLatin1String("P1")) {
                pillars = r.count;
            }
        }
        QCOMPARE(pillars, 4);

        const auto views = SheetBuilder::availableViews(in);
        bool plan = false, elev = false, list = false;
        for (const auto &v : views) {
            plan = plan || v.kind == SheetBuilder::ViewKind::PlanProjection;
            elev = elev || v.kind == SheetBuilder::ViewKind::ElevationXZ;
            list = list || v.kind == SheetBuilder::ViewKind::BarList;
        }
        QVERIFY(plan && elev && list);
        SheetBuilder::Options options;
        options.views = QSet<QString>();   // padrao
        const auto sheets = SheetBuilder::build(in, options);
        QVERIFY(sheets.size() >= 3);
        bool sawFolha = false, sawMark = false, sawDim = false;
        for (const auto &sheet : sheets) {
            QCOMPARE(sheet.width, 420.0);
            for (const auto &entity : sheet.entities) {
                if (const auto *t = std::get_if<Structura::Drawing::Text>(&entity)) {
                    sawFolha = sawFolha || t->text.find(" / " + std::to_string(sheets.size())) != std::string::npos;
                    sawMark = sawMark || t->text.rfind("P1", 0) == 0;
                    sawDim = sawDim || t->text == "8000";
                }
            }
        }
        QVERIFY(sawFolha);
        QVERIFY(sawMark);
        QVERIFY(sawDim);   // cota entre os pilares (8 m)
        // cota entre faces: 8000 - 2 x 75 (mesa de 150 mm dos pilares, ao longo de X)
        bool sawFace = false, sawFaceLayer = false;
        for (const auto &sheet : sheets) {
            for (std::size_t i = 0; i < sheet.layers.size(); ++i) {
                sawFaceLayer = sawFaceLayer || sheet.layers[i].name == "COTAS_FACES";
            }
            for (const auto &entity : sheet.entities) {
                if (const auto *t = std::get_if<Structura::Drawing::Text>(&entity)) {
                    sawFace = sawFace || t->text == "7850";
                }
            }
        }
        QVERIFY(sawFace && sawFaceLayer);
        // folha composta: duas vistas, cada uma com a sua escala
        {
            QString planId, elevId;
            for (const auto &v : views) {
                if (v.kind == SheetBuilder::ViewKind::PlanProjection && planId.isEmpty()) {
                    planId = v.id;
                }
                if (v.kind == SheetBuilder::ViewKind::ElevationXZ && elevId.isEmpty()) {
                    elevId = v.id;
                }
            }
            SheetBuilder::Options composed;
            composed.views.insert(QStringLiteral("__none__"));
            SheetBuilder::SheetLayout layout;
            layout.name = QStringLiteral("Composta");
            layout.views.append({planId, 100.0});
            layout.views.append({elevId, 50.0});
            composed.customSheets.append(layout);
            const auto one = SheetBuilder::build(in, composed);
            QCOMPARE(static_cast<int>(one.size()), 1);
            QCOMPARE(one.front().name, std::string("Composta"));
            bool s100 = false, s50 = false, indicated = false;
            for (const auto &entity : one.front().entities) {
                if (const auto *t = std::get_if<Structura::Drawing::Text>(&entity)) {
                    s100 = s100 || t->text == "ESC. 1:100";
                    s50 = s50 || t->text == "ESC. 1:50";
                    indicated = indicated || t->text == "INDICADA";
                }
            }
            QVERIFY(s100 && s50 && indicated);
        }
        // modo "linha de eixo": nenhuma cota entre faces desenhada como contorno; contorno gera polilinhas fechadas de barras
        SheetBuilder::Options outlineOff;
        outlineOff.profiles = SheetBuilder::ProfileDetail::Axis;
        outlineOff.faceDimensions = false;
        const auto axisSheets = SheetBuilder::build(in, outlineOff);
        auto closedPolylines = [](const std::vector<Structura::Drawing::Sheet> &list) {
            int n = 0;
            for (const auto &sheet : list) {
                for (const auto &entity : sheet.entities) {
                    if (const auto *p = std::get_if<Structura::Drawing::Polyline>(&entity)) {
                        n += p->closed && !p->filled && p->points.size() == 4 ? 1 : 0;
                    }
                }
            }
            return n;
        };
        QVERIFY(closedPolylines(sheets) > closedPolylines(axisSheets));
        const QString dxf = Structura::Drawing::sheetToDxf(sheets.front());
        QVERIFY(dxf.contains(QStringLiteral("LINE")) && dxf.endsWith(QStringLiteral("EOF\r\n")));
    }
    void titleBlockPersistsAndRendersInSheets()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("t1.dat"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(kSample);
        file.close();
        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, first, loaded));
        auto &tb = loaded.extras.titleBlock;
        tb.company = QStringLiteral("Estruturas Ltda");
        tb.client = QStringLiteral("Cliente S/A");
        tb.site = QStringLiteral("Galpão 3 – São Paulo");
        tb.project = QStringLiteral("Projeto 123");
        tb.author = QStringLiteral("Eng. Fulano");
        tb.registration = QStringLiteral("CREA 12345");
        tb.revision = QStringLiteral("B");
        tb.drawingPrefix = QStringLiteral("EST");
        tb.notes = QStringLiteral("1. Aço MR250.\n2. Cotas em mm.");
        tb.revisions = {{QStringLiteral("A"), QStringLiteral("01/01/2026"), QStringLiteral("Emissão inicial"), QStringLiteral("FS")},
                        {QStringLiteral("B"), QStringLiteral("10/02/2026"), QStringLiteral("Ajuste de perfis"), QStringLiteral("FS")}};
        tb.sheetTitles.insert(QStringLiteral("plan-all"), QStringLiteral("Locação geral"));
        const QString second = dir.filePath(QStringLiteral("t2.dat"));
        QString error;
        QVERIFY(service.save(second, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, &error));
        MemoryGateway gateway2;
        Loaded again;
        QVERIFY(load(service, gateway2, second, again));
        const auto &r = again.extras.titleBlock;
        QCOMPARE(r.company, tb.company);
        QCOMPARE(r.site, tb.site);
        QCOMPARE(r.registration, tb.registration);
        QCOMPARE(r.revision, QStringLiteral("B"));
        QCOMPARE(r.drawingPrefix, QStringLiteral("EST"));
        QCOMPARE(r.notes, tb.notes);
        QCOMPARE(r.revisions.size(), 2);
        QCOMPARE(r.revisions.at(1).description, QStringLiteral("Ajuste de perfis"));
        QCOMPARE(r.sheetTitles.value(QStringLiteral("plan-all")), QStringLiteral("Locação geral"));

        // o carimbo aparece nas pranchas (nº do desenho com prefixo, titulo personalizado, revisoes)
        using Structura::App::Services::SheetBuilder;
        SheetBuilder::Input in;
        in.nodes = {{1, 0, 0, 0}, {2, 0, 0, 4}, {3, 8, 0, 4}, {4, 8, 0, 0}};
        for (int i = 0; i < 3; ++i) {
            SheetBuilder::Bar b;
            b.id = i + 1;
            b.nodeI = i + 1;
            b.nodeJ = i + 2;
            b.section = QStringLiteral("W 310 x 38.7");
            b.massPerMeter = 38.7;
            in.bars.append(b);
        }
        in.info = r;
        const auto views = SheetBuilder::availableViews(in);
        QCOMPARE(views.first().title, QStringLiteral("Locação geral"));
        const auto sheets = SheetBuilder::build(in, {});
        QVERIFY(!sheets.empty());
        bool number = false, rev = false, notes = false, title = false;
        for (const auto &entity : sheets.front().entities) {
            if (const auto *t = std::get_if<Structura::Drawing::Text>(&entity)) {
                number = number || t->text == "EST-01";
                rev = rev || t->text.find("Ajuste de perfis") != std::string::npos;
                notes = notes || t->text == "2. Cotas em mm.";
                title = title || t->text.find("Locação geral") != std::string::npos;
            }
        }
        QVERIFY(number && rev && notes && title);
        const auto preview = SheetBuilder::previewTitleBlock(r);
        QVERIFY(preview.entities.size() > 30);
    }
    void sheetOutlineFollowsSectionEccentricity()
    {
        using Structura::App::Services::SheetBuilder;
        auto build = [](double offsetY) {
            SheetBuilder::Input in;
            in.nodes = {{1, 0, 0, 0}, {2, 0, 0, 4}, {3, 8, 0, 4}, {4, 8, 0, 0}};
            for (int i = 0; i < 3; ++i) {
                SheetBuilder::Bar b;
                b.id = i + 1;
                b.nodeI = i + 1;
                b.nodeJ = i + 2;
                b.section = QStringLiteral("S");
                b.widthMm = 150;
                b.depthMm = 300;
                if (i == 1) {   // viga: centroide deslocado para cima (eixo local y vertical) em ambas as extremidades
                    b.offsetIy = offsetY;
                    b.offsetJy = offsetY;
                }
                in.bars.append(b);
            }
            SheetBuilder::Options options;
            options.scaleDenominator = 100.0;
            options.dimensions = false;
            options.barLabels = false;
            options.faceDimensions = false;
            options.views = {QStringLiteral("elev-xz-0")};
            return SheetBuilder::build(in, options);
        };
        auto bandMinusAxis = [](const std::vector<Structura::Drawing::Sheet> &sheets) {
            const auto &sheet = sheets.front();
            // cota de papel do nivel z = 4000 (linha de eixo horizontal mais alta) e centro da faixa da viga (maior comprimento horizontal)
            double axisY = -1e300, bandY = 0.0, bandLength = 0.0;
            for (const auto &entity : sheet.entities) {
                if (const auto *line = std::get_if<Structura::Drawing::Line>(&entity)) {
                    if (sheet.layers[static_cast<std::size_t>(line->layer)].name == "EIXOS" && std::abs(line->a.y - line->b.y) < 1e-9) {
                        axisY = std::max(axisY, line->a.y);
                    }
                } else if (const auto *poly = std::get_if<Structura::Drawing::Polyline>(&entity)) {
                    if (poly->closed && poly->points.size() == 4 && sheet.layers[static_cast<std::size_t>(poly->layer)].name == "VIGAS") {
                        const double w = std::abs(poly->points[1].x - poly->points[0].x);
                        if (w > bandLength) {
                            bandLength = w;
                            bandY = (poly->points[0].y + poly->points[2].y) / 2.0;
                        }
                    }
                }
            }
            return bandY - axisY;
        };
        const double centred = bandMinusAxis(build(0.0));
        const double raised = bandMinusAxis(build(100.0));
        QVERIFY(std::abs(centred) < 1e-6);
        QVERIFY(std::abs(raised - 1.0) < 1e-6);   // 100 mm na escala 1:100 = 1 mm de papel
    }
    void hiddenLinesAndSheetDetails()
    {
        using namespace Structura::Drawing;
        // face quadrada na frente e uma aresta atras que a atravessa: so as pontas aparecem
        Camera3D camera = cameraFromAngles(0.0, 0.0, {0, 0, 0}, 1000.0);   // olho em -Y, olhando para +Y
        std::vector<Triangle3D> faces = {{{-10, 0, -10}, {10, 0, -10}, {10, 0, 10}}, {{-10, 0, -10}, {10, 0, 10}, {-10, 0, 10}}};
        std::vector<Edge3D> edges = {{{-20, 50, 0}, {20, 50, 0}, 7}};
        const auto result = computeHiddenLines(camera, faces, edges, false, 2000);
        QVERIFY(result.ok);
        double visible = 0.0;
        for (const auto &seg : result.segments) {
            QCOMPARE(seg.tag, 7);
            visible += std::abs(seg.x1 - seg.x0);
        }
        QVERIFY(std::abs(visible - 20.0) < 1.0);   // [-20,-10] e [10,20]
        const auto withHidden = computeHiddenLines(camera, faces, edges, true, 2000);
        double hidden = 0.0;
        for (const auto &seg : withHidden.segments) {
            hidden += seg.hidden ? std::abs(seg.x1 - seg.x0) : 0.0;
        }
        QVERIFY(std::abs(hidden - 20.0) < 1.0);
        // triangulacao de poligono nao convexo (L): area preservada
        const std::vector<std::array<double, 2>> poly = {{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
        const auto idx = triangulatePolygon(poly);
        QCOMPARE(static_cast<int>(idx.size()), 12);
        double area = 0.0;
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
            const auto &a = poly[static_cast<std::size_t>(idx[i])], &b = poly[static_cast<std::size_t>(idx[i + 1])], &c = poly[static_cast<std::size_t>(idx[i + 2])];
            area += std::abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2.0;
        }
        QVERIFY(std::abs(area - 3.0) < 1e-9);

        using Structura::App::Services::SheetBuilder;
        // perfil formado a frio com dobras: aparecem linhas de detalhe nas elevacoes; vista 3D gera linhas
        if (Structura::Visualization::SectionShapes::findFamily(QStringLiteral("C Formado")) == nullptr) {
            QSKIP("familia C Formado indisponivel");
        }
        SheetBuilder::Input in;
        in.nodes = {{1, 0, 0, 0}, {2, 0, 0, 4}, {3, 8, 0, 4}, {4, 8, 0, 0}};
        for (int i = 0; i < 3; ++i) {
            SheetBuilder::Bar b;
            b.id = i + 1;
            b.nodeI = i + 1;
            b.nodeJ = i + 2;
            b.section = QStringLiteral("Ue 200x75x25");
            b.family = QStringLiteral("C Formado");
            b.geometry = {{"d", 200}, {"bf", 75}, {"c", 25}, {"t", 3.0}};
            b.widthMm = 75;
            b.depthMm = 200;
            b.massPerMeter = 9.0;
            in.bars.append(b);
        }
        SheetBuilder::Options options;
        options.scaleDenominator = 10.0;   // escala grande: faixa larga o bastante para as linhas de dobra
        options.dimensions = false;
        options.faceDimensions = false;
        options.barLabels = false;
        options.views = {QStringLiteral("elev-xz-0")};
        auto countLayer = [](const std::vector<Structura::Drawing::Sheet> &sheets, const char *name) {
            int n = 0;
            for (const auto &sheet : sheets) {
                for (const auto &entity : sheet.entities) {
                    if (const auto *line = std::get_if<Structura::Drawing::Line>(&entity)) {
                        n += sheet.layers[static_cast<std::size_t>(line->layer)].name == name ? 1 : 0;
                    }
                }
            }
            return n;
        };
        options.profiles = SheetBuilder::ProfileDetail::OutlineWithFlanges;
        QVERIFY(countLayer(SheetBuilder::build(in, options), "DETALHES_PERFIL") >= 6);   // dobras (alma-mesa e mesa-enrijecedor) nas tres barras
        options.profiles = SheetBuilder::ProfileDetail::Outline;
        QCOMPARE(countLayer(SheetBuilder::build(in, options), "DETALHES_PERFIL"), 0);

        // cor e espessura das linhas
        {
            auto weightOf = [](const std::vector<Structura::Drawing::Sheet> &sheets, const char *layer) {
                for (const auto &layerDef : sheets.front().layers) {
                    if (layerDef.name == layer) {
                        return layerDef.weight;
                    }
                }
                return -1.0;
            };
            options.coloredLines = true;
            options.lineWeightScale = 1.0;
            const auto normal = SheetBuilder::build(in, options);
            QVERIFY(weightOf(normal, "VIGAS") > 0.0 && weightOf(normal, "VIGAS") <= 0.25);   // contorno em espessura normal de desenho
            options.lineWeightScale = 1.5;
            options.coloredLines = false;
            const auto thick = SheetBuilder::build(in, options);
            QVERIFY(std::abs(weightOf(thick, "VIGAS") - 1.5 * weightOf(normal, "VIGAS")) < 1e-9);
            for (const auto &layerDef : thick.front().layers) {
                QCOMPARE(layerDef.rgb[0] + layerDef.rgb[1] + layerDef.rgb[2], 0);
            }
            options.coloredLines = true;
            options.lineWeightScale = 1.0;
        }
        // vista 3D padrao
        SheetBuilder::Options o3;
        o3.views = {QStringLiteral("view3d-default")};
        o3.barLabels = false;
        const auto sheets3 = SheetBuilder::build(in, o3);
        QCOMPARE(static_cast<int>(sheets3.size()), 1);
        int lines3 = 0;
        for (const auto &entity : sheets3.front().entities) {
            lines3 += std::holds_alternative<Structura::Drawing::Line>(entity) ? 1 : 0;
        }
        QVERIFY(lines3 > 30);
        o3.hiddenLines3D = true;
        const int hiddenLines = countLayer(SheetBuilder::build(in, o3), "OCULTO");
        QVERIFY(hiddenLines > 0);
        // camera explicita em perspectiva
        Structura::App::ProjectData::SavedView3D saved;
        saved.name = QStringLiteral("persp");
        saved.explicitCamera = true;
        saved.perspective = true;
        saved.position = {14.0, -12.0, 9.0};
        saved.target = {4.0, 0.0, 2.0};
        in.views3d = {saved};
        o3.views = {QStringLiteral("view3d-0")};
        o3.hiddenLines3D = false;
        const auto persp = SheetBuilder::build(in, o3);
        QCOMPARE(static_cast<int>(persp.size()), 1);
        QVERIFY(persp.front().entities.size() > 40);
    }
    void solverEndReleaseGivesProppedCantilever()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.0e11, 8.0e10});
        model.sections.push_back({1, 0.01, 4e-6, 8e-6, 1e-5});
        SolverBridge::NodeData fixed;
        fixed.id = 1;
        fixed.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData prop;
        prop.id = 2;
        prop.x = 5.0;
        prop.restraints = {{1, 1, 1, 1, 0, 0}};   // apoio simples vertical (e fora do plano); gira livre
        model.nodes = {fixed, prop};
        SolverBridge::MemberData member {1, 1, 2, 1, 1};
        member.releases[11] = 1;                     // rz no no J (flexao no plano x-y local = x-Z global)
        model.members.push_back(member);
        SolverBridge::MemberLoadData load;
        load.memberId = 1;
        load.coordSystem = SolverBridge::MemberLoadCoordSystem::Global;
        load.qz = -1000.0;
        model.memberLoads.push_back(load);

        const auto result = SolverBridge(dll).solve(model);
        // Viga engastada-apoiada: R_engaste = 5qL/8, M_engaste = qL^2/8, R_apoio = 3qL/8.
        QVERIFY(std::abs(result.nodeResults[0].reactions[2] - 3125.0) < 1e-6);
        QVERIFY(std::abs(std::abs(result.nodeResults[0].reactions[4]) - 3125.0) < 1e-6);
        QVERIFY(std::abs(result.nodeResults[1].reactions[2] - 1875.0) < 1e-6);
        // O momento no no articulado e nulo nos esforcos da barra (Mz em J, indice 11).
        QVERIFY(std::abs(result.memberResults[0].localForces[11]) < 1e-6);
    }
    void exampleGalpaoLoadsAndSolves()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        const QString example = QStringLiteral(STRUCTURA_SOURCE_DIR "/Examples/galpao_3_andares.dat");
        if (!QFile::exists(dll) || !QFile::exists(example)) {
            QSKIP("solver_dll.dll ou exemplo nao encontrados.");
        }
        DatModelIoService service;
        MemoryGateway gateway;
        Loaded loaded;
        QVERIFY(load(service, gateway, example, loaded));
        QCOMPARE(gateway.nodeInfos().size(), std::size_t(60));
        QCOMPARE(gateway.bars().size(), std::size_t(123));
        QCOMPARE(loaded.extras.placements.size(), 123);
        QCOMPARE(loaded.extras.loadCases.size(), 4);

        AnalysisService analysis;
        for (const QString &loadCase : {QStringLiteral("PP"), QStringLiteral("CP"), QStringLiteral("SC"), QStringLiteral("VT")}) {
            const auto build = analysis.buildModel(&gateway, loaded.materials, loaded.sections, loaded.supports,
                                                   loaded.nodal, loaded.member, loaded.extras, loadCase);
            QVERIFY2(build.success, qPrintable(build.errors.join(QLatin1Char(';'))));
            const auto result = SolverBridge(dll).solve(build.model);

            double sumFx = 0.0;
            double sumFz = 0.0;
            for (const auto &node : result.nodeResults) {
                sumFx += node.reactions[0];
                sumFz += node.reactions[2];
            }
            double appliedFx = 0.0;
            double appliedFz = 0.0;
            for (const auto &load : build.model.nodalLoads) {
                appliedFx += load.values[0];
                appliedFz += load.values[2];
            }
            // Cargas distribuidas (globais em -Z): q * comprimento de cada barra.
            QHash<int, double> length;
            QHash<int, std::array<double, 3>> position;
            for (const auto &node : build.model.nodes) {
                position.insert(node.id, {node.x, node.y, node.z});
            }
            for (const auto &member : build.model.members) {
                const auto a = position.value(member.nodeI);
                const auto b = position.value(member.nodeJ);
                length.insert(member.id, std::sqrt(std::pow(b[0] - a[0], 2) + std::pow(b[1] - a[1], 2) + std::pow(b[2] - a[2], 2)));
            }
            for (const auto &load : build.model.memberLoads) {
                appliedFz += load.qz * length.value(load.memberId);
            }
            // Equilibrio global: soma das reacoes + cargas aplicadas = 0.
            QVERIFY2(std::abs(sumFx + appliedFx) < 1e-3 * std::max(1.0, std::abs(appliedFx)), qPrintable(loadCase));
            QVERIFY2(std::abs(sumFz + appliedFz) < 1e-3 * std::max(1.0, std::abs(appliedFz)), qPrintable(loadCase));
        }
    }
    void memberEndForcesSatisfyEquilibriumUnderDistributedLoad()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.0e11, 8.0e10});
        model.sections.push_back({1, 0.01, 4e-6, 8e-6, 1e-5});
        SolverBridge::NodeData fixed;
        fixed.id = 1;
        fixed.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData tip;
        tip.id = 2;
        tip.x = 5.0;
        model.nodes = {fixed, tip};
        model.members.push_back({1, 1, 2, 1, 1});
        SolverBridge::MemberLoadData load;
        load.memberId = 1;
        load.coordSystem = SolverBridge::MemberLoadCoordSystem::Local;
        load.qz = -1000.0;
        model.memberLoads.push_back(load);

        const auto forces = SolverBridge(dll).solve(model).memberResults[0].localForces;
        // Balanco com q = -1000 N/m em z local: engaste Vz = qL = 5000, My = -qL^2/2 = -12500; ponta livre = 0.
        QVERIFY(std::abs(forces[2] - 5000.0) < 1e-6);
        QVERIFY(std::abs(forces[4] + 12500.0) < 1e-6);
        QVERIFY(std::abs(forces[8]) < 1e-6);
        QVERIFY(std::abs(forces[10]) < 1e-6);
        // Equilibrio da barra: soma das forcas de extremidade + carga = 0.
        QVERIFY(std::abs(forces[2] + forces[8] + load.qz * 5.0) < 1e-6);
    }
};

QTEST_APPLESS_MAIN(TestProjectIoAndSolver)
#include "TestProjectIoAndSolver.moc"
