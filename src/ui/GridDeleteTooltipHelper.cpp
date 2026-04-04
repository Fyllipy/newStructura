#include "GridDeleteTooltipHelper.h"

#include <QLabel>
#include <QWidget>

#include <algorithm>

namespace Structura::UI {

void GridDeleteTooltipHelper::show(QLabel *&tooltip,
                                   QWidget *parent,
                                   const QPoint &widgetPos,
                                   const QString &message)
{
    if (!parent) {
        return;
    }

    if (!tooltip) {
        tooltip = new QLabel(parent);
        tooltip->setStyleSheet(QStringLiteral(
            "QLabel { background: rgba(28,36,45,220); color: #f2f5fa; border-radius: 4px; padding: 6px 10px; }"));
        tooltip->setAttribute(Qt::WA_TransparentForMouseEvents);
        tooltip->hide();
    }

    tooltip->setText(message);
    tooltip->adjustSize();

    QPoint pos = widgetPos + QPoint(16, -16);
    const int tooltipWidth = tooltip->width();
    const int tooltipHeight = tooltip->height();
    pos.setX(std::clamp(pos.x(), 0, std::max(0, parent->width() - tooltipWidth)));
    pos.setY(std::clamp(pos.y(), 0, std::max(0, parent->height() - tooltipHeight)));

    tooltip->move(pos);
    if (!tooltip->isVisible()) {
        tooltip->show();
    }
}

void GridDeleteTooltipHelper::hide(QLabel *tooltip)
{
    if (tooltip) {
        tooltip->hide();
    }
}

} // namespace Structura::UI
