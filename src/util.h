#pragma once

#include <QString>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QByteArray>

namespace Util
{

    // INI elérési út
    inline QString iniPath()
    {
        const QString confDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(confDir);
        return confDir + "/camerawall.ini";
    }

    // Nyers (akár már kódolt) sztringből QUrl
    inline QUrl urlFromEncoded(const QString &raw)
    {
        return QUrl::fromEncoded(raw.trimmed().toUtf8());
    }

    // Van-e explicit user/pass az URL-ben?
    inline bool hasUserInfo(const QUrl &u)
    {
        return !u.userName().isEmpty() || !u.password().isEmpty();
    }

    /*
     * RTSP hitelesítés biztosítása:
     * - Ha az URL-ben NINCS user/pass, és kaptunk usert (nem üres),
     *   akkor beírjuk user:pass@ formában az authority-be.
     * - Ha az URL-ben MÁR VAN userinfo, NEM írjuk felül (csak ha overwrite=true).
     */
    inline QUrl ensureRtspCredentials(const QUrl &in,
                                      const QString &user,
                                      const QString &pass,
                                      bool overwrite = false)
    {
        if (user.isEmpty())
            return in;

        QUrl u = in;
        const QString scheme = u.scheme().toLower();
        if (scheme != "rtsp" && scheme != "rtsps")
            return in;

        if (!overwrite && hasUserInfo(u))
            return u;

        u.setUserName(user);
        if (!pass.isEmpty())
            u.setPassword(pass);
        return u;
    }

    // Backward-compat: nem ír felül meglévő userinfo-t
    inline QUrl withCredentials(const QUrl &in, const QString &user, const QString &pass)
    {
        return ensureRtspCredentials(in, user, pass, /*overwrite=*/false);
    }

    /* ---- ONVIF/WSSE segédek ---- */

    // Base64
    inline QString base64(const QByteArray &data)
    {
        return QString::fromLatin1(data.toBase64());
    }

    // ISO 8601 UTC „Z” végződéssel
    inline QString dateTimeZuluNow()
    {
        QString iso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!iso.endsWith('Z'))
            iso += 'Z';
        return iso;
    }

} // namespace Util
