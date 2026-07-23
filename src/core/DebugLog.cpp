#include "DebugLog.h"
#include <QTime>
#include <atomic>

namespace {
std::atomic<bool> g_enabled{false};
}

namespace DebugLog {

void setEnabled(bool enabled)
{
    g_enabled.store(enabled, std::memory_order_relaxed);
}

bool isEnabled()
{
    return g_enabled.load(std::memory_order_relaxed);
}

QString prefix(const char *tag)
{
    return QStringLiteral("[%1][%2]")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), QString::fromLatin1(tag));
}

} // namespace DebugLog
