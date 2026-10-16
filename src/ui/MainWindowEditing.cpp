#include "MainWindow.h"

#include "PlacementController.h"
#include "SelectionModel.h"
#include "SvgIcons.h"
#include "app/UndoRedoService.h"
#include "viz/SceneController.h"

#include <QAction>
#include <QHash>
#include <QMessageBox>
#include <QSet>
#include <QStatusBar>
#include <QTimer>
#include "TransformDialogs.h"
#include "viz/construction/ConstructionScene.h"
#include <functional>
#include <QVector3D>
#include <QVTKOpenGLNativeWidget.h>

#include <array>
#include <cmath>

namespace {

using Vec3 = Structura::UI::PlacementController::Vec3;

} // namespace

QVector<QUuid> MainWindow::editNodeIds() const
{
    QVector<QUuid> ids;
    if (!m_selectionModel || !m_sceneController) {
        return ids;
    }
    QSet<QUuid> seen;
    auto add = [&](const QUuid &id) {
        if (!id.isNull() && !seen.contains(id)) {
            seen.insert(id);
            ids.append(id);
        }
    };
    for (const QUuid &id : m_selectionModel->selectedNodes()) {
        add(id);
    }
    for (const QUuid &barId : m_selectionModel->selectedBars()) {
        if (const auto *bar = m_sceneController->findBar(barId)) {
            add(bar->startNodeId());
            add(bar->endNodeId());
        }
    }
    return ids;
}

void MainWindow::updateEditActionsEnabled()
{
    const bool hasSelection = m_selectionModel
        && (!m_selectionModel->selectedNodes().isEmpty() || !m_selectionModel->selectedBars().isEmpty());
    if (m_copyElementsAction) {
        m_copyElementsAction->setEnabled(hasSelection);
    }
    if (m_moveElementsAction) {
        m_moveElementsAction->setEnabled(hasSelection);
    }
    for (QAction *action : {m_serialCopyAction, m_rotateElementsAction, m_mirrorElementsAction}) {
        if (action) {
            action->setEnabled(hasSelection);
        }
    }
}

void MainWindow::beginDisplacementCommand(Command command)
{
    if (editNodeIds().isEmpty()) {
        QMessageBox::information(this,
                                 tr("Editar elementos"),
                                 tr("Selecione nos e/ou barras antes de usar este comando."));
        return;
    }

    // Previa: barras selecionadas (segmentos) e nos (pequenas cruzes) que acompanham o cursor.
    std::vector<std::pair<Vec3, Vec3>> segments;
    for (const QUuid &barId : m_selectionModel->selectedBars()) {
        const auto *bar = m_sceneController->findBar(barId);
        if (!bar) {
            continue;
        }
        const auto *a = m_sceneController->findNode(bar->startNodeId());
        const auto *b = m_sceneController->findNode(bar->endNodeId());
        if (a && b) {
            segments.push_back({Vec3 {a->x(), a->y(), a->z()}, Vec3 {b->x(), b->y(), b->z()}});
        }
    }
    constexpr double cross = 0.12;
    for (const QUuid &nodeId : editNodeIds()) {
        if (const auto *node = m_sceneController->findNode(nodeId)) {
            const Vec3 p {node->x(), node->y(), node->z()};
            for (int axis = 0; axis < 3; ++axis) {
                Vec3 a = p;
                Vec3 b = p;
                a[static_cast<std::size_t>(axis)] -= cross;
                b[static_cast<std::size_t>(axis)] += cross;
                segments.push_back({a, b});
            }
        }
    }

    setCommand(command);
    if (m_placement) {
        m_placement->setGhostSource(segments);
    }
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

void MainWindow::onSerialCopy()
{
    if (editNodeIds().isEmpty()) {
        QMessageBox::information(this, tr("Copia em serie"), tr("Selecione nos e/ou barras antes de usar este comando."));
        return;
    }
    Structura::UI::SerialCopyDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (dialog.dx() == 0.0 && dialog.dy() == 0.0 && dialog.dz() == 0.0) {
        showStatusMessage(tr("Informe um passo diferente de zero."), 3000);
        return;
    }
    for (int i = 1; i <= dialog.count(); ++i) {
        const double dx = dialog.dx() * i;
        const double dy = dialog.dy() * i;
        const double dz = dialog.dz() * i;
        applyTransform(true, [dx, dy, dz](const std::array<double, 3> &p) {
            return std::array<double, 3> {p[0] + dx, p[1] + dy, p[2] + dz};
        });
    }
    showStatusMessage(tr("%1 copia(s) em serie criada(s).").arg(dialog.count()), 4000);
}

void MainWindow::onRotateElements()
{
    beginDisplacementCommand(Command::RotateElements);
}

void MainWindow::onMirrorElements()
{
    beginDisplacementCommand(Command::MirrorElements);
}

bool MainWindow::applyRotation(const Vec3 &pivot)
{
    auto *construction = m_sceneController->constructionScene();
    const auto axes = Structura::Construction::planeAxes(construction->planeMode());
    Structura::UI::RotateDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const double angle = dialog.angleDegrees() * 3.14159265358979323846 / 180.0;
    if (std::abs(angle) < 1e-12) {
        return false;
    }
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const auto u = static_cast<std::size_t>(axes.u);
    const auto v = static_cast<std::size_t>(axes.v);
    const bool ok = applyTransform(dialog.copy(), [=](const std::array<double, 3> &p) {
        std::array<double, 3> q = p;
        const double du = p[u] - pivot[u];
        const double dv = p[v] - pivot[v];
        q[u] = pivot[u] + c * du - s * dv;
        q[v] = pivot[v] + s * du + c * dv;
        return q;
    });
    QTimer::singleShot(0, this, [this]() { setCommand(Command::None); });
    return ok;
}

bool MainWindow::applyMirror(const Vec3 &a, const Vec3 &b)
{
    auto *construction = m_sceneController->constructionScene();
    const auto axes = Structura::Construction::planeAxes(construction->planeMode());
    const auto u = static_cast<std::size_t>(axes.u);
    const auto v = static_cast<std::size_t>(axes.v);
    double du = b[u] - a[u];
    double dv = b[v] - a[v];
    const double length = std::hypot(du, dv);
    if (length < SceneController::kCoordTolerance) {
        showStatusMessage(tr("Os dois pontos do eixo de espelhamento devem ser diferentes no plano ativo."), 3000);
        return false;
    }
    du /= length;
    dv /= length;
    Structura::UI::MirrorDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const bool ok = applyTransform(dialog.keepOriginals(), [=](const std::array<double, 3> &p) {
        std::array<double, 3> q = p;
        const double wu = p[u] - a[u];
        const double wv = p[v] - a[v];
        const double dot = wu * du + wv * dv;
        q[u] = a[u] + 2.0 * dot * du - wu;
        q[v] = a[v] + 2.0 * dot * dv - wv;
        return q;
    });
    QTimer::singleShot(0, this, [this]() { setCommand(Command::None); });
    return ok;
}

void MainWindow::onCopyElements()
{
    beginDisplacementCommand(Command::CopyElements);
}

void MainWindow::onMoveElements()
{
    beginDisplacementCommand(Command::MoveElements);
}

bool MainWindow::applyDisplacement(bool copy, const Vec3 &from, const Vec3 &to)
{
    const double dx = to[0] - from[0];
    const double dy = to[1] - from[1];
    const double dz = to[2] - from[2];
    if (std::abs(dx) < SceneController::kCoordTolerance && std::abs(dy) < SceneController::kCoordTolerance
        && std::abs(dz) < SceneController::kCoordTolerance) {
        showStatusMessage(tr("O destino deve ser diferente do ponto base."), 3000);
        return false;
    }
    return applyTransform(copy, [dx, dy, dz](const std::array<double, 3> &p) {
        return std::array<double, 3> {p[0] + dx, p[1] + dy, p[2] + dz};
    });
}

bool MainWindow::applyTransform(bool copy, const std::function<std::array<double, 3>(const std::array<double, 3> &)> &map)
{
    const QVector<QUuid> nodeIds = editNodeIds();
    if (nodeIds.isEmpty()) {
        return false;
    }

    if (!copy) {
        QVector<QVector3D> oldPositions;
        QVector<QVector3D> newPositions;
        for (const QUuid &id : nodeIds) {
            const auto *node = m_sceneController->findNode(id);
            if (!node) {
                continue;
            }
            oldPositions.append(QVector3D(static_cast<float>(node->x()), static_cast<float>(node->y()), static_cast<float>(node->z())));
            const auto mapped = map({node->x(), node->y(), node->z()});
            newPositions.append(QVector3D(static_cast<float>(mapped[0]), static_cast<float>(mapped[1]), static_cast<float>(mapped[2])));
        }
        m_undoService->pushMoveNodesCommand(m_sceneController, nodeIds, oldPositions, newPositions);
        syncLoadVisuals();
        syncSupportVisuals();
        refreshPropertiesPanel();
        showStatusMessage(tr("%1 no(s) transformado(s). Ctrl+Z desfaz.").arg(nodeIds.size()), 4000);
        return true;
    }

    // ── Copia ────────────────────────────────────────────────────────────
    QHash<QUuid, QUuid> nodeMap;
    QHash<int, int> externalNodeMap; // somente nos realmente criados (recebem apoios e cargas)
    QVector<QUuid> createdNodes;
    for (const QUuid &id : nodeIds) {
        const auto *source = m_sceneController->findNode(id);
        if (!source) {
            continue;
        }
        const std::array<double, 3> target = map({source->x(), source->y(), source->z()});
        QUuid destination = findNodeAtPosition(target);
        if (destination.isNull()) {
            const int sourceExternal = source->externalId();
            const auto restraints = source->restraints();
            destination = m_sceneController->addPoint(target[0], target[1], target[2]);
            if (auto *created = m_sceneController->findNode(destination)) {
                for (int i = 0; i < 6; ++i) {
                    created->setRestraint(i, restraints[static_cast<std::size_t>(i)]);
                }
                externalNodeMap.insert(sourceExternal, created->externalId());
            }
            createdNodes.append(destination);
        }
        nodeMap.insert(id, destination);
    }

    // Apoios e cargas nodais dos nos criados.
    {
        const auto supports = m_supports;
        for (const auto &support : supports) {
            const auto it = externalNodeMap.constFind(support.nodeId);
            if (it != externalNodeMap.constEnd()) {
                NodeSupport copy = support;
                copy.nodeId = it.value();
                m_supports.append(copy);
            }
        }
        const auto loads = m_nodalLoads;
        for (const auto &load : loads) {
            const auto it = externalNodeMap.constFind(load.nodeId);
            if (it != externalNodeMap.constEnd()) {
                NodalLoad copy = load;
                copy.nodeId = it.value();
                m_nodalLoads.append(copy);
            }
        }
    }

    // Barras selecionadas (mesmo material/secao/ponto K) e suas cargas.
    QHash<int, int> externalBarMap;
    QVector<QUuid> createdBars;
    const auto existingBars = m_sceneController->bars();
    for (const QUuid &barId : m_selectionModel->selectedBars()) {
        const auto *source = m_sceneController->findBar(barId);
        if (!source) {
            continue;
        }
        const QUuid start = nodeMap.value(source->startNodeId());
        const QUuid end = nodeMap.value(source->endNodeId());
        if (start.isNull() || end.isNull() || start == end) {
            continue;
        }
        bool duplicate = false;
        for (const auto &bar : m_sceneController->bars()) {
            duplicate = duplicate || (bar.startNodeId == start && bar.endNodeId == end)
                        || (bar.startNodeId == end && bar.endNodeId == start);
        }
        if (duplicate) {
            continue;
        }
        const int sourceExternal = source->externalId();
        const std::optional<Structura::Model::Vector3> kPoint = source->kPoint();
        const QUuid materialId = source->materialId();
        const QUuid sectionId = source->sectionId();
        const QUuid created = m_sceneController->addBar(start, end, materialId, sectionId);
        if (created.isNull()) {
            continue;
        }
        const int newExternal = nextBarExternalId();
        m_sceneController->setBarExternalId(created, newExternal);
        if (kPoint) {
            if (auto *bar = m_sceneController->findBar(created)) {
                const auto mappedK = map({kPoint->x(), kPoint->y(), kPoint->z()});
                bar->setKPoint(Structura::Model::Vector3(mappedK[0], mappedK[1], mappedK[2]));
            }
        }
        externalBarMap.insert(sourceExternal, newExternal);
        createdBars.append(created);
    }
    Q_UNUSED(existingBars)

    {
        const auto loads = m_memberLoads;
        for (const auto &load : loads) {
            const auto it = externalBarMap.constFind(load.memberId);
            if (it != externalBarMap.constEnd()) {
                MemberLoad copy = load;
                copy.memberId = it.value();
                m_memberLoads.append(copy);
            }
        }
        const auto placements = m_extras.placements;
        for (const auto &placement : placements) {
            const auto it = externalBarMap.constFind(placement.memberId);
            if (it != externalBarMap.constEnd()) {
                auto copy = placement;
                copy.memberId = it.value();
                m_extras.placements.append(copy);
            }
        }
        const auto moments = m_extras.memberMoments;
        for (const auto &moment : moments) {
            const auto it = externalBarMap.constFind(moment.memberId);
            if (it != externalBarMap.constEnd()) {
                auto copy = moment;
                copy.memberId = it.value();
                m_extras.memberMoments.append(copy);
            }
        }
    }

    syncLoadVisuals();
    syncSupportVisuals();
    syncPlacementVisuals();
    refreshPropertiesPanel();
    showStatusMessage(tr("Copiado: %1 no(s) novo(s), %2 barra(s). Clique no proximo destino ou Esc.")
                          .arg(createdNodes.size())
                          .arg(createdBars.size()),
                      4000);
    return true;
}
