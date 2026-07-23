#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QVector>
#include "Frequency.h"

struct ActivityLogEntry {
    QDateTime startTime;
    qint64 freqHz = 0;
    QString label;
    Modulation modulation = Modulation::NFM;
    qint64 durationMs = 0;
};

// Table model backing the activity log: one row per completed
// transmission (squelch open -> close), newest first. Capped so a long
// session doesn't grow this unboundedly.
class ActivityLogModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColTime = 0, ColFrequency, ColLabel, ColDuration, ColumnCount };

    explicit ActivityLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addEntry(const ActivityLogEntry &entry);
    void clear();

private:
    QVector<ActivityLogEntry> m_entries; // newest first
    static constexpr int kMaxEntries = 500;
};
