#pragma once

#include <QString>
#include <QWidget>

namespace Structura::UI {

/// Legenda da escala de cores dos resultados de cascas, desenhada sobre a vista 3D (filha do widget da vista, sem janela propria).
/// Mostra o nome da grandeza, a unidade, a barra de cores (a mesma escala `Structura::Shell::rainbow` da malha) e os valores
/// minimo, maximo e intermediarios. Transparente ao mouse: nao atrapalha a selecao nem a camera.
class ColorLegendOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ColorLegendOverlay(QWidget *parent);

    /// Define o conteudo e mostra a legenda. `fixedRange`: a faixa foi fixada pelo usuario (indicado na legenda).
    void setLegend(const QString &title, const QString &unit, double minValue, double maxValue, bool fixedRange, bool smoothed);
    /// Remove o conteudo e oculta a legenda (sem resultados).
    void clearLegend();
    [[nodiscard]] bool hasLegend() const noexcept { return m_hasLegend; }

    [[nodiscard]] QString title() const { return m_title; }
    [[nodiscard]] QString unit() const { return m_unit; }
    [[nodiscard]] double minValue() const noexcept { return m_min; }
    [[nodiscard]] double maxValue() const noexcept { return m_max; }
    [[nodiscard]] bool fixedRange() const noexcept { return m_fixed; }
    /// Rotulos das marcas, do maximo (topo) ao minimo (base): o que e desenhado.
    [[nodiscard]] QStringList tickLabels() const;

    /// Reposiciona (canto direito, centralizada na vertical) e dimensiona conforme o tamanho do widget pai.
    void placeInParent();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    [[nodiscard]] QString formatValue(double value) const;

    QString m_title;
    QString m_unit;
    double m_min {0.0};
    double m_max {0.0};
    bool m_fixed {false};
    bool m_smoothed {false};
    bool m_hasLegend {false};
    int m_barHeight {180};
};

} // namespace Structura::UI
