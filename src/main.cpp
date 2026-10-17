#include "ui/MainWindow.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSplashScreen>
#include <QSurfaceFormat>

#include <QVTKOpenGLNativeWidget.h>

namespace {

/// Tela de abertura (images/splash.png, trocavel) com mensagem e barra de progresso na faixa inferior.
class StartupSplash final : public QSplashScreen
{
public:
    explicit StartupSplash(const QPixmap &pixmap)
        : QSplashScreen(pixmap, Qt::WindowStaysOnTopHint | Qt::SplashScreen)
    {
    }

    void setProgress(int percent, const QString &message)
    {
        m_percent = percent;
        m_message = message;
        repaint();
        QCoreApplication::processEvents();
    }

protected:
    void drawContents(QPainter *painter) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        const int w = width();
        const int h = height();
        QFont font = painter->font();
        font.setPixelSize(13);
        painter->setFont(font);
        painter->setPen(QColor(QStringLiteral("#dbe9f5")));
        painter->drawText(QRect(46, h - 112, w - 92, 20), Qt::AlignLeft | Qt::AlignVCenter, m_message);
        const QRectF track(46, h - 86, w - 92, 6);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 40));
        painter->drawRoundedRect(track, 3, 3);
        painter->setBrush(QColor(QStringLiteral("#ffb400")));
        painter->drawRoundedRect(QRectF(track.left(), track.top(), track.width() * m_percent / 100.0, track.height()), 3, 3);
    }

private:
    int m_percent {0};
    QString m_message;
};

QIcon applicationIcon()
{
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addFile(QStringLiteral(":/icons/app/app-%1.png").arg(size), QSize(size, size));
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    // Ensure VTK uses the correct OpenGL context inside Qt.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    Q_INIT_RESOURCE(resources);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Structura"));
    app.setWindowIcon(applicationIcon());

    StartupSplash splash(QPixmap(QStringLiteral(":/images/splash.png")));
    splash.show();
    splash.setProgress(8, QObject::tr("Iniciando…"));

    splash.setProgress(25, QObject::tr("Carregando o motor gráfico e os catálogos de perfis…"));
    MainWindow mainWindow;
    mainWindow.resize(1280, 720);
    splash.setProgress(80, QObject::tr("Montando a interface…"));
    mainWindow.show();

    const QStringList arguments = app.arguments();
    if (arguments.size() > 1) {
        splash.setProgress(92, QObject::tr("Abrindo o modelo…"));
        mainWindow.openModelFile(arguments.at(1));
    } else {
        splash.setProgress(92, QObject::tr("Verificando cópias de recuperação…"));
    }
    splash.setProgress(100, QObject::tr("Pronto."));
    splash.finish(&mainWindow);
    if (arguments.size() <= 1) {
        mainWindow.offerRecovery();
    }

    return app.exec();
}
