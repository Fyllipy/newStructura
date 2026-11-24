// Torre de telecomunicacoes autoportante (docs/validacao_mecway/torre/torre_telecom.dat): carrega o ARQUIVO pelo mesmo caminho do aplicativo (DatModelIoService ->
// AnalysisService::buildModel por caso -> CombinationService::mergeModels -> rds.dll) e roda estatica, flambagem e modal. Verificacoes de equilibrio e de sanidade
// (a comparacao com o Mecway esta em docs/validacao_mecway/torre). Se STRUCTURA_TOWER_OUT apontar para uma pasta, os resultados sao gravados em CSV para a comparacao.
#include <QtTest/QtTest>

#include "app/ISceneModelGateway.h"
#include "app/BarFrame.h"
#include "app/SolverBridge.h"
#include "app/services/AnalysisService.h"
#include "app/services/CombinationService.h"
#include "app/services/DatModelIoService.h"

#include <cmath>
#include <deque>
#include <fstream>

using namespace Structura::App;
using namespace Structura::App::Services;

namespace {

QString solverLibrary()
{
    const QString enc = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.enc");
    if (QFile::exists(enc)) {
        return enc;
    }
    return QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
}

/// Gateway minimo em memoria (sem VTK).
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

} // namespace

class TestTowerModel : public QObject
{
    Q_OBJECT

private slots:
    void init() { Structura::App::BarFrame::setLegacy(false); }

    void towerFromDatFileSolvesStaticBucklingAndModal()
    {
        const QString path = QStringLiteral(STRUCTURA_SOURCE_DIR "/docs/validacao_mecway/torre/torre_telecom.dat");
        QVERIFY2(QFile::exists(path), qPrintable(path));
        DatModelIoService service;
        MemoryGateway gateway;
        QVector<ProjectData::MaterialInfo> materials;
        QVector<ProjectData::SectionInfo> sections;
        QVector<ProjectData::NodeSupport> supports;
        QVector<ProjectData::NodalLoad> nodal;
        QVector<ProjectData::MemberLoad> member;
        ProjectData::ProjectExtras extras;
        QUuid lastMaterial, lastSection;
        QString error;
        QStringList warnings;
        QVERIFY2(service.load(path, &gateway, materials, sections, supports, nodal, member, extras, lastMaterial, lastSection, &error, &warnings), qPrintable(error));
        QCOMPARE(gateway.nodeInfos().size(), std::size_t(84));
        QCOMPARE(gateway.bars().size(), std::size_t(244));
        QCOMPARE(sections.size(), 5);
        // perfis L e C (U laminado) lidos com a geometria
        int angles = 0, channels = 0;
        for (const auto &s : sections) {
            angles += s.family == QLatin1String("L Laminado") ? 1 : 0;
            channels += s.family == QLatin1String("U Laminado") ? 1 : 0;
        }
        QCOMPARE(angles, 4);
        QCOMPARE(channels, 1);

        AnalysisService analysis;
        const auto cases = CombinationService::effectiveCases(extras);
        QMap<QString, SolverBridge::ModelData> caseModels;
        for (const auto &loadCase : cases) {
            const auto build = analysis.buildModel(&gateway, materials, sections, supports, nodal, member, extras, loadCase.name);
            QVERIFY2(build.success, qPrintable(build.errors.join('\n')));
            caseModels.insert(loadCase.name, build.model);
        }
        QVERIFY(caseModels.contains(QStringLiteral("PP")) && caseModels.contains(QStringLiteral("VENTO")) && caseModels.contains(QStringLiteral("ANT")));
        std::vector<std::pair<const SolverBridge::ModelData *, double>> terms;
        for (auto it = caseModels.constBegin(); it != caseModels.constEnd(); ++it) {
            terms.emplace_back(&it.value(), 1.0);
        }
        SolverBridge::ModelData model = CombinationService::mergeModels(terms);
        const SolverBridge solver(solverLibrary());

        // ---- estatica linear: equilibrio global (peso proprio + antenas + vento) ----
        const SolverBridge::Result stat = solver.solve(model);
        QVERIFY(!stat.nodeResults.empty());
        double rx = 0.0, ry = 0.0, rz = 0.0;
        for (const auto &n : stat.nodeResults) {
            rx += n.reactions[0];
            ry += n.reactions[1];
            rz += n.reactions[2];
        }
        double weight = 0.0, fx = 0.0, fy = 0.0, fz = 0.0;
        std::map<int, std::array<double, 3>> pos;
        for (const auto &n : model.nodes) {
            pos[n.id] = {n.x, n.y, n.z};
        }
        std::map<int, double> sectionArea;
        for (const auto &s : model.sections) {
            sectionArea[s.id] = s.area;
        }
        for (const auto &m : model.members) {
            const auto &a = pos[m.nodeI];
            const auto &b = pos[m.nodeJ];
            weight += sectionArea[m.sectionId] * std::sqrt(std::pow(b[0] - a[0], 2) + std::pow(b[1] - a[1], 2) + std::pow(b[2] - a[2], 2)) * 76.982202;
        }
        for (const auto &l : model.nodalLoads) {
            fx += l.values[0];
            fy += l.values[1];
            fz += l.values[2];
        }
        QVERIFY2(std::abs(rx + fx) < 1e-6 * std::max(1.0, std::abs(fx)), qPrintable(QString("Rx %1 Fx %2").arg(rx).arg(fx)));
        QVERIFY(std::abs(ry + fy) < 1e-6);
        QVERIFY2(std::abs(rz + fz - weight) < 2e-3 * weight, qPrintable(QString("Rz %1, cargas %2, peso %3").arg(rz).arg(fz).arg(weight)));   // as reacoes equilibram peso + antenas (2 casas: peso dos perfis lido da tabela)

        // ---- flambagem linear e modal ----
        SolverBridge::ModelData buckling = model;
        buckling.bucklingModes = 4;
        const SolverBridge::Result b = solver.solve(buckling);
        QVERIFY(!b.bucklingFactors.empty());
        QVERIFY(b.bucklingFactors.front() > 0.0);
        SolverBridge::ModelData modal = model;
        modal.modalModes = 6;
        const SolverBridge::Result m = solver.solve(modal);
        QVERIFY(m.modalOmega.size() >= 2);
        QVERIFY(m.modalOmega.front() > 0.0);
        const double f1 = m.modalOmega.front() / (2.0 * 3.14159265358979323846);
        QVERIFY2(f1 > 0.5 && f1 < 40.0, qPrintable(QString::number(f1)));   // torre de 30 m: primeiras frequencias de poucos Hz

        const QByteArray outEnv = qgetenv("STRUCTURA_TOWER_OUT");
        if (outEnv.isEmpty()) {
            return;
        }
        const QString dir = QString::fromLocal8Bit(outEnv);
        auto open = [&](const char *name) {
            auto f = std::make_unique<std::ofstream>(QDir(dir).filePath(QString::fromLatin1(name)).toStdString());
            f->precision(17);
            return f;
        };
        {
            auto f = open("sections_effective.csv");
            for (const auto &s : model.sections) {
                *f << s.id << ',' << s.area << ',' << s.iy << ',' << s.iz << ',' << s.j << '\n';
            }
            auto mm = open("members_effective.csv");
            for (const auto &mb : model.members) {
                *mm << mb.id << ',' << mb.nodeI << ',' << mb.nodeJ << ',' << mb.sectionId << ',' << mb.sectionRotationDeg << ',' << mb.offsetY1 << ',' << mb.offsetZ1 << ',' << mb.offsetY2 << ',' << mb.offsetZ2 << ','
                    << (mb.tensionOnly ? 1 : 0) << ',' << mb.rigidStart << ',' << mb.rigidEnd << '\n';
            }
            auto mat = open("material_effective.csv");
            *mat << model.materials.front().e << ',' << model.materials.front().g << ',' << model.materials.front().density << '\n';
        }
        {
            auto f = open("static_nodes.csv");
            for (const auto &n : stat.nodeResults) {
                const auto &p = pos[n.nodeId];
                *f << n.nodeId << ',' << p[0] << ',' << p[1] << ',' << p[2];
                for (int k = 0; k < 6; ++k) {
                    *f << ',' << n.displacements[static_cast<std::size_t>(k)];
                }
                for (int k = 0; k < 6; ++k) {
                    *f << ',' << n.reactions[static_cast<std::size_t>(k)];
                }
                *f << '\n';
            }
            auto fm = open("static_members.csv");
            for (const auto &mr : stat.memberResults) {
                *fm << mr.memberId;
                for (const double v : mr.localForces) {
                    *fm << ',' << v;
                }
                *fm << '\n';
            }
        }
        {
            auto f = open("buckling.csv");
            for (std::size_t k = 0; k < b.bucklingFactors.size(); ++k) {
                *f << "fator" << k + 1 << '=' << b.bucklingFactors[k] << '\n';
                auto shape = open((std::string("buckling_mode") + std::to_string(k + 1) + ".csv").c_str());
                for (std::size_t i = 0; i < buckling.nodes.size(); ++i) {
                    *shape << buckling.nodes[i].id << ',' << buckling.nodes[i].x << ',' << buckling.nodes[i].y << ',' << buckling.nodes[i].z;
                    for (std::size_t c = 0; c < 6; ++c) {
                        *shape << ',' << b.bucklingModes[k][6 * i + c];
                    }
                    *shape << '\n';
                }
            }
            auto fm = open("modal.csv");
            for (std::size_t k = 0; k < m.modalOmega.size(); ++k) {
                *fm << 'f' << k + 1 << '=' << m.modalOmega[k] / (2.0 * 3.14159265358979323846) << '\n';
                auto shape = open((std::string("modal_mode") + std::to_string(k + 1) + ".csv").c_str());
                for (std::size_t i = 0; i < modal.nodes.size(); ++i) {
                    *shape << modal.nodes[i].id << ',' << modal.nodes[i].x << ',' << modal.nodes[i].y << ',' << modal.nodes[i].z;
                    for (std::size_t c = 0; c < 6; ++c) {
                        *shape << ',' << m.modalShapes[k][6 * i + c];
                    }
                    *shape << '\n';
                }
            }
        }
    }
};

QTEST_MAIN(TestTowerModel)
#include "TestTowerModel.moc"
