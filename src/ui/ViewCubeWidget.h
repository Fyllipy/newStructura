#pragma once

#include <QWidget>

#include <array>
#include <functional>

class QToolButton;

namespace Structura::UI {

/// Cubo de vistas: acompanha a orientacao da camera; clicar numa face leva a camera a essa vista padrao.
class ViewCubeWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ViewCubeWidget(QWidget *parent = nullptr);

    /// Fornece posicao, alvo e vetor "para cima" da camera atual (retorna falso se indisponivel).
    void setCameraProvider(std::function<bool(double *, double *, double *)> provider) { m_camera = std::move(provider); }
    QToolButton *isoButton() const { return m_iso; }
    QToolButton *viewsButton() const { return m_views; }
    QToolButton *projectionButton() const { return m_projection; }
    /// Atualiza o texto do botao de projecao (true = perspectiva).
    void setPerspective(bool perspective);

signals:
    /// Direcao do olho em relacao ao alvo (unitaria) e vetor para cima.
    void viewRequested(double dx, double dy, double dz, double ux, double uy, double uz);
    /// Troca da projecao (true = perspectiva).
    void perspectiveToggled(bool perspective);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override { return {112, 182}; }

private:
    struct Face {
        std::array<double, 3> normal;
        QString label;
        std::array<double, 3> up;
    };
    [[nodiscard]] int faceAt(const QPointF &p) const;
    void layoutButtons();

    std::function<bool(double *, double *, double *)> m_camera;
    QToolButton *m_iso {nullptr};
    QToolButton *m_views {nullptr};
    QToolButton *m_projection {nullptr};
    int m_hover {-1};
    std::array<std::array<QPointF, 4>, 6> m_polygons {};
    std::array<bool, 6> m_visible {};
};

} // namespace Structura::UI
