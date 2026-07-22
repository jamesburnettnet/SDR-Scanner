#include "FrequencyListModel.h"
#include <algorithm>
#include <QSet>
#include <QColor>

FrequencyListModel::FrequencyListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int FrequencyListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_items.size();
}

int FrequencyListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant FrequencyListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const Frequency &f = m_items.at(index.row());

    if (role == Qt::CheckStateRole && index.column() == ColEnabled)
        return f.enabled ? Qt::Checked : Qt::Unchecked;

    if (role == Qt::BackgroundRole && !m_activeId.isNull() && f.id == m_activeId)
        return QColor(60, 130, 80); // active-call highlight

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case ColFrequency:  return QString::number(f.mhz(), 'f', 5);
            case ColLabel:      return f.label;
            case ColModulation: return modulationToString(f.modulation);
            case ColSquelch:    return f.autoSquelch ? QStringLiteral("Auto")
                                                       : QString::number(f.squelchDb, 'f', 1);
            case ColGroup:      return f.group;
            default: return {};
        }
    }

    return {};
}

bool FrequencyListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_items.size())
        return false;

    Frequency &f = m_items[index.row()];

    if (role == Qt::CheckStateRole && index.column() == ColEnabled) {
        f.enabled = (value.toInt() == Qt::Checked);
        emit dataChanged(index, index, {role});
        emit listChanged();
        return true;
    }

    if (role == Qt::EditRole) {
        switch (index.column()) {
            case ColFrequency:  f.setMhz(value.toDouble()); break;
            case ColLabel:      f.label = value.toString(); break;
            case ColModulation: f.modulation = modulationFromString(value.toString()); break;
            case ColGroup:      f.group = value.toString(); break;
            default: return false;
        }
        emit dataChanged(index, index);
        emit listChanged();
        return true;
    }

    return false;
}

QVariant FrequencyListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
        case ColEnabled:    return QStringLiteral("On");
        case ColFrequency:  return QStringLiteral("Frequency (MHz)");
        case ColLabel:      return QStringLiteral("Label");
        case ColModulation: return QStringLiteral("Mode");
        case ColSquelch:    return QStringLiteral("Squelch");
        case ColGroup:      return QStringLiteral("Group");
        default: return {};
    }
}

Qt::ItemFlags FrequencyListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ColEnabled)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

void FrequencyListModel::addFrequency(const Frequency &freq)
{
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(freq);
    endInsertRows();
    emit listChanged();
}

void FrequencyListModel::updateFrequency(int row, const Frequency &f)
{
    if (row < 0 || row >= m_items.size())
        return;
    m_items[row] = f;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    emit listChanged();
}

void FrequencyListModel::removeRows(const QList<int> &rows)
{
    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int row : sorted) {
        if (row < 0 || row >= m_items.size())
            continue;
        beginRemoveRows(QModelIndex(), row, row);
        m_items.remove(row);
        endRemoveRows();
    }
    emit listChanged();
}

QVector<Frequency> FrequencyListModel::enabledItems() const
{
    QVector<Frequency> out;
    for (const auto &f : m_items)
        if (f.enabled)
            out.append(f);
    return out;
}

void FrequencyListModel::setItems(const QVector<Frequency> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
    emit listChanged();
}

void FrequencyListModel::setEnabledForRows(const QList<int> &rows, bool enabled)
{
    for (int row : rows) {
        if (row < 0 || row >= m_items.size())
            continue;
        m_items[row].enabled = enabled;
        const QModelIndex idx = index(row, ColEnabled);
        emit dataChanged(idx, idx, {Qt::CheckStateRole});
    }
    if (!rows.isEmpty())
        emit listChanged();
}

void FrequencyListModel::setActiveId(const QUuid &id)
{
    if (m_activeId == id)
        return;

    const int oldRow = rowForId(m_activeId);
    const int newRow = rowForId(id);
    m_activeId = id;

    if (oldRow >= 0)
        emit dataChanged(index(oldRow, 0), index(oldRow, ColumnCount - 1), {Qt::BackgroundRole});
    if (newRow >= 0)
        emit dataChanged(index(newRow, 0), index(newRow, ColumnCount - 1), {Qt::BackgroundRole});
}

int FrequencyListModel::rowForId(const QUuid &id) const
{
    if (id.isNull())
        return -1;
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == id)
            return i;
    return -1;
}

QStringList FrequencyListModel::allGroups() const
{
    QSet<QString> set;
    for (const auto &f : m_items)
        if (!f.group.trimmed().isEmpty())
            set.insert(f.group.trimmed());
    QStringList list = set.values();
    std::sort(list.begin(), list.end());
    return list;
}
