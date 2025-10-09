#pragma once
#include <QObject>
#include <QHash>
#include <QString>

class QCoreApplication;

class Language : public QObject
{
    Q_OBJECT
public:
    static Language &instance();

    // kulcs → fordítás; ha nincs, fallback
    QString t(const QString &key, const QString &fallback) const;

    // "hu" / "en" betöltése (JSON), INI-be menti, jelez
    bool load(const QString &langCode);

    // --lang=hu|en parancssorból; ha nincs, INI-ből; ha az sincs, "hu"
    void loadFromArgs(const QCoreApplication &app);

    QString current() const { return m_lang; }

signals:
    void languageChanged(const QString &code);

private:
    Language() = default;
    void persist(); // INI-be ment
    bool loadFromJsonFile(const QString &path);

    QHash<QString, QString> m_map;
    QString m_lang = "hu";
};
