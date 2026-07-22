#include "Frequency.h"

QString modulationToString(Modulation m)
{
    switch (m) {
        case Modulation::FM:  return QStringLiteral("FM");
        case Modulation::NFM: return QStringLiteral("NFM");
        case Modulation::AM:  return QStringLiteral("AM");
    }
    return QStringLiteral("NFM");
}

Modulation modulationFromString(const QString &s)
{
    const QString u = s.trimmed().toUpper();
    if (u == QStringLiteral("FM")) return Modulation::FM;
    if (u == QStringLiteral("AM")) return Modulation::AM;
    return Modulation::NFM;
}

QJsonObject Frequency::toJson() const
{
    QJsonObject o;
    o["id"] = id.toString(QUuid::WithoutBraces);
    o["label"] = label;
    o["hz"] = static_cast<double>(hz);
    o["modulation"] = modulationToString(modulation);
    o["enabled"] = enabled;
    o["autoSquelch"] = autoSquelch;
    o["squelchDb"] = squelchDb;
    o["group"] = group;
    return o;
}

Frequency Frequency::fromJson(const QJsonObject &obj)
{
    Frequency f;
    const QString idStr = obj.value("id").toString();
    f.id = idStr.isEmpty() ? QUuid::createUuid() : QUuid(idStr);
    if (f.id.isNull())
        f.id = QUuid::createUuid();
    f.label = obj.value("label").toString();
    f.hz = static_cast<qint64>(obj.value("hz").toDouble());
    f.modulation = modulationFromString(obj.value("modulation").toString());
    f.enabled = obj.value("enabled").toBool(true);
    f.autoSquelch = obj.value("autoSquelch").toBool(true);
    f.squelchDb = obj.value("squelchDb").toDouble(-50.0);
    f.group = obj.value("group").toString();
    return f;
}
