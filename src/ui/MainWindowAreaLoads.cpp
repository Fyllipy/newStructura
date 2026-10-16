#include "MainWindow.h"

#include "AreaLoadDialog.h"
#include "SelectionModel.h"
#include "app/services/AreaLoadService.h"
#include "viz/SceneController.h"

#include <QHash>
#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <functional>
#include <cmath>

using Structura::App::Services::AreaLoadService;

namespace {
template <typename Container, typename Predicate>
void eraseIf(Container &container, Predicate predicate)
{
    container.erase(std::remove_if(container.begin(), container.end(), predicate), container.end());
}
} // namespace

void MainWindow::applyAreaLoad(bool wind)
{
    StateGuard guard(this, tr("Carga de área / vento"));
    if (!m_selectionModel || !m_sceneController) {
        return;
    }
    const QSet<QUuid> selectedBars = m_selectionModel->selectedBars();
    if (selectedBars.isEmpty()) {
        QMessageBox::information(this, wind ? tr("Vento (NBR 6123)") : tr("Carga de área"),
                                 tr("Selecione as barras que recebem a carga (terças, travessas ou vigas da superfície)."));
        return;
    }

    // Geometria
    QHash<QUuid, AreaLoadService::Vec3> position;
    AreaLoadService::Vec3 centroid {0, 0, 0};
    double maxZ = 0.0;
    int count = 0;
    for (const auto &node : m_sceneController->nodeInfos()) {
        position.insert(node.id, {node.x, node.y, node.z});
        centroid[0] += node.x;
        centroid[1] += node.y;
        centroid[2] += node.z;
        maxZ = std::max(maxZ, node.z);
        ++count;
    }
    if (count > 0) {
        for (double &c : centroid) {
            c /= count;
        }
    }
    QVector<AreaLoadService::BarSegment> segments;
    for (const auto &bar : m_sceneController->bars()) {
        if (!selectedBars.contains(bar.id) || bar.externalId <= 0) {
            continue;
        }
        const auto a = position.constFind(bar.startNodeId);
        const auto b = position.constFind(bar.endNodeId);
        if (a == position.constEnd() || b == position.constEnd()) {
            continue;
        }
        segments.append({bar.externalId, *a, *b});
    }
    if (segments.isEmpty()) {
        return;
    }

    Structura::UI::AreaLoadDialog dialog(wind ? Structura::UI::AreaLoadDialog::Mode::Wind : Structura::UI::AreaLoadDialog::Mode::Area,
                                         loadCaseNames(), dialogLoadCase(), maxZ, selectedBars.size(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto request = dialog.request();
    if (request.caseName.isEmpty()) {
        return;
    }

    // Direcao da forca
    AreaLoadService::Vec3 direction = request.direction;
    if (request.directionKind != 0) {
        AreaLoadService::Vec3 normal = AreaLoadService::surfaceNormal(segments);
        AreaLoadService::Vec3 faceCenter {0, 0, 0};
        for (const auto &s : segments) {
            for (int k = 0; k < 3; ++k) {
                faceCenter[k] += 0.5 * (s.a[k] + s.b[k]) / segments.size();
            }
        }
        const AreaLoadService::Vec3 away {faceCenter[0] - centroid[0], faceCenter[1] - centroid[1], faceCenter[2] - centroid[2]};
        auto dotp = [](const AreaLoadService::Vec3 &a, const AreaLoadService::Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
        const double normNormal = std::sqrt(dotp(normal, normal));
        if (normNormal < 1e-9) {
            // barras paralelas: a normal e a direcao do centro da estrutura a face, sem a componente ao longo das barras
            const auto &s = segments.first();
            AreaLoadService::Vec3 u {s.b[0] - s.a[0], s.b[1] - s.a[1], s.b[2] - s.a[2]};
            const double un = std::sqrt(dotp(u, u));
            for (double &c : u) {
                c /= std::max(un, 1e-12);
            }
            const double along = dotp(away, u);
            normal = {away[0] - along * u[0], away[1] - along * u[1], away[2] - along * u[2]};
            const double n2 = std::sqrt(dotp(normal, normal));
            if (n2 < 1e-9) {
                QMessageBox::warning(this, tr("Carga de área"), tr("Não foi possível determinar a normal da superfície. Use uma direção fixa."));
                return;
            }
            for (double &c : normal) {
                c /= n2;
            }
        } else if (dotp(normal, away) < 0.0) {
            for (double &c : normal) {
                c = -c;
            }
        }
        const double sign = request.directionKind == 1 ? 1.0 : -1.0;   // 1 = para fora; 2 = para dentro
        direction = {sign * normal[0], sign * normal[1], sign * normal[2]};
    }

    AreaLoadService::Options options;
    options.pressure = request.pressure;
    options.direction = direction;
    options.projected = request.projected;
    options.manualWidth = request.manualWidth;
    options.overhang = request.overhang;
    const auto loads = AreaLoadService::distribute(segments, options);

    // Caso de carga (cria se nao existir)
    bool caseExists = false;
    for (const auto &c : m_extras.loadCases) {
        caseExists = caseExists || c.name == request.caseName;
    }
    if (!caseExists) {
        Structura::App::ProjectData::LoadCase loadCase;
        loadCase.name = request.caseName;
        loadCase.abbreviation = request.caseName.left(3).toUpper();
        m_extras.loadCases.append(loadCase);
        rebuildCaseSelector();
    }

    int applied = 0;
    for (const auto &load : loads) {
        if (load.id <= 0) {
            continue;
        }
        eraseIf(m_memberLoads, [&](const MemberLoad &m) { return m.memberId == load.id && m.loadCase == request.caseName; });
        MemberLoad record;
        record.memberId = load.id;
        record.loadCase = request.caseName;
        record.system = QStringLiteral("GLOBAL");
        record.qx = load.q[0];
        record.qy = load.q[1];
        record.qz = load.q[2];
        m_memberLoads.append(record);
        ++applied;
    }
    // Panos: um por grupo de barras paralelas; substitui os panos do mesmo caso que compartilham barras.
    {
        QSet<int> touched;
        for (const auto &seg : segments) {
            touched.insert(seg.id);
        }
        auto &panelsList = m_extras.areaLoads;
        for (int i = panelsList.size() - 1; i >= 0; --i) {
            if (panelsList[i].loadCase != request.caseName) {
                continue;
            }
            for (const int member : panelsList[i].members) {
                if (touched.contains(member)) {
                    panelsList.removeAt(i);
                    break;
                }
            }
        }
        int nextId = 1;
        for (const auto &p : panelsList) {
            nextId = std::max(nextId, p.id + 1);
        }
        const double dn = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
        for (const auto &panel : AreaLoadService::panels(segments, options)) {
            Structura::App::ProjectData::AreaLoadPanel record;
            record.id = nextId++;
            record.loadCase = request.caseName;
            record.kind = wind ? QStringLiteral("WIND") : QStringLiteral("AREA");
            record.pressure = request.pressure;
            record.direction = {direction[0] / std::max(dn, 1e-12), direction[1] / std::max(dn, 1e-12), direction[2] / std::max(dn, 1e-12)};
            for (const auto &pt : panel.polygon) {
                record.polygon.append(pt);
            }
            record.members = panel.members;
            record.description = request.description;
            panelsList.append(record);
        }
    }
    syncLoadVisuals();
    refreshPropertiesPanel();
    showStatusMessage(tr("%1 aplicada em %2 barra(s) no caso \"%3\" (%4).").arg(wind ? tr("Carga de vento") : tr("Carga de área")).arg(applied).arg(request.caseName).arg(request.description), 8000);
}

void MainWindow::onApplyAreaLoad()
{
    applyAreaLoad(false);
}

void MainWindow::onApplyWindLoad()
{
    applyAreaLoad(true);
}
