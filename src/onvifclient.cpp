#include "onvifclient.h"
#include "util.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QXmlStreamReader>

using namespace Util;

void OnvifClient::addCommonHeaders(QNetworkRequest &nr, const char *soapAction)
{
    nr.setHeader(QNetworkRequest::ContentTypeHeader, "application/soap+xml; charset=utf-8");
    nr.setRawHeader("SOAPAction", soapAction);
    nr.setTransferTimeout(7000);
}

QString OnvifClient::wssePasswordDigest(const QByteArray &nonce, const QString &created, const QString &password)
{
    QByteArray data = nonce + created.toUtf8() + password.toUtf8();
    QByteArray sha1 = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
    return base64(sha1);
}

QByteArray OnvifClient::envelope(const QString &bodyXml, const QString &user, const QString &pass)
{
    QByteArray nonce(16, '\0');
    auto *rng = QRandomGenerator::system();
    for (int i = 0; i < nonce.size(); ++i)
        nonce[i] = char(rng->bounded(256));
    const QString created = dateTimeZuluNow();
    const QString digest = wssePasswordDigest(nonce, created, pass);
    const QString nonceB64 = base64(nonce);

    const QString hdr = QString::fromUtf8(R"(
<wsse:Security xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd"
               xmlns:wsu ="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">
  <wsse:UsernameToken>
    <wsse:Username>%1</wsse:Username>
    <wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">%2</wsse:Password>
    <wsse:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">%3</wsse:Nonce>
    <wsu:Created>%4</wsu:Created>
  </wsse:UsernameToken>
</wsse:Security>)")
                            .arg(user.toHtmlEscaped(), digest, nonceB64, created);

    const QString env = QString::fromUtf8(R"(<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">
  <soap:Header>%1</soap:Header>
  <soap:Body>%2</soap:Body>
</soap:Envelope>)")
                            .arg(hdr, bodyXml);

    return env.toUtf8();
}

bool OnvifClient::postSync(const QNetworkRequest &nr, const QByteArray &payload, QByteArray &out, QString *err)
{
    QNetworkAccessManager nam;
    QNetworkReply *rp = nam.post(nr, payload);
    QEventLoop loop;
    QObject::connect(rp, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(8000);
    loop.exec();
    if (rp->isRunning())
    {
        rp->abort();
        rp->deleteLater();
        if (err)
            *err = "Timeout during ONVIF request.";
        return false;
    }
    if (rp->error() != QNetworkReply::NoError)
    {
        if (err)
            *err = rp->errorString();
        rp->deleteLater();
        return false;
    }
    out = rp->readAll();
    rp->deleteLater();
    return true;
}

bool OnvifClient::getCapabilities(const QUrl &deviceXAddr, const QString &user, const QString &pass,
                                  QUrl &mediaXAddr, QString *err)
{
    const QString body =
        R"(<tds:GetCapabilities xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><tds:Category>All</tds:Category></tds:GetCapabilities>)";
    QByteArray req = envelope(body, user, pass);
    QNetworkRequest nr(deviceXAddr);
    addCommonHeaders(nr, "http://www.onvif.org/ver10/device/wsdl/GetCapabilities");
    QByteArray resp;
    if (!postSync(nr, req, resp, err))
        return false;

    QXmlStreamReader xr(resp);
    while (!xr.atEnd())
    {
        xr.readNext();
        if (xr.isStartElement())
        {
            const QString name = xr.name().toString();
            if (name.endsWith("XAddr", Qt::CaseInsensitive))
            {
                const QString text = xr.readElementText().trimmed();
                if (text.contains("/media", Qt::CaseInsensitive))
                {
                    mediaXAddr = QUrl(text);
                    return true;
                }
            }
        }
    }
    if (err)
        *err = "I couldn't find Media XAddr in the GetCapabilities response.";
    return false;
}

bool OnvifClient::getProfiles(const QUrl &mediaXAddr, const QString &user, const QString &pass,
                              QList<OnvifProfile> &out, QString *err)
{
    const QString body = R"(<trt:GetProfiles xmlns:trt="http://www.onvif.org/ver10/media/wsdl"/>)";
    QByteArray req = envelope(body, user, pass);
    QNetworkRequest nr(mediaXAddr);
    addCommonHeaders(nr, "http://www.onvif.org/ver10/media/wsdl/GetProfiles");
    QByteArray resp;
    if (!postSync(nr, req, resp, err))
        return false;
    parseProfiles(resp, out);
    if (out.isEmpty())
    {
        if (err)
            *err = "I did not receive any ONVIF profiles back.";
        return false;
    }
    return true;
}

bool OnvifClient::getStreamUri(const QUrl &mediaXAddr, const QString &user, const QString &pass,
                               const QString &profileToken, QString &rtspUri, QString *err)
{
    const QString body = QString::fromUtf8(R"(
<trt:GetStreamUri xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
  <trt:StreamSetup>
    <tt:Stream>RTP-Unicast</tt:Stream>
    <tt:Transport><tt:Protocol>RTSP</tt:Protocol></tt:Transport>
  </trt:StreamSetup>
  <trt:ProfileToken>%1</trt:ProfileToken>
</trt:GetStreamUri>)")
                             .arg(profileToken.toHtmlEscaped());

    QByteArray req = envelope(body, user, pass);
    QNetworkRequest nr(mediaXAddr);
    addCommonHeaders(nr, "http://www.onvif.org/ver10/media/wsdl/GetStreamUri");
    QByteArray resp;
    if (!postSync(nr, req, resp, err))
        return false;

    QXmlStreamReader xr(resp);
    while (!xr.atEnd())
    {
        xr.readNext();
        if (xr.isStartElement() && xr.name().toString().compare("Uri", Qt::CaseInsensitive) == 0)
        {
            rtspUri = xr.readElementText().trimmed();
            return !rtspUri.isEmpty();
        }
    }
    if (err)
        *err = "I couldn't find a Uri field in the GetStreamUri response.";
    return false;
}

void OnvifClient::parseProfiles(const QByteArray &xml, QList<OnvifProfile> &out)
{
    QXmlStreamReader xr(xml);
    OnvifProfile cur;
    bool inProf = false, inEnc = false, inRes = false;
    int w = 0, h = 0;
    while (!xr.atEnd())
    {
        xr.readNext();
        if (xr.isStartElement())
        {
            const QString name = xr.name().toString();
            if (!inProf && name.endsWith("Profiles", Qt::CaseInsensitive))
            {
                inProf = true;
                cur = OnvifProfile{};
                auto attrs = xr.attributes();
                if (attrs.hasAttribute("token"))
                    cur.token = attrs.value("token").toString();
            }
            else if (inProf && name.contains("Name", Qt::CaseInsensitive))
            {
                cur.name = xr.readElementText().trimmed();
            }
            else if (inProf && name.contains("VideoEncoderConfiguration", Qt::CaseInsensitive))
            {
                inEnc = true;
                cur.encoding.clear();
            }
            else if (inEnc && name.contains("Encoding", Qt::CaseInsensitive))
            {
                cur.encoding = xr.readElementText().trimmed();
            }
            else if (inEnc && name.contains("Resolution", Qt::CaseInsensitive))
            {
                inRes = true;
                w = 0;
                h = 0;
            }
            else if (inRes && name.contains("Width", Qt::CaseInsensitive))
            {
                w = xr.readElementText().toInt();
            }
            else if (inRes && name.contains("Height", Qt::CaseInsensitive))
            {
                h = xr.readElementText().toInt();
            }
        }
        else if (xr.isEndElement())
        {
            const QString name = xr.name().toString();
            if (inRes && name.contains("Resolution", Qt::CaseInsensitive))
            {
                inRes = false;
                cur.resolution = QSize(w, h);
            }
            else if (inEnc && name.contains("VideoEncoderConfiguration", Qt::CaseInsensitive))
            {
                inEnc = false;
            }
            else if (inProf && name.endsWith("Profiles", Qt::CaseInsensitive))
            {
                inProf = false;
                if (!cur.token.isEmpty())
                    out.push_back(cur);
                cur = OnvifProfile{};
            }
        }
    }
}
