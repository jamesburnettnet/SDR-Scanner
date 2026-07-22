#include "ActivityLogModel.h"

ActivityLogModel::ActivityLogModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ActivityLogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_entries.size();
}

int ActivityLogModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant ActivityLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    if (role != Qt::DisplayRole)
        return {};

    const ActivityLogEntry &e = m_entries.at(index.row());
    switch (index.column()) {
        case ColTime:      return e.startTime.toString(QStringLiteral("HH:mm:ss"));
        case ColFrequency:  return QString::number(e.freqHz / 1'000'000.0, 'f', 4) + QStringLiteral(" MHz");
        case ColLabel:      return e.label.isEmpty() ? QStringLiteral("(unlabeled)") : e.label;
        case ColDuration:   return QString::number(e.durationMs / 1000.0, 'f', 1) + QStringLiteral("s");
        default: return {};
    }
}

QVariant ActivityLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
        case ColTime:      return QStringLiteral("Time");
        case ColFrequency: return QStringLiteral("Frequency");
        case ColLabel:     return QStringLiteral("Label");
        case ColDuration:  return QStringLiteral("Duration");
        default: return {};
    }
}

void ActivityLogModel::addEntry(const ActivityLogEntry &entry)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_entries.prepend(entry);
    endInsertRows();

    if (m_entries.size() > kMaxEntries) {
        beginRemoveRows(QModelIndex(), kMaxEntries, m_entries.size() - 1);
        m_entries.resize(kMaxEntries);
        endRemoveRows();
    }
}

void ActivityLogModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}
