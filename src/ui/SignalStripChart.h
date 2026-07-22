#pragma once

#include <QWidget>
#include <QTimer>
#include <QVector>
#include "../core/ScannerState.h"

// A scrolling signal-strength-over-time strip chart -- like the level
// trace on a real scanner/SDR panel, not a waterfall/spectrum display.
// Complements LcdPanel's instantaneous bar meter by showing recent
// history, so you can see bursts, chatter, and how close a signal sits to
// the squelch threshold over time.
//
// Same decoupling as LcdPanel: polls ScannerStateHolder on its own timer
// at a fixed rate, independent of the scan thread, so a slow repaint can
// never back-pressure scanning.
class SignalStripChart : public QWidget {
    Q_OBJECT
public:
    explicit SignalStripChart(QWidget *parent = nullptr);

    void setStateHolder(ScannerStateHolder *holder) { m_holder = holder; }
    QSize sizeHint() const override { return QSize(420, 100); }
    QSize minimumSizeHint() const override { return QSize(200, 70); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Sample {
        double db = -120.0;
        double thresholdDb = -50.0;
        bool squelchOpen = false;
        bool deviceConnected = false;
    };

    ScannerStateHolder *m_holder = nullptr;
    QVector<Sample> m_history;
    int m_maxSamples = 150; // ~5s of history at the timer's 30fps tick rate
    QTimer m_tickTimer;
};
