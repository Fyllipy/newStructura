#pragma once

#include "viz/construction/ConstructionPlane.h"

#include <QPointF>
#include <QSizeF>

#include <optional>

class vtkRenderer;
class vtkRenderWindow;

namespace Structura::Construction {

class ConstructionScene;

/// Tipo de alvo de snap resolvido para o cursor.
enum class SnapKind {
    None,
    Grid,
    Endpoint,
    Center,
    Perpendicular,
    Orthogonal
};

struct PlacementTarget {
    Vec3 point {0.0, 0.0, 0.0};
    SnapKind kind {SnapKind::None};
};

/**
 * @brief Resolve o ponto de lancamento sob o cursor (porta do opensa).
 *
 * Intersecta o raio do cursor com o plano de construcao ativo e aplica, por
 * ordem de prioridade, os snaps de no (extremidade), centro de barra e malha;
 * alem das guias perpendicular/ortogonal a partir do primeiro ponto.
 * Todas as distancias de snap sao medidas em pixels de tela (coordenadas
 * logicas do widget Qt, origem no canto superior esquerdo).
 */
class PlacementSolver
{
public:
    static constexpr double kSnapTolerancePixels = 14.0;
    static constexpr double kGuideSnapHalfLength = 10.0;

    PlacementSolver(vtkRenderer *renderer, vtkRenderWindow *window, const ConstructionScene *scene);

    /// Tamanho logico do widget que recebe o mouse (para converter para pixels do render window).
    void setWidgetSize(const QSizeF &size) { m_widgetSize = size; }

    /// Intersecao do raio do cursor com o plano ativo.
    [[nodiscard]] bool cursorOnPlane(const QPointF &pointer, Vec3 &out) const;

    /// Projeta um ponto do mundo para o widget; retorna false se fora da tela.
    [[nodiscard]] bool worldToWidget(const Vec3 &world, QPointF &screen) const;

    /// Alvo final do cursor, considerando snaps e restricao ao plano pelo primeiro ponto.
    /// Nivel (coordenada normal) do plano atingido pelo ultimo `cursorOnPlane`.
    [[nodiscard]] double hitLevel() const noexcept { return m_hitLevel; }

    [[nodiscard]] PlacementTarget resolve(const QPointF &pointer,
                                          const Vec3 &cursor,
                                          const std::optional<Vec3> &start) const;

private:
    struct Projection;

    [[nodiscard]] std::optional<PlacementTarget> snapPosition(const QPointF &pointer,
                                                              const Vec3 &cursor,
                                                              const Projection &projection) const;
    [[nodiscard]] std::optional<std::pair<Vec3, double>> lineSnapCandidate(const Vec3 &direction,
                                                                            const Vec3 &start,
                                                                            const QPointF &pointer,
                                                                            const Projection &projection) const;
    [[nodiscard]] std::optional<PlacementTarget> guideTarget(const Vec3 &start,
                                                             const QPointF &pointer,
                                                             const Projection &projection) const;
    [[nodiscard]] Projection makeProjection() const;

    vtkRenderer *m_renderer;
    vtkRenderWindow *m_window;
    const ConstructionScene *m_scene;
    QSizeF m_widgetSize {1.0, 1.0};
    mutable double m_hitLevel {0.0};
};

} // namespace Structura::Construction
