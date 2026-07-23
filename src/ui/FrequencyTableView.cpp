#include "FrequencyTableView.h"
#include "AddFrequencyDialog.h"
#include "../core/FrequencyListStore.h"
#include "../core/DebugLog.h"
#include <QTableView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>

FrequencyTableView::FrequencyTableView(QWidget *parent)
    : QWidget(parent)
{
    m_proxy.setSourceModel(&m_model);
    // Sort on EditRole, not the default DisplayRole: FrequencyListModel
    // returns a numeric double for ColFrequency under EditRole so "9 MHz"
    // sorts before "10 MHz" instead of lexicographically after it.
    m_proxy.setSortRole(Qt::EditRole);

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
    selectAllBtn->setToolTip(QStringLiteral("Enable all frequencies"));
    deselectAllBtn->setToolTip(QStringLiteral("Disable all frequencies"));

    auto *selectionRow = new QHBoxLayout;
    selectionRow->addStretch(1);
    selectionRow->addWidget(selectAllBtn);
    selectionRow->addWidget(deselectAllBtn);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addBtn);
    buttonRow->addWidget(editBtn);
    buttonRow->addWidget(deleteBtn);
    buttonRow->addStretch(1);
    buttonRow->addWidget(importBtn);
    buttonRow->addWidget(exportBtn);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(selectionRow);
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
    connect(&m_model, &FrequencyListModel::listChanged, this, &FrequencyTableView::listChanged);
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

    QString error;
    bool ok;
    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        ok = FrequencyListStore::saveJson(path, m_model.items(), &error);
    else
        ok = FrequencyListStore::exportCsv(path, m_model.items(), &error);
    if (!ok)
        QMessageBox::warning(this, QStringLiteral("Export Failed"), error);
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
