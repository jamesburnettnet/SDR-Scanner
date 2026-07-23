#pragma once

#include <QDebug>
#include <QString>

// App-wide toggle for verbose diagnostic logging, turned on by passing
// --debug on the command line (see main.cpp). Off by default so normal
// runs stay quiet; SDR_LOG call sites cost only an atomic bool read when
// disabled.
//
// Note for Windows: the app is built without WIN32_EXECUTABLE (see
// CMakeLists.txt), i.e. as a console-subsystem app, so this output shows
// up directly when launched from a terminal (PowerShell/cmd), unlike a
// typical GUI-subsystem exe. Double-clicking the exe still pops a console
// window for it.
namespace DebugLog {
void setEnabled(bool enabled);
bool isEnabled();

// Not for direct use -- see SDR_LOG below.
QString prefix(const char *tag);
} // namespace DebugLog

// Usage: SDR_LOG("engine") << "starting scan, groups=" << count;
// tag identifies which subsystem the line came from (ui/sdr/engine/audio/
// calib) so a --debug log can be skimmed or grepped by area. Arguments
// after the macro are only evaluated when debug logging is enabled.
#define SDR_LOG(tag) \
    if (!DebugLog::isEnabled()) {} else qDebug().noquote() << DebugLog::prefix(tag)
