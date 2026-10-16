#pragma once

#include "app/ProjectDataTypes.h"

#include <QPainterPath>
#include <QWidget>

#include <array>
#include <optional>

namespace Structura::UI {

/**
 * @brief Desenho interativo da disposicao da secao em relacao ao no (vista do no I para o J).
 *
 * Mostra o contorno da secao girado, o retangulo envolvente com os 9 pontos de referencia, o no
 * (alvo laranja), o centroide e os eixos de referencia y/z. No modo interativo:
 *  - clicar num ponto de referencia o torna o ponto da secao que fica sobre o no;
 *  - arrastar a secao altera o deslocamento adicional (resolucao de 1 mm).
 * No modo somente leitura (miniatura do painel de propriedades) nada reage ao mouse.
 */
class SectionPlacementWidget : public QWidget
{
    Q_OBJECT

public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    explicit SectionPlacementWidget(QWidget *parent = nullptr);

    /// Secao exibida (copia); sem geometria parametrica mostra um aviso.
    void setSection(const SectionInfo &section);
    void clearSection();
    /// Giro (graus), ponto de referencia (0-8) e deslocamento adicional (m) da extremidade exibida.
    void setPlacement(double rotationDeg, int reference, double offsetA, double offsetB);
    void setInteractive(bool interactive);
    /// Miniatura: sem legenda nem textos.
    void setCompact(bool compact);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void referenceChanged(int reference);
    void offsetChanged(double offsetA, double offsetB);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Geometry {
        bool valid {false};
        QVector<QPolygonF> loops;        ///< contornos em metros, eixos da secao (x = a/z, y = b/y para cima)
        double minA {0.0}, maxA {0.0}, minB {0.0}, maxB {0.0};
    };

    void rebuildGeometry();
    void updateScale(bool force = false);
    [[nodiscard]] QPointF referenceCoordinate(int reference) const;
    /// Metros no referencial girado -> pixels (a para a direita, b para cima).
    [[nodiscard]] QPointF toScreen(const QPointF &referenceFrameMeters) const;
    /// Ponto da secao (a, b em metros) -> pixels, considerando giro e deslocamento.
    [[nodiscard]] QPointF sectionToScreen(double a, double b) const;
    [[nodiscard]] int handleAt(const QPointF &pos) const;
    [[nodiscard]] bool insideSection(const QPointF &pos) const;
    void drawArrow(QPainter &painter, const QPointF &from, const QPointF &to, const QColor &color, const QString &label) const;

    SectionInfo m_section;
    Geometry m_geometry;
    double m_rotationDeg {0.0};
    int m_reference {0};
    double m_offsetA {0.0};
    double m_offsetB {0.0};
    bool m_interactive {true};
    bool m_compact {false};

    double m_scale {1.0};            ///< pixels por metro
    bool m_scaleFrozen {false};
    int m_hoverHandle {-1};
    bool m_dragging {false};
    QPointF m_dragStart;
    double m_dragOffsetA0 {0.0};
    double m_dragOffsetB0 {0.0};
};

} // namespace Structura::UI
