#include "app/services/ConnectionCatalog.h"
#include "MainWindow.h"

#include "AxesPanel.h"
#include "SectionPlacementPanel.h"
#include "PlacementController.h"
#include "PlacementOverlay.h"
#include "ViewOptionsBar.h"
#include "viz/postprocessor/PostProcessorRenderer.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionScene.h"

#include "app/services/CombinationService.h"
#include "app/services/ConnectionPlacement.h"
#include "app/services/ConnectionSolids.h"
#include "app/services/DesignService.h"
#include "app/services/SectionPlacementService.h"
#include "viz/LocalCoordinateSystem.h"
#include "app/services/SectionPropertiesService.h"
#include <QHash>
#include <QRegularExpression>
#include <QVTKOpenGLNativeWidget.h>
#include <QToolButton>

#include <array>

QVector<MainWindow::ConnectionPlacementInfo> MainWindow::connectionPlacements() const
{
    QVector<ConnectionPlacementInfo> result;
    if (!m_sceneController) {
        return result;
    }
    QHash<QUuid, std::array<double, 3>> positionById;
    for (const auto &node : m_sceneController->nodeInfos()) {
        positionById.insert(node.id, {node.x, node.y, node.z});
    }
    const double lengthFactor = Structura::App::Services::SectionPropertiesService::unitFactors(m_materials).second;
    Structura::Geometry::DefaultLocalAxisProvider axisProvider;
    const auto allBars = m_sceneController->bars();
    QHash<QUuid, const SectionInfo *> sectionsById;
    for (const auto &section : m_sections) {
        sectionsById.insert(section.uuid, &section);
    }

    // Eixos locais e pontos do eixo da barra (com giro da secao e excentricidade, como no solver e nos solidos)
    struct BarFrame {
        std::array<double, 3> start {}, end {}, x {}, y {}, z {};
        const SectionInfo *section {nullptr};
    };
    auto frameOf = [&](const SceneController::BarInfo &bar) -> std::optional<BarFrame> {
        const auto start = positionById.constFind(bar.startNodeId);
        const auto end = positionById.constFind(bar.endNodeId);
        if (start == positionById.constEnd() || end == positionById.constEnd()) {
            return std::nullopt;
        }
        std::optional<std::array<double, 3>> kPoint;
        if (const auto *model = m_sceneController->findBar(bar.id)) {
            if (model->hasKPoint()) {
                const auto &k = *model->kPoint();
                kPoint = std::array<double, 3> {k.x(), k.y(), k.z()};
            }
        }
        Structura::App::ProjectData::MemberPlacement placement;
        for (const auto &candidate : m_extras.placements) {
            if (candidate.memberId == bar.externalId) {
                placement = candidate;
                break;
            }
        }
        const auto lcs = Structura::Geometry::rotatedAboutX(axisProvider.computeLCS(*start, *end, kPoint), placement.rotationDeg);
        BarFrame frame;
        const auto sectionIt = sectionsById.constFind(bar.sectionId);
        frame.section = sectionIt == sectionsById.constEnd() ? nullptr : *sectionIt;
        const auto offset = Structura::App::Services::SectionPlacementService::axisOffset(frame.section, placement);
        for (std::size_t i = 0; i < 3; ++i) {
            const double shift = lcs.yPrime[i] * offset.dy + lcs.zPrime[i] * offset.dz;
            frame.start[i] = (*start)[i] + shift;
            frame.end[i] = (*end)[i] + shift;
        }
        frame.x = lcs.xPrime;
        frame.y = lcs.yPrime;
        frame.z = lcs.zPrime;
        return frame;
    };

    for (int index = 0; index < m_extras.connections.size(); ++index) {
        const auto &connection = m_extras.connections.at(index);
        const SceneController::BarInfo *bar = nullptr;
        for (const auto &candidate : allBars) {
            if (candidate.externalId == connection.memberId) {
                bar = &candidate;
                break;
            }
        }
        if (!bar) {
            continue;
        }
        const auto beam = frameOf(*bar);
        if (!beam) {
            continue;
        }
        ConnectionPlacementInfo info;
        info.index = index;
        const double sign = connection.atEnd ? -1.0 : 1.0;   // s aponta do no para dentro da barra
        info.origin = connection.atEnd ? beam->end : beam->start;   // eixo da barra no no (ja com a excentricidade)
        for (std::size_t i = 0; i < 3; ++i) {
            info.s[i] = sign * beam->x[i];
            info.t[i] = beam->y[i];
            info.u[i] = beam->z[i];
        }
        // a barra termina na face do apoio (mais a folga de montagem da chapa de cisalhamento), nao no no
        const bool outsideNode = connection.values.value(QStringLiteral("outside"), 0.0) != 0.0;   // talas / flange fora do no: junta a `outDist` da face
        const bool bearsOnSupport = connection.type == Structura::App::ProjectData::NodeConnection::kShearTab
            || connection.type == Structura::App::ProjectData::NodeConnection::kDoubleAngle
            || connection.type == Structura::App::ProjectData::NodeConnection::kGusset
            || connection.type == Structura::App::ProjectData::NodeConnection::kEndPlate
            || (Structura::App::Services::ConnectionCatalog::find(connection.type) && Structura::App::Services::ConnectionCatalog::find(connection.type)->usesSupport
                && Structura::App::Services::ConnectionCatalog::find(connection.type)->legacyType < 0)
            || Structura::App::Services::ConnectionCatalog::baseType(connection.type) == Structura::App::ProjectData::NodeConnection::kEndPlate
            || Structura::App::Services::ConnectionCatalog::baseType(connection.type) == Structura::App::ProjectData::NodeConnection::kGusset
            || Structura::App::Services::ConnectionCatalog::baseType(connection.type) == Structura::App::ProjectData::NodeConnection::kShearTab
            || Structura::App::Services::ConnectionCatalog::baseType(connection.type) == Structura::App::ProjectData::NodeConnection::kDoubleAngle
            || outsideNode;
        // s = 0 fica na face do apoio: plano da mesa/alma da barra de apoio escolhida, na posicao real dela
        double supportOffset = connection.values.value(QStringLiteral("supOffset"), 0.0) / lengthFactor;
        int supportId = static_cast<int>(connection.values.value(QStringLiteral("supMember"), 0.0) + 0.5);
        if (supportId <= 0 && supportOffset <= 0.0 && bearsOnSupport) {
            // sem barra de apoio escolhida: a ligacao ficava no eixo do no e a viga atravessava o pilar. Usa-se a barra do mesmo no, nao paralela
            // a viga, de maior altura de secao (pilar ou viga principal), para localizar a face de apoio.
            const QUuid nodeId = connection.atEnd ? bar->endNodeId : bar->startNodeId;
            std::vector<Structura::App::Services::SupportCandidate> candidates;
            for (const auto &candidate : allBars) {
                if (candidate.externalId == connection.memberId || (candidate.startNodeId != nodeId && candidate.endNodeId != nodeId)) {
                    continue;
                }
                const auto host = frameOf(candidate);
                if (!host || !host->section) {
                    continue;
                }
                const auto &g = host->section->geometry;
                candidates.push_back({candidate.externalId, host->x, g.value(QStringLiteral("d"), g.value(QStringLiteral("D"), 0.0))});
            }
            supportId = Structura::App::Services::pickSupportAutomatically(info.s, candidates);
        }
        if (supportId > 0) {
            for (const auto &candidate : allBars) {
                if (candidate.externalId != supportId) {
                    continue;
                }
                const auto host = frameOf(candidate);
                if (!host || !host->section) {
                    break;
                }
                const auto &g = host->section->geometry;
                Structura::App::Services::SupportSection support;
                support.axisPoint = host->start;
                support.y = host->y;
                support.z = host->z;
                support.depth = g.value(QStringLiteral("d"), g.value(QStringLiteral("D"), 0.0));
                support.width = g.value(QStringLiteral("bf"), g.value(QStringLiteral("b"), support.depth));
                support.webThickness = g.value(QStringLiteral("tw"), 0.0);
                support.isIShape = g.contains(QStringLiteral("tf")) && g.contains(QStringLiteral("tw"));
                double computed = 0.0;
                if (Structura::App::Services::supportFaceOffset(info.origin, info.s, support, 1.0 / lengthFactor, computed)) {
                    supportOffset = computed;   // viga quase paralela ao apoio: mantem o afastamento gravado
                }
                break;
            }
        }
        for (std::size_t i = 0; i < 3; ++i) {
            info.origin[i] += info.s[i] * supportOffset;
        }
        if (outsideNode && (supportId > 0 || supportOffset > 0.0)) {
            // ligacao fora do no: a junta fica a `outDist` da face do apoio, ao longo da viga (a viga continua ate o apoio)
            const double outDist = connection.values.value(QStringLiteral("outDist"), 0.0) / lengthFactor;
            for (std::size_t i = 0; i < 3; ++i) {
                info.origin[i] += info.s[i] * outDist;
            }
        }
        if (bearsOnSupport && (supportId > 0 || supportOffset > 0.0)) {
            // chapa de cisalhamento: folga de montagem; chapa de extremidade: a viga comeca na face da chapa
            const double setback = connection.type == Structura::App::ProjectData::NodeConnection::kShearTab ? connection.values.value(QStringLiteral("setback"), 0.0)
                                 : connection.type == Structura::App::ProjectData::NodeConnection::kEndPlate ? connection.values.value(QStringLiteral("plateT"), 0.0) : 0.0;
            info.trimMm = std::max(0.0, supportOffset * lengthFactor + setback);
        }
        result.append(info);
    }
    return result;
}

namespace {
using Bar = Structura::UI::ViewOptionsBar;
}

void MainWindow::syncConnectionVisuals()
{
    using Structura::App::Services::ConnectionSolids;
    if (!m_sceneController || !m_viewOptionsBar || !m_viewOptionsBar->isChecked(Bar::kConnections)) {
        return;
    }
    const double lengthFactor = Structura::App::Services::SectionPropertiesService::unitFactors(m_materials).second;
    ConnectionSolids::Mesh mesh;
    for (const auto &info : connectionPlacements()) {
        const auto &connection = m_extras.connections.at(info.index);
        ConnectionSolids::Frame frame;
        frame.origin = info.origin;
        frame.s = info.s;
        frame.t = info.t;
        frame.u = info.u;
        frame.scale = 1.0 / lengthFactor;
        ConnectionSolids::append(mesh, ConnectionSolids::build(connection), frame, connection.verified ? std::max(connection.eta, 1e-6) : 0.0);
    }
    Structura::Visualization::ConnectionRenderer::Mesh out;
    out.points = std::move(mesh.points);
    out.normals = std::move(mesh.normals);
    out.colors = std::move(mesh.colors);
    out.triangles = std::move(mesh.triangles);
    m_sceneController->setConnectionMesh(out);
}
