#include "MainWindow.h"

#include "WindShedDialog.h"
#include "WindStaticDialog.h"
#include "app/services/DesignService.h"
#include "viz/SceneController.h"

#include <QHash>
#include <QMessageBox>

#include <algorithm>
#include <cmath>
#include <limits>

using Structura::App::Services::WindShedService;
using Structura::App::Services::WindStaticService;
using Structura::App::Services::DesignService;

namespace {

constexpr double kLevelTolerance = 1e-3;   // m

struct LevelNode {
    int id {0};
    double x {0.0}, y {0.0};
};

} // namespace

void MainWindow::onWindStatic()
{
    if (!m_sceneController) {
        return;
    }
    const auto nodes = m_sceneController->nodeInfos();
    if (nodes.size() < 2) {
        QMessageBox::information(this, tr("Vento estático"), tr("Modele a estrutura (nós e barras) antes de gerar o vento."));
        return;
    }
    // planta e cotas detectadas: niveis com ao menos dois nos (o menor e a base)
    double xMin = std::numeric_limits<double>::max(), xMax = std::numeric_limits<double>::lowest();
    double yMin = xMin, yMax = xMax;
    QMap<qint64, int> levelCount;
    for (const auto &node : nodes) {
        xMin = std::min(xMin, node.x);
        xMax = std::max(xMax, node.x);
        yMin = std::min(yMin, node.y);
        yMax = std::max(yMax, node.y);
        ++levelCount[std::llround(node.z * 1000.0)];
    }
    QVector<double> levels;
    for (auto it = levelCount.constBegin(); it != levelCount.constEnd(); ++it) {
        if (it.value() >= 2 || levels.isEmpty()) {
            levels.append(static_cast<double>(it.key()) / 1000.0);
        }
    }
    if (m_extras.axes.levels.size() >= 2) {
        // niveis nomeados do projeto (eixos de referencia) sobrepoem a deteccao
        QVector<double> named;
        for (const auto &level : m_extras.axes.levels) {
            named.append(level.elevation);
        }
        std::sort(named.begin(), named.end());
        levels = named;
    }

    Structura::UI::WindStaticDialog dialog({xMin, xMax, yMin, yMax}, levels, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto result = dialog.result();
    if (result.cases.isEmpty()) {
        QMessageBox::warning(this, tr("Vento estático"), result.warnings.isEmpty() ? tr("Nenhum caso gerado.") : result.warnings.join(QLatin1Char('\n')));
        return;
    }
    const auto building = dialog.building();
    const double forceFactor = DesignService::unitFactors(m_materials).first;   // N por unidade de forca do modelo
    const double toModel = 1000.0 / forceFactor;   // kN -> unidade de forca do modelo

    StateGuard guard(this, tr("Vento estático"));
    QStringList notes = result.warnings;
    int createdLoads = 0;
    const double xCenter = 0.5 * (building.xMin + building.xMax), yCenter = 0.5 * (building.yMin + building.yMax);

    for (const auto &c : result.cases) {
        // caso de carga (criado ou refeito)
        const bool exists = std::any_of(m_extras.loadCases.begin(), m_extras.loadCases.end(), [&](const auto &lc) { return lc.name == c.name; });
        if (!exists) {
            QString abbreviation = c.name;
            abbreviation.replace(QStringLiteral("VENTO "), QStringLiteral("V"));
            m_extras.loadCases.append({c.name, abbreviation.remove(QLatin1Char(' ')), false});
        }
        for (int i = m_nodalLoads.size() - 1; i >= 0; --i) {
            if (m_nodalLoads[i].loadCase == c.name) {
                m_nodalLoads.removeAt(i);
            }
        }
        const bool alongX = c.direction == WindStaticService::PlusX || c.direction == WindStaticService::MinusX;
        const double sign = (c.direction == WindStaticService::PlusX || c.direction == WindStaticService::PlusY) ? 1.0 : -1.0;
        for (const auto &floor : c.floors) {
            const double zAbs = building.levels.first() + floor.z;
            const double force = sign * floor.force;   // kN com o sinal da direcao
            QVector<LevelNode> atLevel;
            for (const auto &node : nodes) {
                if (std::abs(node.z - zAbs) <= kLevelTolerance && node.x >= building.xMin - kLevelTolerance && node.x <= building.xMax + kLevelTolerance
                    && node.y >= building.yMin - kLevelTolerance && node.y <= building.yMax + kLevelTolerance) {
                    atLevel.append({node.externalId, node.x, node.y});
                }
            }
            if (atLevel.isEmpty()) {
                notes << tr("Cota %1 m: nenhum nó na planta; a força do pavimento (%2 kN) não foi aplicada.").arg(zAbs, 0, 'f', 2).arg(floor.force, 0, 'f', 1);
                continue;
            }
            // linha de acao da resultante: centro da planta deslocado de e na direcao transversal
            const double lineTransverse = (alongX ? yCenter : xCenter) + c.eccentricityDistance;
            auto record = [&](int nodeId) -> NodalLoad & {
                for (auto &load : m_nodalLoads) {
                    if (load.nodeId == nodeId && load.loadCase == c.name) {
                        return load;
                    }
                }
                NodalLoad load;
                load.nodeId = nodeId;
                load.loadCase = c.name;
                m_nodalLoads.append(load);
                return m_nodalLoads.last();
            };
            // diafragma do pavimento: forca e momento da excentricidade no no mestre
            const Structura::App::ProjectData::RigidDiaphragm *diaphragm = nullptr;
            const LevelNode *master = nullptr;
            for (const auto &d : m_extras.diaphragms) {
                if (d.normalAxis != 2) {
                    continue;
                }
                for (const auto &n : atLevel) {
                    if (n.id == d.masterNode) {
                        diaphragm = &d;
                        master = &n;
                    }
                }
            }
            if (diaphragm && master) {
                NodalLoad &load = record(master->id);
                if (alongX) {
                    load.fx += force * toModel;
                    load.mz += -(lineTransverse - master->y) * force * toModel;
                } else {
                    load.fy += force * toModel;
                    load.mz += (lineTransverse - master->x) * force * toModel;
                }
                ++createdLoads;
                continue;
            }
            // sem diafragma: reparte entre os nos do nivel pela largura tributaria na direcao transversal e corrige a linha de acao com um binario nos nos extremos
            std::sort(atLevel.begin(), atLevel.end(), [&](const LevelNode &a, const LevelNode &b) { return (alongX ? a.y : a.x) < (alongX ? b.y : b.x); });
            const int n = static_cast<int>(atLevel.size());
            QVector<double> weight(n, 0.0);
            auto t = [&](int i) { return alongX ? atLevel[i].y : atLevel[i].x; };
            for (int i = 0; i < n; ++i) {
                const double left = i > 0 ? 0.5 * (t(i) - t(i - 1)) : 0.0;
                const double right = i + 1 < n ? 0.5 * (t(i + 1) - t(i)) : 0.0;
                weight[i] = left + right;
            }
            double total = 0.0, centroid = 0.0;
            for (int i = 0; i < n; ++i) {
                total += weight[i];
                centroid += weight[i] * t(i);
            }
            if (total <= 0.0) {
                weight.fill(1.0);
                total = n;
                centroid = 0.0;
                for (int i = 0; i < n; ++i) {
                    centroid += t(i);
                }
            }
            centroid /= total;
            for (int i = 0; i < n; ++i) {
                NodalLoad &load = record(atLevel[i].id);
                (alongX ? load.fx : load.fy) += force * toModel * weight[i] / total;
                ++createdLoads;
            }
            const double offset = lineTransverse - centroid;
            if (std::abs(offset) > 1e-6 && n >= 2 && t(n - 1) - t(0) > 1e-6) {
                // binario F x offset por dois nos extremos (forcas iguais e opostas na direcao do vento)
                const double couple = force * offset / (t(n - 1) - t(0));
                (alongX ? record(atLevel[n - 1].id).fx : record(atLevel[n - 1].id).fy) += couple * toModel;
                (alongX ? record(atLevel[0].id).fx : record(atLevel[0].id).fy) -= couple * toModel;
            } else if (std::abs(offset) > 1e-6) {
                notes << tr("Cota %1 m: um único nó transversal; a excentricidade da resultante não pôde ser aplicada (use um diafragma rígido).").arg(zAbs, 0, 'f', 2);
            }
        }
    }
    rebuildCaseSelector();
    syncLoadVisuals();
    refreshPropertiesPanel();
    QString message = tr("Vento estático: %1 caso(s) de carga gerado(s) com %2 carga(s) nodal(is).").arg(result.cases.size()).arg(createdLoads);
    if (!notes.isEmpty()) {
        message += QLatin1Char(' ') + notes.join(QLatin1Char(' '));
    }
    showStatusMessage(message, 10000);
    if (!notes.isEmpty() && !qEnvironmentVariableIsSet("STRUCTURA_AUTOSHOT")) {
        QMessageBox::information(this, tr("Vento estático"), notes.join(QStringLiteral("\n\n")));
    }
}

void MainWindow::onWindShed()
{
    if (!m_sceneController) {
        return;
    }
    QHash<QUuid, std::array<double, 3>> position;
    for (const auto &node : m_sceneController->nodeInfos()) {
        position.insert(node.id, {node.x, node.y, node.z});
    }
    QVector<WindShedService::Bar> bars;
    for (const auto &bar : m_sceneController->bars()) {
        if (bar.externalId <= 0) {
            continue;
        }
        const auto a = position.constFind(bar.startNodeId);
        const auto b = position.constFind(bar.endNodeId);
        if (a == position.constEnd() || b == position.constEnd()) {
            continue;
        }
        bars.append({bar.externalId, *a, *b});
    }
    if (bars.size() < 3) {
        QMessageBox::information(this, tr("Vento em galpões"), tr("Modele o galpão (colunas e tesouras de cobertura) antes de gerar o vento."));
        return;
    }
    Structura::UI::WindShedDialog dialog(bars, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto result = dialog.result();
    if (result.cases.isEmpty()) {
        QMessageBox::warning(this, tr("Vento em galpões"), result.warnings.isEmpty() ? tr("Nenhum caso gerado.") : result.warnings.join(QLatin1Char('\n')));
        return;
    }
    const double forceFactor = DesignService::unitFactors(m_materials).first;   // N por unidade de forca do modelo
    const double toModel = 1000.0 / forceFactor;   // kN -> unidade de forca do modelo

    StateGuard guard(this, tr("Vento em galpões"));
    int applied = 0;
    for (const auto &c : result.cases) {
        const bool exists = std::any_of(m_extras.loadCases.begin(), m_extras.loadCases.end(), [&](const auto &lc) { return lc.name == c.name; });
        if (!exists) {
            QString abbreviation = c.name;
            abbreviation.replace(QStringLiteral("VENTO "), QStringLiteral("V"));
            m_extras.loadCases.append({c.name, abbreviation.remove(QLatin1Char(' ')), false});
        }
        m_memberLoads.erase(std::remove_if(m_memberLoads.begin(), m_memberLoads.end(), [&](const MemberLoad &m) { return m.loadCase == c.name; }), m_memberLoads.end());
        for (const auto &load : c.loads) {
            MemberLoad record;
            record.memberId = load.memberId;
            record.loadCase = c.name;
            record.system = QStringLiteral("GLOBAL");
            record.qx = load.q[0] * toModel;
            record.qy = load.q[1] * toModel;
            record.qz = load.q[2] * toModel;
            m_memberLoads.append(record);
            ++applied;
        }
    }
    rebuildCaseSelector();
    syncLoadVisuals();
    refreshPropertiesPanel();
    QString message = tr("Vento em galpões: %1 caso(s) de carga gerado(s) com %2 carga(s) distribuída(s).").arg(result.cases.size()).arg(applied);
    showStatusMessage(message, 10000);
}
