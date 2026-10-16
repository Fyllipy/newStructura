#pragma once

#include "viz/construction/ConstructionPlane.h"
#include "viz/construction/PlacementSolver.h"

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QEvent;
class QWidget;
class SceneController;

namespace Structura::UI {

class CoordinateReadoutWidget;
class SnapMarkerWidget;

/**
 * @brief Fluxo grafico de lancamento de nos e barras (porta do opensa).
 *
 * O cursor e projetado no plano de construcao ativo, com snaps de malha,
 * extremidade e centro e guias perpendicular/ortogonal. Mostra marcador de
 * snap, previa da barra e uma caixa de coordenadas X/Y/Z/angulo editavel
 * (Tab alterna campos, Enter confirma, prefixo '@' = relativo ao ponto inicial).
 */
class PlacementController : public QObject
{
    Q_OBJECT

public:
    using Vec3 = Structura::Construction::Vec3;

    enum class Mode {
        None,
        Bar,
        Copy, ///< Dois pontos: base e destino (copia os elementos selecionados)
        Move, ///< Dois pontos: base e destino (move os elementos selecionados)
        Rotate, ///< Um ponto (pivo)
        Mirror  ///< Dois pontos (eixo de espelhamento)
    };

    PlacementController(SceneController *scene, QWidget *viewport, QObject *parent = nullptr);
    ~PlacementController() override;

    /// Retorna true se a barra foi criada; em caso de falha o ponto inicial e mantido.
    std::function<bool(const Vec3 &, const Vec3 &)> onBarPlaced;
    /// Notifica coordenadas sob o cursor (para a barra de status); `valid=false` ao sair.
    std::function<void(bool, const Vec3 &)> onHoverChanged;
    /// Copiar/Mover: base -> destino; retorna true se a operacao foi aplicada.
    std::function<bool(Mode, const Vec3 &, const Vec3 &)> onDisplacement;
    /// Rotacionar: ponto unico (pivo).
    std::function<bool(Mode, const Vec3 &)> onSinglePoint;

    /// Segmentos (mundo) exibidos como previa deslocada durante copiar/mover.
    void setGhostSource(const std::vector<std::pair<Vec3, Vec3>> &segments) { m_ghostSource = segments; }
    std::function<void()> onCancelRequested;
    std::function<void()> onStateChanged;

    void start(Mode mode);
    void stop();
    [[nodiscard]] bool active() const noexcept { return m_mode != Mode::None; }
    [[nodiscard]] Mode mode() const noexcept { return m_mode; }
    [[nodiscard]] bool hasStartPoint() const noexcept { return m_start.has_value(); }

    /// Encaminha eventos do viewport; retorna true se o evento foi consumido.
    bool handleViewportEvent(QEvent *event);

    /// Reavalia o alvo sob o cursor (camera, plano ou modelo mudaram).
    void scheduleRefresh();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refresh();
    void hideVisuals();
    std::optional<Vec3> currentTarget(Structura::Construction::SnapKind *kind = nullptr) const;
    void placeAt(const Vec3 &point);

    // Edicao manual de coordenadas
    void cycleCoordinateEdit();
    void finishCoordinateEdit();
    void cancelCoordinateEdit(bool refreshAfter = true);
    void commitCoordinateEditor(int index);
    [[nodiscard]] std::vector<int> editableFields() const;
    [[nodiscard]] std::optional<double> previewAngleDegrees(const Vec3 &endpoint) const;

    SceneController *m_scene;
    QWidget *m_viewport;
    std::unique_ptr<Structura::Construction::PlacementSolver> m_solver;
    SnapMarkerWidget *m_marker;
    CoordinateReadoutWidget *m_readout;
    QTimer m_refreshTimer;

    Mode m_mode {Mode::None};
    std::vector<std::pair<Vec3, Vec3>> m_ghostSource;
    std::optional<QPointF> m_pointer;
    std::optional<QPointF> m_press;
    std::optional<Vec3> m_start;
    std::optional<Vec3> m_lastDisplay;

    std::optional<Vec3> m_manualTarget;
    std::optional<Vec3> m_manualDirection;
    int m_manualIndex {-1};
    bool m_manualConfirmable {false};
};

} // namespace Structura::UI
