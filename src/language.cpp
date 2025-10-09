#include "language.h"
#include "util.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

Language &Language::instance()
{
    static Language g;
    return g;
}

QString Language::t(const QString &key, const QString &fallback) const
{
    auto it = m_map.constFind(key);
    return (it == m_map.constEnd()) ? fallback : it.value();
}

bool Language::loadFromJsonFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;
    m_map.clear();
    const auto obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        if (it.value().isString())
            m_map.insert(it.key(), it.value().toString());
    }
    return true;
}

void Language::persist()
{
    QSettings s(Util::iniPath(), QSettings::IniFormat);
    s.beginGroup("App");
    s.setValue("lang", m_lang);
    s.endGroup();
    s.sync();
}

bool Language::load(const QString &langCode)
{
    // Próbálja az app mappából: lang/<code>.json, majd resource-ból :/lang/<code>.json
    const QString disk = QCoreApplication::applicationDirPath() + "/lang/" + langCode + ".json";
    const QString rsrc = QString(":/lang/%1.json").arg(langCode);
    if (!loadFromJsonFile(disk) && !loadFromJsonFile(rsrc))
        return false;
    m_lang = langCode;
    persist();
    emit languageChanged(m_lang);
    return true;
}

void Language::loadFromArgs(const QCoreApplication &app)
{
    // 1) parancssor: --lang=hu|en
    const auto args = app.arguments();
    for (const QString &a : args)
    {
        if (a.startsWith("--lang="))
        {
            const QString v = a.mid(QString("--lang=").size());
            if (load(v))
                return; // siker → kész
        }
    }
    // 2) INI
    {
        QSettings s(Util::iniPath(), QSettings::IniFormat);
        s.beginGroup("App");
        const QString fromIni = s.value("lang", "hu").toString();
        s.endGroup();
        if (load(fromIni))
            return;
    }
    // 3) végső fallback
    load("hu");
}
