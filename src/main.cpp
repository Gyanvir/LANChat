#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // Enable High DPI scaling if supported on older Qt 5 versions
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication a(argc, argv);

    // Set application identity values (used by QSettings)
    QCoreApplication::setOrganizationName("LANChat");
    QCoreApplication::setOrganizationDomain("lanchat.net");
    QCoreApplication::setApplicationName("LANChatClient");

    MainWindow w;
    w.show();

    return a.exec();
}
