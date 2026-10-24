#include "ui/MainWindow.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QTabWidget>
#include <QToolButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
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
        // A arte (1536 x 1024) traz uma barra de progresso e uma mensagem fixas: cobre essa area com o fundo da propria imagem e desenha o
        // progresso real no mesmo lugar. As coordenadas abaixo sao da arte original, escaladas para o tamanho exibido.
        const double k = width() / 1536.0;
        const QImage art = pixmap().toImage();
        painter->setPen(Qt::NoPen);
        auto cover = [&](const QRectF &area) {   // area em coordenadas da arte; usa a cor do fundo a esquerda dela
            const QColor background = art.pixelColor(static_cast<int>(60 * k), static_cast<int>(area.center().y() * k));
            painter->setBrush(background);
            painter->drawRect(QRectF(area.left() * k, area.top() * k, area.width() * k, area.height() * k));
        };
        cover(QRectF(76, 908, 696, 36));      // barra de progresso da arte
        cover(QRectF(76, 952, 460, 64));      // mensagem da arte
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF track(82 * k, 918 * k, 680 * k, 20 * k);
        painter->setBrush(QColor(QStringLiteral("#dfe6f0")));
        painter->drawRoundedRect(track, track.height() / 2, track.height() / 2);
        QLinearGradient fill(track.topLeft(), track.topRight());
        fill.setColorAt(0.0, QColor(QStringLiteral("#0b2f8f")));
        fill.setColorAt(1.0, QColor(QStringLiteral("#2f7fd6")));
        painter->setBrush(fill);
        painter->drawRoundedRect(QRectF(track.left(), track.top(), std::max(track.height(), track.width() * m_percent / 100.0), track.height()), track.height() / 2, track.height() / 2);
        QFont font = painter->font();
        font.setPixelSize(std::max(11, static_cast<int>(26 * k)));
        painter->setFont(font);
        painter->setPen(QColor(QStringLiteral("#5a6574")));
        painter->drawText(QRectF(82 * k, 962 * k, 680 * k, 44 * k), Qt::AlignLeft | Qt::AlignVCenter, m_message);
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

namespace {

/// Tamanho inicial da janela; STRUCTURA_WINDOW_SIZE=LxA (so desenvolvimento) serve a verificacao automatizada da interface em varias larguras.
QSize initialWindowSize()
{
    const QStringList size = qEnvironmentVariable("STRUCTURA_WINDOW_SIZE").split(QLatin1Char('x'));
    const int w = size.size() == 2 ? size.at(0).toInt() : 0, h = size.size() == 2 ? size.at(1).toInt() : 0;
    return w >= 640 && h >= 480 ? QSize(w, h) : QSize(1280, 720);
}

/// Ganchos de verificacao automatizada da interface (so desenvolvimento): STRUCTURA_OPEN_PROPERTIES=1 abre o painel de propriedades e
/// STRUCTURA_RIBBON_TAB=n abre a guia n da faixa de opcoes.
void applyUiAutomationHooks(MainWindow &window)
{
    if (qEnvironmentVariableIntValue("STRUCTURA_OPEN_PROPERTIES") > 0) {
        if (auto *button = window.findChild<QToolButton *>(QStringLiteral("propertiesToolButton"))) {
            button->setChecked(true);
        }
    }
    if (qEnvironmentVariableIntValue("STRUCTURA_OPEN_AXES") > 0) {
        if (auto *action = window.findChild<QAction *>(QStringLiteral("axesAction"))) {
            action->trigger();   // abre a janela de eixos de referencia
        }
    }
    if (const int tab = qEnvironmentVariableIntValue("STRUCTURA_RIBBON_TAB"); tab > 0) {
        if (auto *ribbon = window.findChild<QTabWidget *>(QStringLiteral("mainRibbon"))) {
            ribbon->setCurrentIndex(tab);
        }
    }
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

    // a arte da abertura tem 1536 x 1024 px: reduz para 720 px de largura (tamanho de exibicao da abertura)
    StartupSplash splash(QPixmap(QStringLiteral(":/images/splash.png")).scaledToWidth(720, Qt::SmoothTransformation));
    splash.show();
    splash.setProgress(8, QObject::tr("Iniciando…"));

    splash.setProgress(25, QObject::tr("Carregando o motor gráfico e os catálogos de perfis…"));
    if (const QByteArray shot = qgetenv("STRUCTURA_SPLASH_SHOT"); !shot.isEmpty()) {
        splash.grab().save(QString::fromLocal8Bit(shot));   // verificacao automatizada: grava a abertura nesse progresso
    }
    MainWindow mainWindow;
    mainWindow.resize(initialWindowSize());
    splash.setProgress(80, QObject::tr("Montando a interface…"));
    mainWindow.show();

    const QStringList arguments = app.arguments();
    if (arguments.size() > 1) {
        splash.setProgress(92, QObject::tr("Abrindo o modelo…"));
        mainWindow.openModelFile(arguments.at(1));
    } else {
        splash.setProgress(92, QObject::tr("Verificando cópias de recuperação…"));
    }
    applyUiAutomationHooks(mainWindow);   // depois de abrir o modelo (abrir um modelo fecha os paineis)
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
