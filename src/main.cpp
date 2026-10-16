#include "ui/MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>

#include <QVTKOpenGLNativeWidget.h>

int main(int argc, char *argv[])
{
    // Ensure VTK uses the correct OpenGL context inside Qt.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    Q_INIT_RESOURCE(resources);

    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.resize(1280, 720);
    mainWindow.show();

    const QStringList arguments = app.arguments();
    if (arguments.size() > 1) {
        mainWindow.openModelFile(arguments.at(1));
    }

    return app.exec();
}
