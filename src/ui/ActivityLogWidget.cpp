#include "ActivityLogWidget.h"
#include <QTableView>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

ActivityLogWidget::ActivityLogWidget(QWidget *parent)
    : QWidget(parent)
{
    m_table = new QTableView(this);
    m_table->setModel(&m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);

    auto *clearButton = new QPushButton(QStringLiteral("Clear"), this);
    connect(clearButton, &QPushButton::clicked, &m_model, &ActivityLogModel::clear);

    auto *headerRow = new QHBoxLayout;
    headerRow->addWidget(new QLabel(QStringLiteral("Activity Log"), this));
    headerRow->addStretch(1);
    headerRow->addWidget(clearButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(headerRow);
    layout->addWidget(m_table, 1);
}

void ActivityLogWidget::addEntry(const QDateTime &startTime, qint64 freqHz, const QString &label,
                                  Modulation modulation, qint64 durationMs)
{
    ActivityLogEntry e;
    e.startTime = startTime;
    e.freqHz = freqHz;
    e.label = label;
    e.modulation = modulation;
    e.durationMs = durationMs;
    m_model.addEntry(e);
}
