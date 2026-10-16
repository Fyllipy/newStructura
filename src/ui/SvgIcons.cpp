#include "SvgIcons.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace Structura::UI {

QIcon svgIcon(const QString &name, const QColor &color, int size, const QColor &accent)
{
    QFile file(QStringLiteral(":/svg/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray data = file.readAll();
    data.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
    data.replace("#ACCENT", accent.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        return {};
    }

    QIcon icon;
    for (const int scale : {1, 2}) {
        const int pixels = size * scale;
        QPixmap pixmap(pixels, pixels);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter, QRectF(0, 0, pixels, pixels));
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace Structura::UI
