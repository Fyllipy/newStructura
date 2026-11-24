#include "ui/SupportLegendOverlay.h"

#include "shell/ShellSupports.h"

#include <QFontMetrics>
#include <QPainter>

#include <algorithm>

namespace Structura::UI {

namespace {
// mesmas cores dos glifos (ShellRenderer): engaste, articulado, movel, parcial, mola, so compressao
constexpr int kColors[6][3] = {{192, 57, 43}, {46, 134, 193}, {40, 180, 99}, {175, 122, 197}, {230, 126, 34}, {23, 165, 137}};
constexpr int kPad = 8, kSwatch = 12, kGap = 7, kMargin = 14;

QString nameOf(int kind)
{
    switch (kind) {
    case 0: return QObject::tr("Engaste");
    case 1: return QObject::tr("Articulado");
    case 2: return QObject::tr("Apoio móvel");
    case 3: return QObject::tr("Restrição parcial");
    case 4: return QObject::tr("Mola");
    default: return QObject::tr("Só compressão");
    }
}
} // namespace

SupportLegendOverlay::SupportLegendOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setObjectName(QStringLiteral("shellSupportLegend"));
    hide();
}

void SupportLegendOverlay::setEntries(const std::vector<Entry> &entries)
{
    m_entries = entries;
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) { return a.kind < b.kind; });
    if (m_entries.empty()) {
        hide();
        return;
    }
    placeInParent();
    show();
    raise();
    update();
}

void SupportLegendOverlay::clearEntries()
{
    m_entries.clear();
    hide();
}

void SupportLegendOverlay::placeInParent()
{
    QWidget *host = parentWidget();
    if (!host || m_entries.empty()) {
        return;
    }
    const QFontMetrics fm(font());
    int textWidth = 0;
    for (const Entry &e : m_entries) {
        textWidth = std::max(textWidth, fm.horizontalAdvance(QStringLiteral("%1 (%2)").arg(nameOf(e.kind)).arg(e.count)));
    }
    const int rowHeight = std::max(fm.height(), kSwatch) + 4;
    const int width = 2 * kPad + kSwatch + kGap + textWidth;
    const int height = 2 * kPad + rowHeight * static_cast<int>(m_entries.size());
    resize(std::min(width, std::max(80, host->width() - 2 * kMargin)), std::min(height, std::max(40, host->height() - 2 * kMargin)));
    move(std::max(0, host->width() - this->width() - kMargin), std::max(0, host->height() - this->height() - kMargin));
}

void SupportLegendOverlay::paintEvent(QPaintEvent *)
{
    if (m_entries.empty()) {
        return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(143, 160, 184, 200), 1));
    p.setBrush(QColor(255, 255, 255, 225));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
    const QFontMetrics fm(font());
    const int rowHeight = std::max(fm.height(), kSwatch) + 4;
    int y = kPad;
    for (const Entry &e : m_entries) {
        const int k = std::clamp(e.kind, 0, 5);
        p.setPen(QPen(QColor(30, 35, 43), 1));
        p.setBrush(QColor(kColors[k][0], kColors[k][1], kColors[k][2]));
        p.drawRect(kPad, y + (rowHeight - kSwatch) / 2, kSwatch, kSwatch);
        p.drawText(QRect(kPad + kSwatch + kGap, y, width() - kPad - kSwatch - kGap - 4, rowHeight), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1 (%2)").arg(nameOf(e.kind)).arg(e.count));
        y += rowHeight;
    }
}

} // namespace Structura::UI
