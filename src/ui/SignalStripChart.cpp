#include "SignalStripChart.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <algorithm>

namespace {
constexpr double kFloorDb = -100.0;
constexpr double kCeilDb = -10.0;

double fracForDb(double db)
{
    double f = (db - kFloorDb) / (kCeilDb - kFloorDb);
    return std::clamp(f, 0.0, 1.0);
}
}

SignalStripChart::SignalStripChart(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(70);
    m_history.reserve(m_maxSamples);

    connect(&m_tickTimer, &QTimer::timeout, this, [this]() {
        Sample s;
        if (m_holder) {
            const ScannerSnapshot snap = m_holder->read();
            s.db = snap.signalDb;
            s.thresholdDb = snap.squelchThresholdDb;
            s.squelchOpen = snap.squelchOpen;
            s.deviceConnected = snap.deviceConnected;
        }
        m_history.append(s);
        while (m_history.size() > m_maxSamples)
            m_history.removeFirst();
        update();
    });
    // Fixed-rate tick (not tied to the scan thread's publish rate) so the
    // chart scrolls smoothly and shows flat stretches when nothing changes,
    // like a real strip chart rather than an event log.
    m_tickTimer.start(33); // ~30fps
}

void SignalStripChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 14, 10));
    p.drawRoundedRect(r, 6, 6);

    const QRectF plot = r.adjusted(8, 8, -8, -8);
    if (plot.width() <= 1 || plot.height() <= 1)
        return;

    const bool connected = !m_history.isEmpty() && m_history.last().deviceConnected;

    if (!connected) {
        p.setPen(QColor(70, 90, 78));
        QFont f = font();
        f.setPointSizeF(10);
        p.setFont(f);
        p.drawText(plot, Qt::AlignCenter, QStringLiteral("NO SIGNAL"));
        return;
    }

    auto yFor = [&](double db) {
        return plot.bottom() - fracForDb(db) * plot.height();
    };

    // Squelch threshold reference line.
    if (!m_history.isEmpty()) {
        const double threshY = yFor(m_history.last().thresholdDb);
        QPen threshPen(QColor(150, 130, 60, 180));
        threshPen.setStyle(Qt::DashLine);
        threshPen.setWidthF(1.0);
        p.setPen(threshPen);
        p.drawLine(QPointF(plot.left(), threshY), QPointF(plot.right(), threshY));
    }

    if (m_history.size() < 2)
        return;

    const double pixelsPerSample = plot.width() / static_cast<double>(m_maxSamples);
    const int n = m_history.size();
    auto xFor = [&](int i) {
        return plot.right() - (n - 1 - i) * pixelsPerSample;
    };

    // Filled area under the curve, for visual weight.
    QPainterPath fillPath;
    fillPath.moveTo(xFor(0), plot.bottom());
    for (int i = 0; i < n; ++i)
        fillPath.lineTo(xFor(i), yFor(m_history[i].db));
    fillPath.lineTo(xFor(n - 1), plot.bottom());
    fillPath.closeSubpath();

    QLinearGradient fillGrad(0, plot.top(), 0, plot.bottom());
    fillGrad.setColorAt(0.0, QColor(70, 190, 140, 70));
    fillGrad.setColorAt(1.0, QColor(70, 190, 140, 5));
    p.setPen(Qt::NoPen);
    p.setBrush(fillGrad);
    p.drawPath(fillPath);

    // Trace line, colored per-segment by whether squelch was open then --
    // bright green bursts show clearly against the dim idle trace.
    for (int i = 1; i < n; ++i) {
        const QColor color = m_history[i].squelchOpen ? QColor(90, 235, 140) : QColor(80, 130, 110);
        QPen pen(color);
        pen.setWidthF(m_history[i].squelchOpen ? 2.0 : 1.3);
        p.setPen(pen);
        p.drawLine(QPointF(xFor(i - 1), yFor(m_history[i - 1].db)), QPointF(xFor(i), yFor(m_history[i].db)));
    }

    // Current level readout, top-left.
    QFont dbFont = font();
    dbFont.setPointSizeF(9);
    dbFont.setBold(true);
    p.setFont(dbFont);
    p.setPen(m_history.last().squelchOpen ? QColor(140, 255, 170) : QColor(120, 160, 140));
    p.drawText(QRectF(plot.left(), plot.top() - 2, 140, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QString::number(m_history.last().db, 'f', 1) + QStringLiteral(" dBFS"));
}
