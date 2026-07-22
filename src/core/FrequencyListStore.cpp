#include "FrequencyListStore.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>

namespace FrequencyListStore {

QString defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("frequencies.json"));
}

bool saveJson(const QString &path, const QVector<Frequency> &frequencies, QString *errorOut)
{
    QJsonArray arr;
    for (const auto &f : frequencies)
        arr.append(f.toJson());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}

bool loadJson(const QString &path, QVector<Frequency> &frequenciesOut, QString *errorOut)
{
    QFile file(path);
    if (!file.exists()) {
        // Not an error: first run, nothing saved yet.
        frequenciesOut.clear();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut) *errorOut = parseError.errorString();
        return false;
    }

    frequenciesOut.clear();
    if (doc.isArray()) {
        for (const auto &v : doc.array())
            frequenciesOut.append(Frequency::fromJson(v.toObject()));
    }
    return true;
}

bool exportCsv(const QString &path, const QVector<Frequency> &frequencies,
                const QStringList &groups, QString *errorOut)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out << "Label,Frequency_MHz,Modulation,Squelch,Group,Enabled\n";
    for (const auto &f : frequencies) {
        if (!groups.isEmpty() && !groups.contains(f.group))
            continue;
        QString label = f.label;
        label.replace('"', "'");
        QString group = f.group;
        group.replace('"', "'");
        const QString squelch = f.autoSquelch ? QStringLiteral("Auto") : QString::number(f.squelchDb, 'f', 1);
        out << '"' << label << "\"," << QString::number(f.mhz(), 'f', 5) << ','
            << modulationToString(f.modulation) << ',' << squelch << ',' << '"' << group << "\","
            << (f.enabled ? "1" : "0") << '\n';
    }
    return true;
}

} // namespace FrequencyListStore
