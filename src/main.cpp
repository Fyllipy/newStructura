#include "ui/MainWindow.h"

#include "app/Product.h"
#include "app/services/UserDataMigration.h"

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

/// Tela de abertura (images/splash.png, trocavel) com mensagem e barra de progresso na base.
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
        // A arte (950 x 713) ja traz logotipo, descricao e versao; so se desenha por cima o progresso real (faixa fina na base, mensagem
        // acima dela, a direita) e, na edicao Demonstrativa, a identificacao. Coordenadas da arte, escaladas para o tamanho exibido.
        const double k = width() / 950.0;
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);

        const QRectF track(0, height() - 7 * k, width(), 7 * k);
        painter->setBrush(QColor(255, 255, 255, 70));
        painter->drawRect(track);
        QLinearGradient fill(track.topLeft(), track.topRight());
        fill.setColorAt(0.0, QColor(QStringLiteral("#0b4fc0")));
        fill.setColorAt(1.0, QColor(QStringLiteral("#3d8fe6")));
        painter->setBrush(fill);
        painter->drawRect(QRectF(track.left(), track.top(), track.width() * m_percent / 100.0, track.height()));

        QFont font = painter->font();
        font.setPixelSize(std::max(10, static_cast<int>(15 * k)));
        painter->setFont(font);
        painter->setPen(QColor(255, 255, 255, 230));
        painter->drawText(QRectF(width() * 0.40, height() - 38 * k, width() * 0.58, 26 * k), Qt::AlignRight | Qt::AlignVCenter, m_message);

        if (Structura::Product::kIsDemo) {
            // faixa abaixo do logotipo; a arte e a mesma nas duas edicoes
            QFont badgeFont = painter->font();
            badgeFont.setPixelSize(std::max(10, static_cast<int>(17 * k)));
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            const QRectF badge(110 * k, 462 * k, 194 * k, 30 * k);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, 235));
            painter->drawRoundedRect(badge, badge.height() / 2, badge.height() / 2);
            painter->setPen(QColor(QStringLiteral("#0b2f8f")));
            painter->drawText(badge, Qt::AlignCenter, QObject::tr("DEMONSTRAÇÃO"));
        }
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
    // identidade centralizada em cmake/ProductInfo.cmake; kAppId define a pasta de dados e as configuracoes do usuario (uma por edicao)
    app.setApplicationName(QString::fromUtf8(Structura::Product::kAppId));
    app.setApplicationVersion(QString::fromUtf8(Structura::Product::kVersion));
    Structura::App::Services::UserDataMigration::runIfNeeded();   // Comercial: copia (sem apagar) os dados da identidade antiga "Structura"
    app.setWindowIcon(applicationIcon());

    // a arte da abertura tem 950 x 713 px: exibida com 720 px de largura
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
