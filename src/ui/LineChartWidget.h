#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

namespace Structura::UI {

/// Grafico de linhas simples (eixos com graduacao, grade, legenda e leitura dos valores sob o mouse).
class LineChartWidget final : public QWidget
{
    Q_OBJECT

public:
    struct Series {
        QString name;
        QColor color {QStringLiteral("#0d7fb9")};
        QVector<QPointF> points;
        bool markers {false};   ///< desenha pontos (sem ligar), para marcar valores discretos
        bool dashed {false};
    };

    explicit LineChartWidget(QWidget *parent = nullptr);

    void setTitles(const QString &title, const QString &xLabel, const QString &yLabel);
    void setSeries(const QVector<Series> &series);
    void clear();
    /// Linhas verticais de referencia (por exemplo, os periodos dos modos).
    void setVerticalMarkers(const QVector<double> &x);
    [[nodiscard]] QSize sizeHint() const override { return {520, 300}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Range {
        double lo {0.0}, hi {1.0};
    };
    [[nodiscard]] QRectF plotRect() const;
    void computeRanges();
    [[nodiscard]] QPointF toScreen(const QPointF &p) const;

    QString m_title, m_xLabel, m_yLabel;
    QVector<Series> m_series;
    QVector<double> m_markers;
    Range m_x, m_y;
    QPointF m_hover;
    bool m_hasHover {false};
};

} // namespace Structura::UI
