#pragma once

#include <QWidget>
#include <QTimer>
#include "../core/ScannerState.h"

// Purely a renderer: polls a ScannerStateHolder on its own QTimer and
// paints the latest snapshot. Deliberately decoupled from the scan
// engine's thread so a slow/expensive repaint can never back-pressure the
// actual scanning/demod work -- worst case the LCD just shows slightly
// stale data for one frame.
class LcdPanel : public QWidget {
    Q_OBJECT
public:
    explicit LcdPanel(QWidget *parent = nullptr);

    void setStateHolder(ScannerStateHolder *holder) { m_holder = holder; }
    QSize sizeHint() const override { return QSize(480, 220); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    ScannerStateHolder *m_holder = nullptr;
    ScannerSnapshot m_last;
    QTimer m_refreshTimer;
};
