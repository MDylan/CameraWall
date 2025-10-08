#pragma once
#include <QtCore>
#include <QDir>
#include <QUrl>
#include <QCryptographicHash>

namespace Util
{

    // cameras.ini elérési útja az exe mellé
    static inline QString iniPath()
    {
        const QString dir = QCoreApplication::applicationDirPath();
        return QDir(dir).filePath("cameras.ini");
    }

    // Biztonságos URL-dekódolás
    static inline QUrl urlFromEncoded(const QString &raw)
    {
        return QUrl::fromEncoded(raw.toUtf8(), QUrl::StrictMode);
    }

    // Hasznos segédek (ha az ONVIF kliensed használja)
    static inline QString dateTimeZuluNow()
    {
        return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }
    static inline QString base64(const QByteArray &b)
    {
        return QString::fromLatin1(b.toBase64());
    }
    static inline QString wssePasswordDigest(const QByteArray &nonce,
                                             const QString &created,
                                             const QString &password)
    {
        QByteArray data = nonce + created.toUtf8() + password.toUtf8();
        QByteArray sha1 = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
        return base64(sha1);
    }

    // ONVIF által adott rtsp:// URL-ekbe beégeti a user/pass-t, ha hiányzik
    static inline QUrl withCredentials(QUrl u, const QString &user, const QString &pass)
    {
        if (!user.isEmpty() && u.userName().isEmpty())
            u.setUserName(user);
        if (!pass.isEmpty() && u.password().isEmpty())
            u.setPassword(pass);
        return u;
    }

    // RTSP URL-be injektálja a user:pass@ részt, ha hiányzik.
    // Megőrzi a path / query / fragment részeket, IPv6 hostot is kezeli.
    static inline QUrl applyRtspCredentials(const QUrl &in,
                                            const QString &user,
                                            const QString &pass)
    {
        if (in.scheme().compare("rtsp", Qt::CaseInsensitive) != 0)
            return in;
        if (user.isEmpty() && pass.isEmpty())
            return in;

        // Ha már van user/pass az URL-ben, hagyjuk úgy.
        if (!in.userName().isEmpty() || !in.password().isEmpty())
            return in;

        // Percent-encode a felhasználó/jelszóban (:@/ stb. ne zavarjon bele)
        const QString encUser = QString::fromLatin1(QUrl::toPercentEncoding(user));
        const QString encPass = QString::fromLatin1(QUrl::toPercentEncoding(pass));

        // Host (IPv6-ot [ ] közé tesszük, ha nincs)
        QString host = in.host();
        if (host.contains(':') && !host.startsWith('['))
            host = "[" + host + "]";

        const QString port = (in.port() > 0) ? (":" + QString::number(in.port())) : QString();
        QString path = in.path(QUrl::FullyEncoded);
        if (path.isEmpty()) path = "/";

        const QString query = in.query(QUrl::FullyEncoded);
        const QString frag  = in.fragment(QUrl::FullyEncoded);

        QString url = QStringLiteral("rtsp://%1:%2@%3%4%5")
                        .arg(encUser, encPass, host, port, path);
        if (!query.isEmpty()) url += "?" + query;
        if (!frag.isEmpty())  url += "#" + frag;

        return QUrl::fromEncoded(url.toUtf8(), QUrl::StrictMode);
    }

}
