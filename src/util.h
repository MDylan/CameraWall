#pragma once
#include <QtCore>
#include <QDir>
#include <QUrl>

namespace Util
{

    static inline QString iniPath()
    {
        const QString dir = QCoreApplication::applicationDirPath();
        return QDir(dir).filePath("cameras.ini");
    }

    static inline QUrl urlFromEncoded(const QString &raw)
    {
        return QUrl::fromEncoded(raw.toUtf8(), QUrl::StrictMode);
    }

    static inline QString dateTimeZuluNow()
    {
        return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }

    static inline QString base64(const QByteArray &b)
    {
        return QString::fromLatin1(b.toBase64());
    }

} // namespace Util
