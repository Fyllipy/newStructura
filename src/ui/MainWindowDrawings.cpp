#include "MainWindow.h"

#include "ConnectionForces.h"
#include "DesignRunData.h"
#include "DrawingDialog.h"
#include "app/services/ConnectionService.h"
#include "app/services/SectionPlacementService.h"
#include "app/services/SheetBuilder.h"
#include "viz/SceneController.h"

#include <QDate>
#include <QMessageBox>

#include <cmath>

void MainWindow::onGenerateDrawings()
{
    using Structura::App::Services::SheetBuilder;
    if (!m_sceneController || m_sceneController->bars().empty()) {
        QMessageBox::information(this, tr("Pranchas"), tr("Não há barras no modelo."));
        return;
    }
    SheetBuilder::Input input;
    QHash<QUuid, int> nodeExternal;
    for (const auto &node : m_sceneController->nodeInfos()) {
        input.nodes.append({node.externalId, node.x, node.y, node.z});
        nodeExternal.insert(node.id, node.externalId);
    }
    QHash<int, Structura::App::ProjectData::MemberPlacement> placement;
    for (const auto &p : m_extras.placements) {
        placement.insert(p.memberId, p);
    }
    QHash<int, QString> groupOf;
    for (const auto &group : m_extras.groups) {
        for (const int member : group.members) {
            groupOf.insert(member, group.name.isEmpty() ? QString::number(group.id) : group.name);
        }
    }
    using Structura::App::ProjectData::MemberRelease::kEndRy;
    using Structura::App::ProjectData::MemberRelease::kEndRz;
    using Structura::App::ProjectData::MemberRelease::kStartRy;
    using Structura::App::ProjectData::MemberRelease::kStartRz;
    for (const auto &bar : m_sceneController->bars()) {
        SheetBuilder::Bar b;
        b.id = bar.externalId;
        b.nodeI = nodeExternal.value(bar.startNodeId);
        b.nodeJ = nodeExternal.value(bar.endNodeId);
        for (const auto &section : m_sections) {
            if (section.uuid != bar.sectionId) {
                continue;
            }
            b.section = section.name;
            b.massPerMeter = section.area * 7850.0;
            const auto &g = section.geometry;
            b.widthMm = g.value(QStringLiteral("bf"), g.value(QStringLiteral("b"), g.value(QStringLiteral("D"), std::sqrt(section.area) * 1000.0)));
            b.depthMm = g.value(QStringLiteral("d"), g.value(QStringLiteral("h"), g.value(QStringLiteral("D"), std::sqrt(section.area) * 1000.0)));
            b.family = section.family;
            b.geometry = section.geometry;
            b.mirrored = section.mirrored;
            b.flangeMm = g.value(QStringLiteral("tf"), 0.0);
            b.webMm = g.value(QStringLiteral("tw"), 0.0);
            b.iShape = g.contains(QStringLiteral("tf")) && g.contains(QStringLiteral("tw")) && g.contains(QStringLiteral("bf"));
        }
        const auto it = placement.constFind(b.id);
        if (it != placement.constEnd()) {
            // alinhamento / excentricidade: centroide em relacao ao no, em cada extremidade (mm)
            const Structura::App::ProjectData::SectionInfo *sectionPtr = nullptr;
            for (const auto &section : m_sections) {
                if (section.uuid == bar.sectionId) {
                    sectionPtr = &section;
                }
            }
            auto endOffset = [&](double extraA, double extraB) {
                Structura::App::ProjectData::MemberPlacement end = *it;
                end.offsetA = extraA;
                end.offsetB = extraB;
                return Structura::App::Services::SectionPlacementService::axisOffset(sectionPtr, end);
            };
            const auto startOffset = endOffset(it->offsetA, it->offsetB);
            const auto finishOffset = endOffset(it->endOffsetA(), it->endOffsetB());
            b.offsetIy = startOffset.dy * 1000.0;
            b.offsetIz = startOffset.dz * 1000.0;
            b.offsetJy = finishOffset.dy * 1000.0;
            b.offsetJz = finishOffset.dz * 1000.0;
            b.memberType = it->memberType;
            b.rotationDeg = it->rotationDeg;
            b.hingeI = (it->releaseMask & (kStartRy | kStartRz)) != 0;
            b.hingeJ = (it->releaseMask & (kEndRy | kEndRz)) != 0;
        }
        b.group = groupOf.value(b.id);
        input.bars.append(b);
    }
    input.axes = m_extras.axes;
    input.views3d = m_extras.views3d;
    input.layouts = m_extras.sheetLayouts;
    input.info = m_extras.titleBlock;
    if (input.info.project.isEmpty()) {
        input.info.project = m_projectName.isEmpty() ? tr("Projeto") : m_projectName;
    }
    if (input.info.date.isEmpty()) {
        input.info.date = QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy"));
    }

    // detalhes das ligacoes (com os esforcos da analise, se houver)
    if (!m_extras.connections.isEmpty()) {
        std::shared_ptr<DesignRunData> run;
        if (!m_caseRuns.isEmpty()) {
            run = std::make_shared<DesignRunData>();
            if (!buildDesignRun(*run)) {
                run.reset();
            }
        }
        for (const auto &connection : m_extras.connections) {
            SheetBuilder::ConnectionDetail detail;
            detail.connection = connection;
            for (const auto &bar : m_sceneController->bars()) {
                if (bar.externalId != connection.memberId) {
                    continue;
                }
                for (const auto &section : m_sections) {
                    if (section.uuid == bar.sectionId) {
                        detail.beamSection = section.name;
                        detail.beamDepthMm = section.geometry.value(QStringLiteral("d"), 0.0);
                        detail.beamFlangeMm = section.geometry.value(QStringLiteral("tf"), 0.0);
                    }
                }
            }
            if (run) {
                if (const auto forces = Structura::UI::connectionForces(*run, connection, m_extras.connections)) {
                    detail.hasForces = true;
                    detail.shearKN = forces->shear / 1000.0;
                    detail.axialKN = forces->axial / 1000.0;
                    detail.eta = Structura::App::Services::ConnectionService::evaluate(connection, *forces).maxRatio;
                }
            }
            input.connections.append(detail);
        }
    }
    Structura::UI::DrawingDialog dialog(input, this);
    connect(&dialog, &Structura::UI::DrawingDialog::titleBlockChanged, this,
            [this](const Structura::App::ProjectData::TitleBlockSettings &settings) {
                m_extras.titleBlock = settings;
                markModified();
            });
    dialog.setCameraCapture([this](Structura::App::ProjectData::SavedView3D &view) {
        double position[3], target[3], up[3], angle = 30.0;
        bool parallel = false;
        if (!m_sceneController->cameraState(position, target, up, angle, parallel)) {
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            view.position[static_cast<std::size_t>(i)] = position[i];
            view.target[static_cast<std::size_t>(i)] = target[i];
            view.up[static_cast<std::size_t>(i)] = up[i];
        }
        view.fovDeg = angle;
        view.perspective = !parallel;
        view.explicitCamera = true;
        return true;
    });
    connect(&dialog, &Structura::UI::DrawingDialog::layoutsChanged, this, [this](const QVector<Structura::App::ProjectData::SheetLayoutDef> &layouts) {
        m_extras.sheetLayouts = layouts;
        markModified();
    });
    connect(&dialog, &Structura::UI::DrawingDialog::views3dChanged, this,
            [this](const QVector<Structura::App::ProjectData::SavedView3D> &views) {
                m_extras.views3d = views;
                markModified();
            });
    dialog.exec();
}
