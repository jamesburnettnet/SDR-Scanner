#include <QApplication>
#include <QDebug>
#include "ui/MainWindow.h"
#include "core/Frequency.h"
#include "core/DebugLog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SDR Scanner"));
    QApplication::setOrganizationName(QStringLiteral("SDR-Scanner"));

    // Modulation crosses threads in ScanEngine::activityLogged (a queued
    // connection), so it needs to be known to the meta-object system.
    qRegisterMetaType<Modulation>("Modulation");

    bool selfTest = false;
    bool debugLogging = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--test"))
            selfTest = true;
        else if (arg == QStringLiteral("--debug"))
            debugLogging = true;
    }
    DebugLog::setEnabled(debugLogging);
    if (debugLogging)
        qInfo() << "Debug logging enabled (--debug): UI actions, device open/close, scan "
                   "engine lifecycle, and audio sink state will be logged to stderr.";

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
