#pragma once

#include "design/JointClassification.h"

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

namespace Structura::UI {

/// Unidades de exibicao dos graficos (os dados do calculo ficam sempre em rad e kN.m).
struct JointChartUnits {
    double thetaFactor {1.0};     ///< unidade de exibicao por rad (1 = rad, 1000 = mrad)
    QString thetaLabel {QStringLiteral("rad")};
    double momentFactor {1.0};    ///< unidade de exibicao por kN.m (1 = kN.m, 1000 = N.m, 100 = kN.cm)
    QString momentLabel {QStringLiteral("kN·m")};
};

/// Base dos graficos da ferramenta de ligacoes: exportacao em PNG/SVG pelo mesmo desenho da tela.
class JointChartBase : public QWidget
{
    Q_OBJECT
public:
    explicit JointChartBase(QWidget *parent = nullptr);
    bool exportPng(const QString &path);
    bool exportSvg(const QString &path, const QString &title);
    [[nodiscard]] QSize sizeHint() const override { return {640, 420}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    virtual void drawChart(QPainter &painter, const QRectF &area) = 0;
};

/// Grafico 1 — curva momento-rotacao, nivel de 2/3 de Mj,Rd, ponto de leitura da rotacao e reta secante (Si). Zoom (roda), pan (arrastar),
/// reinicio (duplo clique), cursor com leitura de coordenadas e do ponto mais proximo da curva.
class MomentRotationChart final : public JointChartBase
{
    Q_OBJECT
public:
    explicit MomentRotationChart(QWidget *parent = nullptr);
    void setAnalysis(const Structura::Design::JointClass::JointAnalysis &analysis);
    void setUnits(const JointChartUnits &units);
    void resetView();
    /// Descreve o que o grafico exibe (para testes e leitores de tela): numero de curvas, se ha dados insuficientes.
    [[nodiscard]] int drawnCurves() const { return m_drawnCurves; }
    [[nodiscard]] bool insufficientData() const { return m_insufficient; }
    [[nodiscard]] QRectF viewRect() const { return m_view; }
    [[nodiscard]] QString hoverText() const { return m_hoverText; }
    /// Texto que explica a falta de dados (ex.: nao ha ligacao definida no no), mostrado no lugar da orientacao padrao.
    void setHint(const QString &hint);

protected:
    void drawChart(QPainter &painter, const QRectF &area) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    [[nodiscard]] QRectF plotArea(const QRectF &area) const;
    [[nodiscard]] QPointF toScreen(const QPointF &data, const QRectF &plot) const;
    [[nodiscard]] QPointF toData(const QPointF &screen, const QRectF &plot) const;
    void fitView();

    Structura::Design::JointClass::JointAnalysis m_analysis;
    JointChartUnits m_units;
    QRectF m_view;            ///< em unidades de exibicao
    bool m_userView {false};
    bool m_dragging {false};
    QPointF m_dragStart;
    QRectF m_dragView;
    QPointF m_hover;
    bool m_hasHover {false};
    QString m_hoverText;
    QString m_hint;
    int m_drawnCurves {0};
    bool m_insufficient {true};
};

/// Grafico 2 — classificacao pela rigidez relativa eta = Si / (E Iv/Lv) (eixo horizontal, escala logaritmica) e Si (eixo vertical, logaritmico),
/// com as regioes rotulada, semirrigida e rigida (esta, so quando Kv/Kp >= 0,1) e um marcador por sentido do momento.
class ClassificationChart final : public JointChartBase
{
    Q_OBJECT
public:
    explicit ClassificationChart(QWidget *parent = nullptr);
    void setAnalysis(const Structura::Design::JointClass::JointAnalysis &analysis);
    /// Posicao do marcador (eta, Si em kN.m/rad) do sentido positivo; vazio se nao ha resultado.
    [[nodiscard]] bool markerPosition(Structura::Design::JointClass::Direction direction, QPointF &etaAndSi) const;
    [[nodiscard]] QRectF dataRange() const { return m_range; }

protected:
    void drawChart(QPainter &painter, const QRectF &area) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Structura::Design::JointClass::JointAnalysis m_analysis;
    QRectF m_range;   ///< eta min, Si min, largura, altura (decadas)
    QPointF m_hover;
    bool m_hasHover {false};
};

} // namespace Structura::UI
