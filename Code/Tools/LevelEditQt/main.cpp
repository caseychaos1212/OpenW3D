#include "MainWindow.h"

#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("OpenW3D"));
    QCoreApplication::setApplicationName(QStringLiteral("LevelEditQt"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    leveledit_qt::MainWindow window;
    window.show();

    return app.exec();
}
