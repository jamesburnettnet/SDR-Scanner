#pragma once

#include <QString>
#include <QVector>
#include "Frequency.h"

// JSON persistence for the user's frequency list, plus CSV export for
// sharing/importing into spreadsheets or other tools.
namespace FrequencyListStore {

// Default save location (Qt app-data dir), used for autosave/autoload.
QString defaultFilePath();

bool saveJson(const QString &path, const QVector<Frequency> &frequencies, QString *errorOut = nullptr);
bool loadJson(const QString &path, QVector<Frequency> &frequenciesOut, QString *errorOut = nullptr);

// Exports only entries whose group is in `groups` (or all entries if
// `groups` is empty) to a simple CSV file.
bool exportCsv(const QString &path, const QVector<Frequency> &frequencies,
                const QStringList &groups, QString *errorOut = nullptr);

} // namespace FrequencyListStore
