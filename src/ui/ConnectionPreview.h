#pragma once

#include "app/ProjectDataTypes.h"

#include <QPixmap>
#include <QWidget>

namespace Structura::UI {

/// Vista 3D esquematica de uma ligacao (projecao paralela, sombreada), girada com o mouse.
class ConnectionPreview final : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionPreview(QWidget *parent = nullptr);
    void setConnection(const Structura::App::ProjectData::NodeConnection &connection, double eta = 0.0);
    void clear();

    /// Desenho de exemplo de uma tipologia (para listas e seletores).
    [[nodiscard]] static QPixmap example(int type, const QSize &size);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    QSize sizeHint() const override { return {360, 260}; }

private:
    static void paintConnection(QPainter &painter, const QRectF &area, const Structura::App::ProjectData::NodeConnection &connection, double eta, double yaw, double pitch,
                                double zoom);

    Structura::App::ProjectData::NodeConnection m_connection;
    bool m_has {false};
    double m_eta {0.0};
    double m_yaw {-0.75}, m_pitch {0.45}, m_zoom {1.0};
    QPoint m_last;
};

} // namespace Structura::UI
