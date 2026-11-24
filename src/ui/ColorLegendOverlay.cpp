#include "ui/ColorLegendOverlay.h"

#include "shell/ShellVisual.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

namespace {
constexpr int kTicks = Structura::Shell::kColorBands + 1;   // um rotulo em cada fronteira de faixa (maximo no topo, minimo na base)
constexpr int kBarWidth = 16;
constexpr int kPad = 9;
constexpr int kMargin = 14;
} // namespace

ColorLegendOverlay::ColorLegendOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setObjectName(QStringLiteral("shellColorLegend"));
    hide();
}

QString ColorLegendOverlay::formatValue(double value) const
{
    if (!std::isfinite(value)) {
        return QStringLiteral("—");
    }
    if (value == 0.0) {
        return QStringLiteral("0");
    }
    return QString::number(value, 'g', 4);
}

QStringList ColorLegendOverlay::tickLabels() const
{
    QStringList labels;
    if (!m_hasLegend) {
        return labels;
    }
    for (int i = 0; i < kTicks; ++i) {
        const double t = 1.0 - static_cast<double>(i) / (kTicks - 1);   // do topo (maximo) a base (minimo)
        labels << formatValue(m_min + t * (m_max - m_min));
    }
    return labels;
}

void ColorLegendOverlay::setLegend(const QString &title, const QString &unit, double minValue, double maxValue, bool fixedRange, bool smoothed)
{
    m_title = title;
    m_unit = unit;
    m_min = minValue;
    m_max = maxValue;
    m_fixed = fixedRange;
    m_smoothed = smoothed;
    m_hasLegend = true;
    placeInParent();
    show();
    raise();
    update();
}

void ColorLegendOverlay::clearLegend()
{
    m_hasLegend = false;
    m_title.clear();
    m_unit.clear();
    hide();
}

void ColorLegendOverlay::placeInParent()
{
    QWidget *host = parentWidget();
    if (!host) {
        return;
    }
    // barra proporcional a altura da vista (legivel em janelas pequenas, sem passar de 220 px)
    m_barHeight = std::clamp(host->height() / 3, 70, 220);
    const QFontMetrics fm(font());
    int labelWidth = 0;
    for (const QString &t : tickLabels()) {
        labelWidth = std::max(labelWidth, fm.horizontalAdvance(t));
    }
    const QString head = m_unit.isEmpty() ? m_title : QStringLiteral("%1 [%2]").arg(m_title, m_unit);
    QFont bold = font();
    bold.setBold(true);
    const int headWidth = QFontMetrics(bold).horizontalAdvance(head);
    const int body = kBarWidth + 8 + labelWidth;
    const int width = std::min(std::max(headWidth, body) + 2 * kPad, std::max(90, host->width() - 2 * kMargin));
    int height = 2 * kPad + QFontMetrics(bold).height() + 6 + m_barHeight + fm.height() / 2;
    if (m_fixed || m_smoothed) {
        height += fm.height();
    }
    resize(width, height);
    move(std::max(0, host->width() - width - kMargin), std::max(kMargin, (host->height() - height) / 2));
}

void ColorLegendOverlay::paintEvent(QPaintEvent *)
{
    if (!m_hasLegend) {
        return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(143, 160, 184, 200), 1));
    p.setBrush(QColor(255, 255, 255, 225));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);

    QFont bold = font();
    bold.setBold(true);
    p.setFont(bold);
    p.setPen(QColor(30, 35, 43));
    const QFontMetrics bfm(bold);
    const QString head = m_unit.isEmpty() ? m_title : QStringLiteral("%1 [%2]").arg(m_title, m_unit);
    p.drawText(QRect(kPad, kPad, width() - 2 * kPad, bfm.height()), Qt::AlignLeft | Qt::AlignVCenter, bfm.elidedText(head, Qt::ElideRight, width() - 2 * kPad));

    const int top = kPad + bfm.height() + 6;
    // barra: maximo no topo; mesma funcao de cor da malha
    for (int y = 0; y < m_barHeight; ++y) {
        const double t = 1.0 - static_cast<double>(y) / std::max(1, m_barHeight - 1);
        const auto c = Structura::Shell::bandColor(t);   // degraus, como a vista 3D
        // preenche a linha de pixels (drawLine com antialiasing borraria a fronteira entre faixas)
        p.fillRect(QRect(kPad, top + y, kBarWidth, 1), QColor(c[0], c[1], c[2]));
    }
    p.setPen(QColor(60, 66, 76));
    p.setBrush(Qt::NoBrush);
    p.drawRect(kPad, top, kBarWidth - 1, m_barHeight - 1);

    QFont plain = font();
    p.setFont(plain);
    const QFontMetrics fm(plain);
    const QStringList labels = tickLabels();
    for (int i = 0; i < labels.size(); ++i) {
        const int y = top + static_cast<int>(std::lround(static_cast<double>(i) * (m_barHeight - 1) / (kTicks - 1)));
        p.setPen(QColor(60, 66, 76));
        p.drawLine(kPad + kBarWidth - 1, y, kPad + kBarWidth + 3, y);
        p.setPen(QColor(30, 35, 43));
        p.drawText(QRect(kPad + kBarWidth + 6, y - fm.height() / 2, width() - kPad - kBarWidth - 6 - 4, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, labels[i]);
    }
    if (m_fixed || m_smoothed) {
        QFont small = font();
        small.setPointSizeF(std::max(6.5, small.pointSizeF() - 1.0));
        p.setFont(small);
        p.setPen(QColor(90, 100, 114));
        const QString note = m_fixed ? tr("faixa fixa") : tr("alisado nos nós");
        p.drawText(QRect(kPad, top + m_barHeight + 2, width() - 2 * kPad, QFontMetrics(small).height() + 2), Qt::AlignLeft | Qt::AlignVCenter, note);
    }
}

} // namespace Structura::UI
