#include "MainWindow.h"

#include "PlacementController.h"
#include "PlacementOverlay.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionScene.h"

#include <QStatusBar>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>

#include <cmath>

QUuid MainWindow::findNodeAtPosition(const std::array<double, 3> &position) const
{
    if (!m_sceneController) {
        return {};
    }
    constexpr double tolerance = SceneController::kCoordTolerance;
    for (const auto &node : m_sceneController->nodeInfos()) {
        if (std::abs(node.x - position[0]) <= tolerance
            && std::abs(node.y - position[1]) <= tolerance
            && std::abs(node.z - position[2]) <= tolerance) {
            return node.id;
        }
    }
    return {};
}

void MainWindow::updateAxisLabels()
{
    if (!m_labelOverlay || !m_sceneController) {
        return;
    }
    auto *construction = m_sceneController->constructionScene();
    if (!construction) {
        return;
    }
    const auto &labels = construction->axisLabels();
    QVector<QString> texts;
    texts.reserve(static_cast<int>(labels.texts.size()));
    for (const QString &text : labels.texts) {
        texts.append(text);
    }
    m_labelOverlay->setGroup(QStringLiteral("reference-axis"), labels.positions, texts,
                             construction->referenceAxesVisible());
    m_labelOverlay->sync(m_sceneController->compositeMatrix());
}

void MainWindow::setupPlacement()
{
    if (!m_sceneController || !m_vtkWidget || m_placement) {
        return;
    }
    auto *construction = m_sceneController->constructionScene();
    if (!construction) {
        return;
    }

    m_vtkWidget->setMouseTracking(true);
    m_labelOverlay = new Structura::UI::ViewportLabelOverlay(m_vtkWidget);
    m_placement = new Structura::UI::PlacementController(m_sceneController, m_vtkWidget, this);

    m_cameraSyncTimer = new QTimer(this);
    m_cameraSyncTimer->setSingleShot(true);
    m_cameraSyncTimer->setInterval(0);
    connect(m_cameraSyncTimer, &QTimer::timeout, this, [this]() {
        if (m_labelOverlay) {
            m_labelOverlay->sync(m_sceneController->compositeMatrix());
        }
        if (m_placement) {
            m_placement->scheduleRefresh();
        }
    });
    connect(m_sceneController, &SceneController::cameraChanged, this, [this]() {
        if (m_cameraSyncTimer && !m_cameraSyncTimer->isActive()) {
            m_cameraSyncTimer->start();
        }
    });

    construction->onLabelsChanged = [this]() { updateAxisLabels(); };
    construction->onPlaneChanged = [this]() {
        if (m_placement) {
            m_placement->scheduleRefresh();
        }
    };

    m_placement->onBarPlaced = [this](const Structura::UI::PlacementController::Vec3 &start,
                                      const Structura::UI::PlacementController::Vec3 &end) {
        constexpr double tolerance = SceneController::kCoordTolerance;
        if (std::abs(start[0] - end[0]) <= tolerance
            && std::abs(start[1] - end[1]) <= tolerance
            && std::abs(start[2] - end[2]) <= tolerance) {
            showStatusMessage(tr("O segundo ponto deve ser diferente do ponto inicial."), 3000);
            return false;
        }

        QUuid startId = findNodeAtPosition(start);
        QUuid endId = findNodeAtPosition(end);
        const bool createdStart = startId.isNull();
        const bool createdEnd = endId.isNull();
        if (createdStart) {
            startId = m_sceneController->addPoint(start[0], start[1], start[2]);
        }
        if (createdEnd) {
            endId = m_sceneController->addPoint(end[0], end[1], end[2]);
        }

        for (const auto &bar : m_sceneController->bars()) {
            if ((bar.startNodeId == startId && bar.endNodeId == endId)
                || (bar.startNodeId == endId && bar.endNodeId == startId)) {
                showStatusMessage(tr("Ja existe uma barra entre esses dois pontos."), 3000);
                return false;
            }
        }

        m_lastSectionId = resolveSectionId(m_lastSectionId);
        const QUuid barId = m_sceneController->addBar(startId, endId, m_lastMaterialId, m_lastSectionId);
        if (barId.isNull()) {
            return false;
        }
        m_sceneController->setBarExternalId(barId, nextBarExternalId());
        showStatusMessage(tr("Barra criada. Clique no ponto inicial da proxima barra."), 3000);
        refreshPropertiesPanel();
        return true;
    };

    m_placement->onDisplacement = [this](Structura::UI::PlacementController::Mode mode,
                                         const Structura::UI::PlacementController::Vec3 &from,
                                         const Structura::UI::PlacementController::Vec3 &to) {
        if (mode == Structura::UI::PlacementController::Mode::Measure) {
            return applyMeasure(from, to);
        }
        if (mode == Structura::UI::PlacementController::Mode::Mirror) {
            return applyMirror(from, to);
        }
        const bool copy = mode == Structura::UI::PlacementController::Mode::Copy;
        const bool ok = applyDisplacement(copy, from, to);
        if (ok && !copy) {
            // Mover termina o comando; copiar continua para novas copias.
            QTimer::singleShot(0, this, [this]() { setCommand(Command::None); });
        }
        return ok;
    };

    m_placement->onSinglePoint = [this](Structura::UI::PlacementController::Mode mode, const Structura::UI::PlacementController::Vec3 &point) {
        if (mode == Structura::UI::PlacementController::Mode::Node) {
            return applyInsertNode(point);
        }
        return applyRotation(point);
    };

    m_placement->onHoverChanged = [this](bool valid, const Structura::UI::PlacementController::Vec3 &point) {
        if (valid) {
            setHoverInsertPoint(QVector3D(static_cast<float>(point[0]),
                                          static_cast<float>(point[1]),
                                          static_cast<float>(point[2])));
        } else {
            setHoverInsertPoint(std::nullopt);
        }
    };
    m_placement->onCancelRequested = [this]() { setCommand(Command::None); };
    m_placement->onStateChanged = [this]() { updateStatus(); };

    updateAxisLabels();
    setupViewOptions();
    setupViewCube();
}
