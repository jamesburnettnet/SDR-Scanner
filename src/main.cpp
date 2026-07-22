#include <QApplication>
#include <QDebug>
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

    bool selfTest = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--test"))
            selfTest = true;
    }

    MainWindow window;

    if (selfTest) {
        // For CI: constructing the whole window wires up every widget,
        // the device manager, and the audio pipeline without crashing --
        // that's the bar for this smoke test. No need to show a window or
        // run the event loop under a headless/offscreen platform.
        qInfo() << "Self-test OK: MainWindow constructed successfully.";
        return 0;
    }

    window.show();

    return QApplication::exec();
}
