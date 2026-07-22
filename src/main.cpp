#include <QApplication>
#include "ui/MainWindow.h"
#include "core/Frequency.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SDR Scanner"));
    QApplication::setOrganizationName(QStringLiteral("SDR-Scanner"));

    // Modulation crosses threads in ScanEngine::activityLogged (a queued
    // connection), so it needs to be known to the meta-object system.
    qRegisterMetaType<Modulation>("Modulation");

    MainWindow window;
    window.show();

    return QApplication::exec();
}
