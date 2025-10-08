#pragma once
#include <QtCore>
#include <QtNetwork>

struct OnvifProfile
{
    QString token;
    QString name;
    QString encoding;
    QSize resolution;
};

class OnvifClient
{
public:
    OnvifClient() = default;

    bool getCapabilities(const QUrl &deviceXAddr, const QString &user, const QString &pass,
                         QUrl &mediaXAddr, QString *err = nullptr);

    bool getProfiles(const QUrl &mediaXAddr, const QString &user, const QString &pass,
                     QList<OnvifProfile> &out, QString *err = nullptr);

    bool getStreamUri(const QUrl &mediaXAddr, const QString &user, const QString &pass,
                      const QString &profileToken, QString &rtspUri, QString *err = nullptr);

private:
    static void addCommonHeaders(QNetworkRequest &nr, const char *soapAction);
    static QByteArray envelope(const QString &bodyXml, const QString &user, const QString &pass);
    static bool postSync(const QNetworkRequest &nr, const QByteArray &payload,
                         QByteArray &out, QString *err);
    static void parseProfiles(const QByteArray &xml, QList<OnvifProfile> &out);
    static QString wssePasswordDigest(const QByteArray &nonce, const QString &created, const QString &password);
};
