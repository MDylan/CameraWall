#pragma once
#include <QObject>
#include <QHash>
#include <QString>
#include <QCoreApplication>

class Language : public QObject
{
    Q_OBJECT
public:
    static Language &instance();

    // Betölt nyelvet: "hu", "en", stb. Siker = true, és elmenti az ini-be
    bool load(const QString &code);

    // Parancssori kapcsoló: --lang=hu (nem kötelező, de kényelmes)
    void loadFromArgs(const QCoreApplication &app);

    // Fordítás lekérése kulccsal; fallback szöveggel
    QString t(const QString &key, const QString &fallback = QString()) const;

    QString current() const { return m_code; }

signals:
    void languageChanged(const QString &code);

private:
    Language() = default;
    Q_DISABLE_COPY(Language)

    bool loadFromResource(const QString &code);      // :/lang/xx.json
    bool loadFromExecutableDir(const QString &code); // <exe>/lang/xx.json
    bool loadFromCwd(const QString &code);           // ./lang/xx.json (fejlesztéshez)

    bool loadJsonFile(const QString &path); // közös JSON beolvasó
    void saveChoiceToIni() const;           // ini-be mentés
    void loadChoiceFromIni();               // ini-ből visszatöltés

    QHash<QString, QString> m_map;
    QString m_code{"hu"};
};
