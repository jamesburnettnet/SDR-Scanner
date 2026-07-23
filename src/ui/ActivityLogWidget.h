#pragma once

#include <QWidget>
#include "../core/ActivityLogModel.h"

class QTableView;

// Displays the activity log: one row per completed transmission, across
// every channel in whichever group is currently tuned (not just the one
// being displayed), newest first.
class ActivityLogWidget : public QWidget {
    Q_OBJECT
public:
    explicit ActivityLogWidget(QWidget *parent = nullptr);

    void addEntry(const QDateTime &startTime, qint64 freqHz, const QString &label, Modulation modulation,
                  qint64 durationMs);

private:
    ActivityLogModel m_model;
    QTableView *m_table;
};
