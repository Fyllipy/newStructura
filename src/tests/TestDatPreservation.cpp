#include <QtTest/QtTest>

#include "app/ISceneModelGateway.h"
#include "app/services/DatModelIoService.h"

#include <QFile>
#include <QTemporaryDir>

#include <deque>
#include <functional>

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

bool load(const DatModelIoService &service, const QString &path, Loaded &out)
{
    MemoryGateway gateway;
    QString error;
    QStringList warnings;
    return service.load(path, &gateway, out.materials, out.sections, out.supports, out.nodal, out.member, out.extras, out.lastMaterial, out.lastSection,
                        &error, &warnings);
}

/// Carrega com o servico dado e regrava (as edicoes se encontram aqui: a Comercial grava o que a Demonstrativa preservou).
bool resave(const DatModelIoService &service, const QString &from, const QString &to, const std::function<void(Loaded &)> &edit = {})
{
    MemoryGateway gateway;
    Loaded loaded;
    QString error;
    QStringList warnings;
    if (!service.load(from, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, loaded.lastMaterial,
                      loaded.lastSection, &error, &warnings)) {
        return false;
    }
    if (edit) {
        edit(loaded);
    }
    return service.save(to, &gateway, loaded.materials, loaded.sections, loaded.supports, loaded.nodal, loaded.member, loaded.extras, &error);
}

QString readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll()) : QString();
}

/// Projeto da edicao Comercial: dimensionamento, ligacoes, detalhamento, resultados derivados e itens que nenhuma versao conhece.
const char *kCommercialProject = R"(
[MATERIALS]
1 2.1e11 8.1e10

[MATERIAL_PROPS]
1 78.5

[SECTIONS]
1 0.0049 3.4e-5 7.2e-6 1.0e-7

[SECTION_GEOMETRY]
1 W_Laminado 0 d=310 bf=165 tw=5.8 tf=9.7 r=10

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

[DESIGN_SETTINGS]
DEFAULT_STEEL MR250
RECT_TUBE_STANDARD AUTO
AMPLIFY_B1 0
SECOND_ORDER 1
VON_MISES 1
FIRE_CHECK 1
FIRE_TRRF 90
TARGET_ETA 0.85
GROUP_BY_FAMILY 1
COMBINATION ELU
CHAVE_DO_FUTURO 7 abc
DEFAULTS 0.8 0.9 1 1 0 0 0 1 1

[MEMBER_DESIGN]
1 ASTM_A572_Gr.50 0.8 0.9 1 3 2.5 1 0 1.1 0.9

[CONNECTIONS]
1 0 2 1 1 0.55 1 ASTM_A325 Governante bolts=4 tp=9.5

[COMPOSITE_FLOOR]
BEAM 1 hc 100
DECK tcr 75

[SHEET_LAYOUTS]
LAYOUT Prancha_1
EDIT 1 chave 1.5 2.5 0

[TITLE_BLOCK]
COMPANY Empresa_X

[NONLINEAR]
force=1 steps=5 ficha_futura=9

[ANALYSIS_RESULTS]
FINGERPRINT abc123
RUN ELU
N 1 0 0 0 0 0 0 1 2 3 0 0 0
CHAVE_DE_RESULTADO 1 2 3

[DESIGN_RESULTS]
1 W_310 0.55 Flexao ELU 4

[BUCKLING_RESULTS]
COMBINATION ELU
FACTOR 3.2

[MODAL_RESULTS]
SOURCE massa
MODE 10 0.5 0.1 0

[SECAO_DO_FUTURO]
# comentario preservado
alfa 1 2 3
beta gama
)";

QString writeProject(const QTemporaryDir &dir, const QString &name, const char *content)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(content);
    }
    return path;
}

} // namespace

class TestDatPreservation : public QObject
{
    Q_OBJECT

private slots:
    void commercialKeepsSectionsAndKeysItDoesNotKnow()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = writeProject(dir, QStringLiteral("a.dat"), kCommercialProject);
        const QString saved = dir.filePath(QStringLiteral("b.dat"));
        DatModelIoService commercial(true);
        QVERIFY(resave(commercial, source, saved));

        const QString text = readAll(saved);
        QVERIFY(text.contains(QStringLiteral("[SECAO_DO_FUTURO]")));
        QVERIFY(text.contains(QStringLiteral("# comentario preservado")));
        QVERIFY(text.contains(QStringLiteral("alfa 1 2 3")));
        QVERIFY(text.contains(QStringLiteral("beta gama")));
        QVERIFY(text.contains(QStringLiteral("CHAVE_DO_FUTURO 7 abc")));
        QVERIFY(text.contains(QStringLiteral("ficha_futura=9")));
        QVERIFY(text.contains(QStringLiteral("CHAVE_DE_RESULTADO 1 2 3")));

        // e os dados que a Comercial conhece continuam interpretados
        Loaded again;
        QVERIFY(load(commercial, saved, again));
        QVERIFY(again.extras.design.vonMises);
        QCOMPARE(again.extras.design.fireTrrf, 90.0);
        QCOMPARE(again.extras.design.targetEta, 0.85);
        QCOMPARE(again.extras.design.members.size(), 1);
        QCOMPARE(again.extras.connections.size(), 1);
        QCOMPARE(again.extras.results.design.size(), 1);
        QVERIFY(again.extras.nonlinear.force);
    }

    void demoDoesNotInterpretDesignDataButKeepsItIntact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = writeProject(dir, QStringLiteral("a.dat"), kCommercialProject);
        const QString demoSaved = dir.filePath(QStringLiteral("demo.dat"));
        DatModelIoService demo(false);
        DatModelIoService commercial(true);

        // a Demo nao carrega nada de dimensionamento/ligacoes/detalhamento na memoria ...
        Loaded asDemo;
        QVERIFY(load(demo, source, asDemo));
        QVERIFY(asDemo.extras.design.members.isEmpty());
        QVERIFY(asDemo.extras.connections.isEmpty());
        QVERIFY(asDemo.extras.compositeBeams.isEmpty());
        QVERIFY(asDemo.extras.sheetLayouts.isEmpty());
        QVERIFY(asDemo.extras.results.design.isEmpty());
        QVERIFY(asDemo.extras.results.bucklingFactors.isEmpty());
        QVERIFY(!asDemo.extras.design.vonMises);
        QVERIFY(asDemo.extras.design.secondOrder);   // ... mas as opcoes da analise sim
        QVERIFY(asDemo.extras.nonlinear.force);

        // ... e ao salvar devolve tudo ao arquivo: a Comercial reabre o projeto sem perder nada
        QVERIFY(resave(demo, source, demoSaved));
        Loaded original, afterDemo;
        QVERIFY(load(commercial, source, original));
        QVERIFY(load(commercial, demoSaved, afterDemo));
        QVERIFY(afterDemo.extras.design.vonMises);
        QVERIFY(afterDemo.extras.design.fireCheck);
        QCOMPARE(afterDemo.extras.design.fireTrrf, 90.0);
        QCOMPARE(afterDemo.extras.design.targetEta, 0.85);
        QVERIFY(afterDemo.extras.design.groupByFamily);
        QVERIFY(!afterDemo.extras.design.amplifyB1);
        QCOMPARE(afterDemo.extras.design.combinations, original.extras.design.combinations);
        QCOMPARE(afterDemo.extras.design.defaults.kx, 0.8);
        QCOMPARE(afterDemo.extras.design.members.size(), 1);
        QCOMPARE(afterDemo.extras.design.members.first().kx, 0.8);
        QCOMPARE(afterDemo.extras.design.members.first().steel, original.extras.design.members.first().steel);
        QCOMPARE(afterDemo.extras.connections.size(), 1);
        QCOMPARE(afterDemo.extras.connections.first().values.value(QStringLiteral("bolts")), 4.0);
        QCOMPARE(afterDemo.extras.compositeBeams.value(1).value(QStringLiteral("hc")), 100.0);
        QCOMPARE(afterDemo.extras.deckSlab.value(QStringLiteral("tcr")), 75.0);
        QCOMPARE(afterDemo.extras.sheetLayouts.size(), 1);
        QCOMPARE(afterDemo.extras.sheetEdits.size(), 1);
        QCOMPARE(afterDemo.extras.results.design.size(), 1);
        QCOMPARE(afterDemo.extras.results.bucklingFactors.size(), 1);
        QCOMPARE(afterDemo.extras.results.modalOmega.size(), 1);
        QCOMPARE(afterDemo.extras.results.fingerprint, QStringLiteral("abc123"));

        // inclusive o que nenhuma edicao conhece
        const QString text = readAll(demoSaved);
        QVERIFY(text.contains(QStringLiteral("[SECAO_DO_FUTURO]")));
        QVERIFY(text.contains(QStringLiteral("CHAVE_DO_FUTURO 7 abc")));
        QVERIFY(text.contains(QStringLiteral("ficha_futura=9")));
    }

    void demoEditsOnlyAnalysisSettings()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = writeProject(dir, QStringLiteral("a.dat"), kCommercialProject);
        const QString saved = dir.filePath(QStringLiteral("demo.dat"));
        DatModelIoService demo(false);
        QVERIFY(resave(demo, source, saved, [](Loaded &loaded) {
            loaded.extras.design.secondOrder = false;
            loaded.extras.design.stabilityFunctions = true;
            loaded.extras.design.rigidZoneFactor = 0.5;
        }));

        Loaded again;
        QVERIFY(load(DatModelIoService(true), saved, again));
        QVERIFY(!again.extras.design.secondOrder);
        QVERIFY(again.extras.design.stabilityFunctions);
        QCOMPARE(again.extras.design.rigidZoneFactor, 0.5);
        // as opcoes de dimensionamento ficaram como estavam
        QVERIFY(again.extras.design.vonMises);
        QCOMPARE(again.extras.design.fireTrrf, 90.0);
        QCOMPARE(again.extras.design.members.size(), 1);
        QCOMPARE(again.extras.connections.size(), 1);
    }

    void demoDropsDerivedResultsWhenAnalysisIsRedone()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = writeProject(dir, QStringLiteral("a.dat"), kCommercialProject);
        const QString saved = dir.filePath(QStringLiteral("demo.dat"));
        DatModelIoService demo(false);
        QVERIFY(resave(demo, source, saved, [](Loaded &loaded) { loaded.extras.results.fingerprint = QStringLiteral("analise-refeita"); }));

        Loaded again;
        QVERIFY(load(DatModelIoService(true), saved, again));
        // resultados de dimensionamento, flambagem e modal descreviam a analise antiga: nao sao mais gravados
        QVERIFY(again.extras.results.design.isEmpty());
        QVERIFY(again.extras.results.bucklingFactors.isEmpty());
        QVERIFY(again.extras.results.modalOmega.isEmpty());
        // os dados de entrada do dimensionamento permanecem
        QCOMPARE(again.extras.design.members.size(), 1);
        QCOMPARE(again.extras.connections.size(), 1);
        QCOMPARE(again.extras.compositeBeams.size(), 1);
    }

    void projectWithoutDesignDataStaysWithoutIt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = writeProject(dir, QStringLiteral("a.dat"),
                                            "[MATERIALS]\n1 2.1e11 8.1e10\n[SECTIONS]\n1 0.01 1e-4 1e-4 1e-4\n"
                                            "[NODES]\n1 0 0 0 1 1 1 1 1 1\n2 3 0 0 0 0 0 0 0 0\n[MEMBERS]\n1 1 2 1 1\n");
        const QString saved = dir.filePath(QStringLiteral("demo.dat"));
        QVERIFY(resave(DatModelIoService(false), source, saved));
        const QString text = readAll(saved);
        QVERIFY(!text.contains(QStringLiteral("[DESIGN_SETTINGS]")));
        QVERIFY(!text.contains(QStringLiteral("[MEMBER_DESIGN]")));
        QVERIFY(!text.contains(QStringLiteral("[CONNECTIONS]")));
    }
};

QTEST_APPLESS_MAIN(TestDatPreservation)
#include "TestDatPreservation.moc"
