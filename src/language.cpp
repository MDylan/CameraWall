#include "language.h"
#include "util.h"

#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QFileInfo>

Language &Language::instance()
{
    static Language inst;
    return inst;
}

void Language::loadFromArgs(const QCoreApplication &app)
{
    // Először ini-ből töltsük, hogy legyen alapértelmezett
    loadChoiceFromIni();

    const QStringList args = app.arguments();
    for (const QString &a : args)
    {
        if (a.startsWith("--lang="))
        {
            const QString code = a.mid(QStringLiteral("--lang=").size()).trimmed();
            if (!code.isEmpty())
            {
                load(code); // load() maga elintézi a jelzéseket és az ini mentést
            }
            break;
        }
    }
}

bool Language::load(const QString &code)
{
    qDebug() << "load " << code;
    qDebug() << "QRC list :/lang =" << QDir(":/lang").entryList();
    qDebug() << "hu exists?" << QFileInfo::exists(":/lang/hu.json");
    
    if (code.isEmpty())
        return false;

    // 1) resource
    if (!loadFromResource(code))
    {
        qDebug() << "loadFromResource nem talált ... ";
        // 2) exe melletti lang/XX.json
        if (!loadFromExecutableDir(code))
        {
            qDebug() << "loadFromExecutableDir nem talált ... ";
            // 3) current working dir (fejlesztéshez)
            if (!loadFromCwd(code))
            {
                qDebug() << "egyik se talált ... ";
                return false; // semmi sem sikerült
            }
        }
    }

    // siker: állapot, jelzés, mentés
    m_code = code;
    saveChoiceToIni();
    emit languageChanged(m_code);
    return true;
}

QString Language::t(const QString &key, const QString &fallback) const
{
    auto it = m_map.constFind(key);
    if (it == m_map.constEnd())
        return fallback;
    return it.value();
}

bool Language::loadFromResource(const QString &code)
{
    qDebug() << "[Lang] listing in :/lang ->" << QDir(":/lang").entryList();
    const QString path = QStringLiteral(":/lang/%1.json").arg(code);
    qDebug() << "[Lang] try open:" << path << " exists? " << QFileInfo::exists(path);

    return loadJsonFile(path);
}

bool Language::loadFromExecutableDir(const QString &code)
{
    const QString dir = QCoreApplication::applicationDirPath() + "/lang";
    const QString path = dir + QStringLiteral("/%1.json").arg(code);
    return loadJsonFile(path);
}

bool Language::loadFromCwd(const QString &code)
{
    const QString dir = QDir::currentPath() + "/lang";
    const QString path = dir + QStringLiteral("/%1.json").arg(code);
    return loadJsonFile(path);
}

bool Language::loadJsonFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning() << "[Lang] open failed:" << path << " error:" << f.errorString();
        return false;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
    {
        qWarning() << "[Lang] invalid json at" << path;
        return false;
    }
    const auto obj = doc.object();
    m_map.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        m_map.insert(it.key(), it.value().toString());
    return true;
}

void Language::saveChoiceToIni() const
{
    QSettings s(Util::iniPath(), QSettings::IniFormat);
    s.beginGroup("Language");
    s.setValue("current", m_code);
    s.endGroup();
    s.sync();
}

void Language::loadChoiceFromIni()
{
    QSettings s(Util::iniPath(), QSettings::IniFormat);
    s.beginGroup("Language");
    const QString code = s.value("current", m_code).toString();
    s.endGroup();

    // Ha találunk resource-ot/állományt a kért kódhoz, töltsük be:
    if (!code.isEmpty())
    {
        // csendben próbálkozunk, jelzés nélkül
        if (loadFromResource(code) || loadFromExecutableDir(code) || loadFromCwd(code))
        {
            m_code = code;
        }
    }
}
