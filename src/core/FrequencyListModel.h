#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <QUuid>
#include "Frequency.h"

// Table model backing the main frequency list UI. Owns the authoritative
// in-memory list of Frequency entries.
class FrequencyListModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColEnabled = 0,
        ColFrequency,
        ColLabel,
        ColModulation,
        ColSquelch,
        ColumnCount
    };

    explicit FrequencyListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void addFrequency(const Frequency &f);
    void updateFrequency(int row, const Frequency &f);
    void removeRows(const QList<int> &rows);
    const Frequency &at(int row) const { return m_items.at(row); }
    QVector<Frequency> items() const { return m_items; }
    QVector<Frequency> enabledItems() const;
    void setItems(const QVector<Frequency> &items);

    // Sets enabled/disabled for exactly the given rows (used by Select
    // All / Deselect All).
    void setEnabledForRows(const QList<int> &rows, bool enabled);

    // Highlights the row for this Frequency id (e.g. during an active
    // call); pass a null QUuid to clear the highlight.
    void setActiveId(const QUuid &id);
    int rowForId(const QUuid &id) const;

signals:
    void listChanged();

private:
    QVector<Frequency> m_items;
    QUuid m_activeId;
};
