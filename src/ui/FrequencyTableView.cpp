#include "FrequencyTableView.h"
#include "AddFrequencyDialog.h"
#include "../core/FrequencyListStore.h"
#include "../core/DebugLog.h"
#include <QTableView>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>

FrequencyTableView::FrequencyTableView(QWidget *parent)
    : QWidget(parent)
{
    m_proxy.setSourceModel(&m_model);
    m_proxy.setFilterKeyColumn(FrequencyListModel::ColGroup);

    m_table = new QTableView(this);
    m_table->setModel(&m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *addBtn = new QPushButton(QStringLiteral("Add..."), this);
    auto *editBtn = new QPushButton(QStringLiteral("Edit..."), this);
    auto *deleteBtn = new QPushButton(QStringLiteral("Delete"), this);
    auto *importBtn = new QPushButton(QStringLiteral("Import..."), this);
    auto *exportBtn = new QPushButton(QStringLiteral("Export..."), this);
    auto *selectAllBtn = new QPushButton(QStringLiteral("Select All"), this);
    auto *deselectAllBtn = new QPushButton(QStringLiteral("Deselect All"), this);
    selectAllBtn->setToolTip(QStringLiteral("Enable all frequencies currently shown (respects the group filter)"));
    deselectAllBtn->setToolTip(QStringLiteral("Disable all frequencies currently shown (respects the group filter)"));

    m_groupFilter = new QComboBox(this);
    m_groupFilter->addItem(QStringLiteral("All groups"));

    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(QStringLiteral("Filter:"), this));
    filterRow->addWidget(m_groupFilter);
    filterRow->addStretch(1);
    filterRow->addWidget(selectAllBtn);
    filterRow->addWidget(deselectAllBtn);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addBtn);
    buttonRow->addWidget(editBtn);
    buttonRow->addWidget(deleteBtn);
    buttonRow->addStretch(1);
    buttonRow->addWidget(importBtn);
    buttonRow->addWidget(exportBtn);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);
    layout->addLayout(buttonRow);

    connect(addBtn, &QPushButton::clicked, this, &FrequencyTableView::addFrequency);
    connect(editBtn, &QPushButton::clicked, this, &FrequencyTableView::editSelected);
    connect(deleteBtn, &QPushButton::clicked, this, &FrequencyTableView::deleteSelected);
    connect(importBtn, &QPushButton::clicked, this, &FrequencyTableView::importList);
    connect(exportBtn, &QPushButton::clicked, this, &FrequencyTableView::exportList);
    connect(selectAllBtn, &QPushButton::clicked, this, [this]() { setEnabledForVisible(true); });
    connect(deselectAllBtn, &QPushButton::clicked, this, [this]() { setEnabledForVisible(false); });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &) { editSelected(); });
    connect(m_groupFilter, &QComboBox::currentTextChanged, this, [this](const QString &) { applyGroupFilter(); });
    connect(&m_model, &FrequencyListModel::listChanged, this, [this]() {
        refreshGroupFilter();
        emit listChanged();
    });

    refreshGroupFilter();
}

QList<int> FrequencyTableView::selectedRows() const
{
    QList<int> rows;
    const auto selected = m_table->selectionModel()->selectedRows();
    for (const auto &idx : selected)
        rows.append(m_proxy.mapToSource(idx).row());
    return rows;
}

void FrequencyTableView::addFrequency()
{
    SDR_LOG("ui") << "Add Frequency dialog opened";
    AddFrequencyDialog dlg(this);
    dlg.setExistingGroups(m_model.allGroups());
    wireCalibration(dlg);
    if (dlg.exec() == QDialog::Accepted) {
        const Frequency f = dlg.frequency();
        SDR_LOG("ui") << "Frequency added:" << f.hz << "Hz label=" << f.label;
        m_model.addFrequency(f);
    } else {
        SDR_LOG("ui") << "Add Frequency dialog cancelled";
    }
}

void FrequencyTableView::editSelected()
{
    const auto rows = selectedRows();
    if (rows.size() != 1)
        return;
    const int row = rows.first();

    AddFrequencyDialog dlg(this);
    dlg.setExistingGroups(m_model.allGroups());
    dlg.setFrequency(m_model.at(row));
    wireCalibration(dlg);
    if (dlg.exec() == QDialog::Accepted)
        m_model.updateFrequency(row, dlg.frequency());
}

void FrequencyTableView::wireCalibration(AddFrequencyDialog &dlg)
{
    if (!m_calibrationFn)
        return;
    connect(&dlg, &AddFrequencyDialog::autoTuneRequested, &dlg, [this, &dlg](qint64 freqHz, Modulation mod) {
        dlg.setCalibrating(true);
        m_calibrationFn(freqHz, mod, [&dlg](bool ok, double noiseFloorDb, double suggestedDb, QString error) {
            if (ok)
                dlg.applyCalibrationResult(noiseFloorDb, suggestedDb);
            else
                dlg.showCalibrationError(error);
        });
    });
}

void FrequencyTableView::deleteSelected()
{
    const auto rows = selectedRows();
    if (rows.isEmpty())
        return;
    if (QMessageBox::question(this, QStringLiteral("Delete"),
                               QStringLiteral("Delete %1 frequency(ies)?").arg(rows.size()))
        != QMessageBox::Yes)
        return;
    m_model.removeRows(rows);
}

void FrequencyTableView::importList()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Frequencies"), QString(),
                                                        QStringLiteral("Frequency List (*.json)"));
    if (path.isEmpty())
        return;
    QVector<Frequency> imported;
    QString error;
    if (!FrequencyListStore::loadJson(path, imported, &error)) {
        QMessageBox::warning(this, QStringLiteral("Import Failed"), error);
        return;
    }
    QVector<Frequency> merged = m_model.items();
    merged += imported;
    m_model.setItems(merged);
}

void FrequencyTableView::exportList()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Frequencies"), QString(),
                                                        QStringLiteral("CSV (*.csv);;Frequency List JSON (*.json)"));
    if (path.isEmpty())
        return;

    QStringList groups;
    if (m_groupFilter->currentIndex() > 0)
        groups << m_groupFilter->currentText();

    QString error;
    bool ok;
    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        QVector<Frequency> toExport;
        for (const auto &f : m_model.items())
            if (groups.isEmpty() || groups.contains(f.group))
                toExport.append(f);
        ok = FrequencyListStore::saveJson(path, toExport, &error);
    } else {
        ok = FrequencyListStore::exportCsv(path, m_model.items(), groups, &error);
    }
    if (!ok)
        QMessageBox::warning(this, QStringLiteral("Export Failed"), error);
}

void FrequencyTableView::refreshGroupFilter()
{
    const QString current = m_groupFilter->currentText();
    m_groupFilter->blockSignals(true);
    m_groupFilter->clear();
    m_groupFilter->addItem(QStringLiteral("All groups"));
    m_groupFilter->addItems(m_model.allGroups());
    const int idx = m_groupFilter->findText(current);
    m_groupFilter->setCurrentIndex(idx >= 0 ? idx : 0);
    m_groupFilter->blockSignals(false);
    applyGroupFilter();
}

void FrequencyTableView::applyGroupFilter()
{
    if (m_groupFilter->currentIndex() <= 0) {
        m_proxy.setFilterFixedString(QString());
    } else {
        m_proxy.setFilterRegularExpression(
            QRegularExpression(QStringLiteral("^%1$").arg(QRegularExpression::escape(m_groupFilter->currentText()))));
    }
}

QList<int> FrequencyTableView::visibleRows() const
{
    QList<int> rows;
    const int count = m_proxy.rowCount();
    rows.reserve(count);
    for (int i = 0; i < count; ++i)
        rows.append(m_proxy.mapToSource(m_proxy.index(i, 0)).row());
    return rows;
}

void FrequencyTableView::setEnabledForVisible(bool enabled)
{
    m_model.setEnabledForRows(visibleRows(), enabled);
}

void FrequencyTableView::setActiveFrequencyId(const QUuid &id)
{
    m_model.setActiveId(id);
    const int sourceRow = m_model.rowForId(id);
    if (sourceRow < 0)
        return;
    const QModelIndex proxyIdx = m_proxy.mapFromSource(m_model.index(sourceRow, 0));
    if (proxyIdx.isValid())
        m_table->scrollTo(proxyIdx);
}
