#include "MainWindow.h"
#include "ViewOptionsBar.h"

#include "ConnectionDialog.h"
#include "ConnectionForces.h"
#include "DesignRunData.h"
#include "NodeConnectionsDialog.h"
#include "SelectionModel.h"
#include "app/services/ConnectionService.h"
#include "app/services/DesignService.h"
#include "viz/SceneController.h"

#include <QHash>
#include <QInputDialog>
#include <QMessageBox>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

using Structura::App::Services::ConnectionService;

void MainWindow::onCheckConnections()
{
    if (!m_sceneController) {
        return;
    }
    using Structura::UI::NodeConnectionsDialog;
    using Structura::App::ProjectData::NodeConnection;

    // analise de dimensionamento reaproveitada enquanto o modelo analisado nao muda
    auto cache = std::make_shared<std::pair<QString, std::shared_ptr<DesignRunData>>>();
    auto currentRun = [this, cache]() -> std::shared_ptr<DesignRunData> {
        if (cache->second && cache->first == m_analysisFingerprint) {
            return cache->second;
        }
        auto run = std::make_shared<DesignRunData>();
        if (!buildDesignRun(*run)) {
            return nullptr;
        }
        cache->first = m_analysisFingerprint;
        cache->second = run;
        return run;
    };

    // posicoes dos nos e barras do modelo (para identificar as diagonais que chegam ao no de um banzo)
    struct EndRef {
        int memberId {0};
        bool atEnd {false};
        QUuid barId;
        double dx {0.0}, dy {0.0}, dz {0.0};   // direcao da barra saindo do no (so a direcao importa)
    };
    auto barsAtNode = [this](int nodeExternalId) {
        QHash<QUuid, std::array<double, 3>> pos;
        QUuid nodeUuid;
        for (const auto &node : m_sceneController->nodeInfos()) {
            pos.insert(node.id, {node.x, node.y, node.z});
            if (node.externalId == nodeExternalId) {
                nodeUuid = node.id;
            }
        }
        std::vector<EndRef> list;
        if (nodeUuid.isNull()) {
            return list;
        }
        const auto origin = pos.value(nodeUuid);
        for (const auto &bar : m_sceneController->bars()) {
            if (bar.startNodeId != nodeUuid && bar.endNodeId != nodeUuid) {
                continue;
            }
            EndRef ref;
            ref.memberId = bar.externalId;
            ref.barId = bar.id;
            ref.atEnd = bar.endNodeId == nodeUuid;
            const auto other = pos.value(ref.atEnd ? bar.startNodeId : bar.endNodeId);
            ref.dx = other[0] - origin[0];
            ref.dy = other[1] - origin[1];
            ref.dz = other[2] - origin[2];
            list.push_back(ref);
        }
        return list;
    };
    auto sectionInfoOf = [this](const QUuid &sectionId) -> Structura::App::ProjectData::SectionInfo {
        for (const auto &section : m_sections) {
            if (section.uuid == sectionId) {
                return section;
            }
        }
        return {};
    };
    auto memberInfo = [this](const Structura::App::ProjectData::SectionInfo &section, int memberId) {
        ConnectionService::BeamInfo beam;
        beam.name = section.name;
        beam.family = section.family;
        beam.geometry = section.geometry;
        beam.area = section.area;
        beam.depth = section.geometry.value(QStringLiteral("d"), section.geometry.value(QStringLiteral("D"), 0.0));
        beam.bf = section.geometry.value(QStringLiteral("bf"), section.geometry.value(QStringLiteral("b"), 0.0));
        beam.tw = section.geometry.value(QStringLiteral("tw"), section.geometry.value(QStringLiteral("t"), 6.0));
        beam.tf = section.geometry.value(QStringLiteral("tf"), section.geometry.value(QStringLiteral("t"), 10.0));
        const auto params = m_extras.design.paramsFor(memberId);
        const auto steel = Structura::App::Services::DesignService::findSteel(params.steel.isEmpty() ? m_extras.design.defaultSteel : params.steel);
        if (steel) {
            beam.fy = steel->fy;
            beam.fu = steel->fu;
        }
        return beam;
    };

    NodeConnectionsDialog::Callbacks callbacks;
    callbacks.forces = [currentRun](const NodeConnection &connection) -> std::optional<ConnectionService::Forces> {
        const auto run = currentRun();
        if (!run) {
            return std::nullopt;
        }
        return Structura::UI::connectionForces(*run, connection);
    };
    callbacks.focus = [this](const NodeConnection &connection) {
        for (const auto &bar : m_sceneController->bars()) {
            if (bar.externalId == connection.memberId) {
                m_selectionModel->selectBars({bar.id});
                focusMember(connection.memberId);
                break;
            }
        }
    };
    callbacks.calculator = [this]() { onOpenConnectionCalculator(); };
    callbacks.create = [this, barsAtNode, sectionInfoOf, memberInfo](int type) -> NodeConnection {
        NodeConnection none;
        const auto selected = m_selectionModel ? m_selectionModel->selectedBars() : QSet<QUuid>();
        QUuid barId;
        for (const auto &bar : m_sceneController->bars()) {
            if (selected.contains(bar.id)) {
                barId = bar.id;
                break;
            }
        }
        if (barId.isNull()) {
            const QString what = type == NodeConnection::kGusset ? tr("a diagonal ou o contraventamento")
                               : type == NodeConnection::kBasePlate ? tr("o pilar")
                               : type == NodeConnection::kTubular ? tr("o banzo (tubo circular)")
                               : type == NodeConnection::kSplice ? tr("a viga (um dos lados da emenda)")
                                                                 : tr("a viga que se liga ao apoio");
            QMessageBox::information(this, tr("Ligações"), tr("Selecione no modelo %1 e tente novamente.").arg(what));
            return none;
        }
        QHash<QUuid, int> nodeExternal;
        QHash<QUuid, QVector3D> nodePosition;
        for (const auto &node : m_sceneController->nodeInfos()) {
            nodeExternal.insert(node.id, node.externalId);
            nodePosition.insert(node.id, QVector3D(static_cast<float>(node.x), static_cast<float>(node.y), static_cast<float>(node.z)));
        }
        auto positionOfNode = [&nodePosition](const QUuid &id) -> std::optional<QVector3D> {
            const auto it = nodePosition.constFind(id);
            return it == nodePosition.constEnd() ? std::nullopt : std::optional<QVector3D>(*it);
        };
        for (const auto &bar : m_sceneController->bars()) {
            if (bar.id != barId) {
                continue;
            }
            const QStringList ends {tr("Início (nó %1)").arg(nodeExternal.value(bar.startNodeId)), tr("Fim (nó %1)").arg(nodeExternal.value(bar.endNodeId))};
            const QString prompt = type == NodeConnection::kBasePlate ? tr("Barra %1: extremidade apoiada na fundação (base do pilar):")
                                 : type == NodeConnection::kTubular ? tr("Barra %1 (banzo): extremidade no nó da ligação:")
                                 : type == NodeConnection::kGusset ? tr("Barra %1 (diagonal): extremidade ligada à chapa de nó:")
                                 : type == NodeConnection::kSplice ? tr("Barra %1: extremidade da emenda:")
                                                                   : tr("Barra %1: extremidade que se liga ao apoio:");
            // o no da extremidade escolhida fica marcado na tela enquanto o usuario decide
            const auto startPos = positionOfNode(bar.startNodeId), endPos = positionOfNode(bar.endNodeId);
            auto highlight = [this, startPos, endPos, &ends](const QString &text) {
                m_sceneController->setNodeHighlight(ends.indexOf(text) == 1 ? endPos : startPos);
            };
            QInputDialog chooser(this);
            chooser.setWindowTitle(tr("Nova ligação"));
            chooser.setLabelText(prompt.arg(bar.externalId));
            chooser.setComboBoxItems(ends);
            chooser.setComboBoxEditable(false);
            highlight(ends.value(0));
            connect(&chooser, &QInputDialog::textValueChanged, this, highlight);
            const bool accepted = chooser.exec() == QDialog::Accepted;
            m_sceneController->setNodeHighlight(std::nullopt);
            if (!accepted) {
                return none;
            }
            const QString choice = chooser.textValue();
            const bool atEnd = ends.indexOf(choice) == 1;
            const auto section = sectionInfoOf(bar.sectionId);
            const ConnectionService::BeamInfo beam = memberInfo(section, bar.externalId);
            const auto defaultSteel = Structura::App::Services::DesignService::findSteel(m_extras.design.defaultSteel);
            const double plateFy = defaultSteel ? defaultSteel->fy : 250.0, plateFu = defaultSteel ? defaultSteel->fu : 400.0;
            const int nodeId = nodeExternal.value(atEnd ? bar.endNodeId : bar.startNodeId);
            NodeConnection connection;
            if (type == NodeConnection::kTubular) {
                // diagonais: barras que chegam ao no e nao sao a continuacao do banzo
                const auto refs = barsAtNode(nodeId);
                const EndRef *chordRef = nullptr;
                for (const auto &r : refs) {
                    if (r.memberId == bar.externalId) {
                        chordRef = &r;
                    }
                }
                QVector<ConnectionService::BraceInfo> braces;
                std::array<double, 3> firstSide {0, 0, 0};
                if (chordRef) {
                    const double cl = std::sqrt(chordRef->dx * chordRef->dx + chordRef->dy * chordRef->dy + chordRef->dz * chordRef->dz);
                    const std::array<double, 3> axis {chordRef->dx / cl, chordRef->dy / cl, chordRef->dz / cl};
                    for (const auto &r : refs) {
                        if (r.memberId == bar.externalId) {
                            continue;
                        }
                        const double l = std::sqrt(r.dx * r.dx + r.dy * r.dy + r.dz * r.dz);
                        const double dot = (r.dx * axis[0] + r.dy * axis[1] + r.dz * axis[2]) / l;
                        if (std::abs(dot) > 0.985) {
                            continue;   // continuacao do banzo
                        }
                        ConnectionService::BraceInfo brace;
                        brace.memberId = r.memberId;
                        brace.atEnd = r.atEnd;
                        brace.angleDeg = std::acos(std::min(1.0, std::abs(dot))) * 180.0 / 3.14159265358979;
                        // lado do banzo em que a diagonal esta (componente perpendicular)
                        const std::array<double, 3> perp {r.dx / l - dot * axis[0], r.dy / l - dot * axis[1], r.dz / l - dot * axis[2]};
                        if (braces.isEmpty()) {
                            firstSide = perp;
                        } else {
                            brace.sameSideAsFirst = (perp[0] * firstSide[0] + perp[1] * firstSide[1] + perp[2] * firstSide[2]) > 0.0;
                        }
                        for (const auto &b2 : m_sceneController->bars()) {
                            if (b2.externalId == r.memberId) {
                                const auto sec = sectionInfoOf(b2.sectionId);
                                brace.d = sec.geometry.value(QStringLiteral("D"), sec.geometry.value(QStringLiteral("d"), 0.0));
                                brace.t = sec.geometry.value(QStringLiteral("t"), 4.0);
                                brace.fy = memberInfo(sec, r.memberId).fy;
                            }
                        }
                        braces.append(brace);
                        if (braces.size() == 2) {
                            break;
                        }
                    }
                }
                connection = ConnectionService::makeTubular(beam, braces);
                connection.values["br1"] = braces.size() > 0 ? braces[0].memberId : 0;
                connection.values["br1End"] = braces.size() > 0 && braces[0].atEnd ? 1.0 : 0.0;
                connection.values["br2"] = braces.size() > 1 ? braces[1].memberId : 0;
                connection.values["br2End"] = braces.size() > 1 && braces[1].atEnd ? 1.0 : 0.0;
                if (braces.isEmpty()) {
                    QMessageBox::information(this, tr("Ligação tubular"),
                                             tr("Nenhuma diagonal chega a este nó além do banzo; os dados da diagonal foram preenchidos com valores padrão e podem ser editados."));
                }
            } else {
                connection = ConnectionService::makeDefault(type, beam, plateFy, plateFu);
            }
            connection.id = 1;   // o dialogo renumera
            connection.memberId = bar.externalId;
            connection.atEnd = atEnd;
            connection.nodeId = nodeId;
            return connection;
        }
        return none;
    };

    StateGuard guard(this, tr("Ligações"));
    NodeConnectionsDialog dialog(m_extras.connections, callbacks, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_extras.connections = dialog.connections();
    if (!m_extras.connections.isEmpty() && m_viewOptionsBar && !m_viewOptionsBar->isChecked(Structura::UI::ViewOptionsBar::kConnections)) {
        m_viewOptionsBar->setChecked(Structura::UI::ViewOptionsBar::kConnections, true);   // mostra as ligacoes recem aplicadas
        applyViewOption(Structura::UI::ViewOptionsBar::kConnections, true);
    }
    syncConnectionVisuals();
    showStatusMessage(tr("%1 ligação(ões) gravada(s) no modelo.").arg(m_extras.connections.size()), 5000);
}

void MainWindow::onOpenConnectionCalculator()
{
    if (!m_sceneController) {
        return;
    }
    Structura::UI::ConnectionDialog dialog([this](bool atEnd) {
        Structura::UI::EndForceEnvelope env;
        if (!m_selectionModel) {
            return env;
        }
        int memberId = 0;
        const auto selected = m_selectionModel->selectedBars();
        for (const auto &bar : m_sceneController->bars()) {
            if (selected.contains(bar.id)) {
                memberId = bar.externalId;
                break;
            }
        }
        if (memberId == 0) {
            return env;
        }
        DesignRunData run;
        if (!buildDesignRun(run)) {
            return env;
        }
        env = Structura::UI::envelopeAt(run, memberId, atEnd);
        env.label = QObject::tr("Barra %1, %2").arg(memberId).arg(atEnd ? QObject::tr("extremidade final") : QObject::tr("extremidade inicial"));
        return env;
    }, this);
    dialog.exec();
}
