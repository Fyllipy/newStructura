#include "ui/MainWindow.h"

#include <QApplication>
#include <QTabWidget>
#include <QToolButton>
#include <QTimer>
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
    {
        // tamanho inicial (a variavel STRUCTURA_WINDOW_SIZE=LxA serve a verificacao automatizada da interface em varias larguras)
        const QStringList size = qEnvironmentVariable("STRUCTURA_WINDOW_SIZE").split(QLatin1Char('x'));
        const int w = size.size() == 2 ? size.at(0).toInt() : 0, h = size.size() == 2 ? size.at(1).toInt() : 0;
        mainWindow.resize(w >= 640 && h >= 480 ? QSize(w, h) : QSize(1280, 720));
    }
    splash.setProgress(80, QObject::tr("Montando a interface…"));
    mainWindow.show();
    if (qEnvironmentVariableIntValue("STRUCTURA_OPEN_PROPERTIES") > 0) {
        if (auto *button = mainWindow.findChild<QToolButton *>(QStringLiteral("propertiesToolButton"))) {
            button->setChecked(true);   // verificacao automatizada: abre o painel de propriedades
        }
    }
    if (const int tab = qEnvironmentVariableIntValue("STRUCTURA_RIBBON_TAB"); tab > 0) {
        if (auto *ribbon = mainWindow.findChild<QTabWidget *>(QStringLiteral("mainRibbon"))) {
            ribbon->setCurrentIndex(tab);   // verificacao automatizada: abre a guia indicada
        }
    }

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

    const QString autoShot = qEnvironmentVariable("STRUCTURA_AUTOSHOT");
    if (!autoShot.isEmpty()) {
        QTimer::singleShot(2500, &mainWindow, [&mainWindow, autoShot]() {
            mainWindow.automatedCapture(autoShot);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
