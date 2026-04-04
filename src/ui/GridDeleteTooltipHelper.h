#pragma once

#include <QPoint>
#include <QString>

class QLabel;
class QWidget;

namespace Structura::UI {

class GridDeleteTooltipHelper
{
public:
    static void show(QLabel *&tooltip,
                     QWidget *parent,
                     const QPoint &widgetPos,
                     const QString &message);

    static void hide(QLabel *tooltip);
};

} // namespace Structura::UI
