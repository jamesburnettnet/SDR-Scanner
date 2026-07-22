#include "LcdPanel.h"
#include <QPainter>
#include <QLinearGradient>
#include <QFontDatabase>
#include <algorithm>

namespace {
QColor statusColor(EngineMode mode, bool deviceConnected)
{
    if (!deviceConnected) return QColor(120, 120, 120);
    switch (mode) {
        case EngineMode::Idle:      return QColor(120, 120, 120);
        case EngineMode::Scanning:  return QColor(70, 220, 120);
        case EngineMode::Holding:   return QColor(255, 190, 40);
        case EngineMode::Exploring: return QColor(90, 190, 255);
    }
    return Qt::gray;
}

QString statusText(EngineMode mode, bool deviceConnected)
{
    if (!deviceConnected) return QStringLiteral("NO DEVICE");
    switch (mode) {
        case EngineMode::Idle:      return QStringLiteral("IDLE");
        case EngineMode::Scanning:  return QStringLiteral("SCAN");
        case EngineMode::Holding:   return QStringLiteral("HOLD");
        case EngineMode::Exploring: return QStringLiteral("EXPLORE");
    }
    return QString();
}
}

LcdPanel::LcdPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(420, 200);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (m_holder) {
            ScannerSnapshot cur = m_holder->read();
            if (cur.updateCounter != m_last.updateCounter) {
                m_last = cur;
                update();
            }
        }
    });
    m_refreshTimer.start(33); // ~30fps, independent of scan-thread rate
}

void LcdPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);

    // Panel background: dark bezel with an inset "glass" screen.
    QLinearGradient bezel(r.topLeft(), r.bottomLeft());
    bezel.setColorAt(0, QColor(40, 42, 46));
    bezel.setColorAt(1, QColor(20, 21, 24));
    p.setPen(Qt::NoPen);
    p.setBrush(bezel);
    p.drawRoundedRect(r, 10, 10);

    const QRectF screen = r.adjusted(10, 10, -10, -10);
    p.setBrush(QColor(8, 14, 10));
    p.drawRoundedRect(screen, 6, 6);

    const QColor accent = statusColor(m_last.mode, m_last.deviceConnected);

    // Status pill, top-right.
    const QString status = statusText(m_last.mode, m_last.deviceConnected);
    QFont statusFont = font();
    statusFont.setBold(true);
    statusFont.setPointSizeF(10);
    p.setFont(statusFont);
    QFontMetrics sfm(statusFont);
    const int pillW = sfm.horizontalAdvance(status) + 20;
    QRectF pillRect(screen.right() - pillW - 10, screen.top() + 10, pillW, 22);
    p.setBrush(accent.darker(160));
    p.setPen(QPen(accent, 1));
    p.drawRoundedRect(pillRect, 11, 11);
    p.setPen(accent);
    p.drawText(pillRect, Qt::AlignCenter, status);

    // Group/channel progress, top-left.
    QFont smallFont = font();
    smallFont.setPointSizeF(9);
    p.setFont(smallFont);
    p.setPen(QColor(90, 140, 110));
    QString groupText;
    if (m_last.groupCount > 0)
        groupText = QStringLiteral("GROUP %1/%2  CH %3/%4")
                        .arg(m_last.groupIndex + 1).arg(m_last.groupCount)
                        .arg(m_last.channelIndex + 1).arg(m_last.channelCount);
    p.drawText(QRectF(screen.left() + 14, screen.top() + 8, screen.width() - 100, 20),
               Qt::AlignLeft | Qt::AlignVCenter, groupText);

    // Big frequency readout.
    QFont freqFont(QStringLiteral("Monospace"));
    freqFont.setStyleHint(QFont::TypeWriter);
    freqFont.setBold(true);
    freqFont.setPointSizeF(34);
    p.setFont(freqFont);
    const QColor digitColor = m_last.deviceConnected ? QColor(140, 255, 170) : QColor(70, 90, 78);
    p.setPen(digitColor);
    const QString freqStr = m_last.deviceConnected && m_last.currentFreqHz > 0
        ? QString::number(m_last.currentFreqHz / 1'000'000.0, 'f', 4) + QStringLiteral(" MHz")
        : QStringLiteral("--.---- MHz");
    QRectF freqRect(screen.left() + 10, screen.top() + 34, screen.width() - 20, 56);
    p.drawText(freqRect, Qt::AlignLeft | Qt::AlignVCenter, freqStr);

    // Label + mode line.
    QFont labelFont = font();
    labelFont.setPointSizeF(13);
    p.setFont(labelFont);
    p.setPen(QColor(170, 220, 190));
    const QString modeStr = modulationToString(m_last.currentModulation);
    QString labelLine = m_last.currentLabel.isEmpty() ? QStringLiteral("(unlabeled)") : m_last.currentLabel;
    labelLine += QStringLiteral("   [") + modeStr + QStringLiteral("]");
    p.drawText(QRectF(screen.left() + 12, screen.top() + 92, screen.width() - 24, 24),
               Qt::AlignLeft | Qt::AlignVCenter, labelLine);

    // Signal meter bar.
    const double meterFloor = -100.0, meterCeil = -10.0;
    double frac = (m_last.signalDb - meterFloor) / (meterCeil - meterFloor);
    frac = std::clamp(frac, 0.0, 1.0);

    QRectF meterRect(screen.left() + 12, screen.bottom() - 34, screen.width() - 24, 14);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRoundedRect(meterRect, 4, 4);

    QRectF fillRect = meterRect;
    fillRect.setWidth(meterRect.width() * frac);
    QLinearGradient meterGrad(fillRect.topLeft(), fillRect.topRight());
    meterGrad.setColorAt(0.0, QColor(70, 220, 120));
    meterGrad.setColorAt(0.75, QColor(230, 210, 60));
    meterGrad.setColorAt(1.0, QColor(230, 70, 60));
    p.setBrush(m_last.squelchOpen ? QBrush(meterGrad) : QBrush(QColor(60, 110, 80)));
    p.drawRoundedRect(fillRect, 4, 4);

    QFont dbFont = font();
    dbFont.setPointSizeF(8);
    p.setFont(dbFont);
    p.setPen(QColor(120, 160, 140));
    const QString dbStr = m_last.deviceConnected ? QString::number(m_last.signalDb, 'f', 1) + QStringLiteral(" dBFS") : QString();
    p.drawText(QRectF(screen.left() + 12, screen.bottom() - 52, screen.width() - 24, 16),
               Qt::AlignLeft | Qt::AlignVCenter, dbStr);
}
