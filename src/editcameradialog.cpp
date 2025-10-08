#include "editcameradialog.h"
#include <QVBoxLayout>

using namespace Util;

EditCameraDialog::EditCameraDialog(const Camera *existing, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(existing ? "Kamera szerkesztése" : "Kamera hozzáadása");
    tabs = new QTabWidget(this);

    // RTSP tab
    QWidget *rtspTab = new QWidget;
    auto *rtForm = new QFormLayout(rtspTab);
    nameRtsp = new QLineEdit;
    rtForm->addRow("Név:", nameRtsp);
    urlRtsp = new QLineEdit;
    urlRtsp->setPlaceholderText("rtsp://user:pass@host:554/stream");
    rtForm->addRow("RTSP URL:", urlRtsp);

    // ONVIF tab
    QWidget *onvifTab = new QWidget;
    auto *ovForm = new QFormLayout(onvifTab);
    nameOnvif = new QLineEdit;
    ovForm->addRow("Név:", nameOnvif);
    ip = new QLineEdit;
    ip->setPlaceholderText("192.168.1.10");
    ovForm->addRow("IP:", ip);
    port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(80);
    ovForm->addRow("Port:", port);
    user = new QLineEdit;
    ovForm->addRow("Felhasználó:", user);
    pass = new QLineEdit;
    pass->setEchoMode(QLineEdit::Password);
    ovForm->addRow("Jelszó:", pass);
    QPushButton *btnFetch = new QPushButton("Profilok lekérése");
    ovForm->addRow(btnFetch);
    lowCombo = new QComboBox;
    highCombo = new QComboBox;
    ovForm->addRow("Rács profil:", lowCombo);
    ovForm->addRow("Teljes képernyő profil:", highCombo);
    info = new QLabel;
    info->setStyleSheet("color:#9fb2c8");
    ovForm->addRow(info);

    tabs->addTab(rtspTab, "RTSP (kézi)");
    tabs->addTab(onvifTab, "ONVIF");

    auto *mainLay = new QVBoxLayout(this);
    mainLay->addWidget(tabs);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLay->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(btnFetch, &QPushButton::clicked, this, [this]
            { fetchProfiles(); });

    if (existing)
        setFromCamera(*existing);
}

void EditCameraDialog::setFromCamera(const Camera &c)
{
    if (c.mode == Camera::RTSP)
    {
        tabs->setCurrentIndex(0);
        nameRtsp->setText(c.name);
        urlRtsp->setText(QString::fromUtf8(c.rtspManual.toEncoded()));
    }
    else
    {
        tabs->setCurrentIndex(1);
        nameOnvif->setText(c.name);
        QUrl x = c.onvifDeviceXAddr;
        ip->setText(x.host());
        port->setValue(x.port() > 0 ? x.port() : 80);
        user->setText(c.onvifUser);
        pass->setText(c.onvifPass);
        lastMediaXAddr = c.onvifMediaXAddr.toString();
    }
}

Camera EditCameraDialog::cameraResult() const
{
    Camera c;
    if (tabs->currentIndex() == 0)
    {
        c.mode = Camera::RTSP;
        c.name = nameRtsp->text().trimmed();
        c.rtspManual = urlFromEncoded(urlRtsp->text().trimmed());
        if (c.name.isEmpty())
            c.name = c.rtspManual.host();
    }
    else
    {
        c.mode = Camera::ONVIF;
        c.name = nameOnvif->text().trimmed();
        QUrl device(QString("http://%1:%2/onvif/device_service").arg(ip->text().trimmed()).arg(port->value()));
        c.onvifDeviceXAddr = device;
        c.onvifUser = user->text();
        c.onvifPass = pass->text();
        if (!lastMediaXAddr.isEmpty())
            c.onvifMediaXAddr = QUrl(lastMediaXAddr);
        if (lowCombo->currentIndex() >= 0 && lowCombo->currentIndex() < fetchedProfiles.size())
            c.onvifLowToken = fetchedProfiles[lowCombo->currentIndex()].token;
        if (highCombo->currentIndex() >= 0 && highCombo->currentIndex() < fetchedProfiles.size())
            c.onvifHighToken = fetchedProfiles[highCombo->currentIndex()].token;
        if (c.name.isEmpty())
            c.name = device.host();
        c.rtspUriLowCached = cachedLowUri;
        c.rtspUriHighCached = cachedHighUri;
    }
    return c;
}

void EditCameraDialog::fetchProfiles()
{
    info->setText("Kapcsolódás…");
    info->repaint();
    QCoreApplication::processEvents();

    QUrl device(QString("http://%1:%2/onvif/device_service").arg(ip->text().trimmed()).arg(port->value()));
    OnvifClient cli;
    QString err;
    QUrl media;
    if (!cli.getCapabilities(device, user->text(), pass->text(), media, &err))
    {
        info->setText("GetCapabilities hiba: " + err);
        return;
    }
    lastMediaXAddr = media.toString();
    QList<OnvifProfile> profs;
    if (!cli.getProfiles(media, user->text(), pass->text(), profs, &err))
    {
        info->setText("GetProfiles hiba: " + err);
        return;
    }
    fetchedProfiles = profs;

    auto fmtProfile = [](const OnvifProfile &p)
    {
        const QString enc = p.encoding.isEmpty() ? "?" : p.encoding;
        const QString res = p.resolution.isValid() ? QString("%1x%2").arg(p.resolution.width()).arg(p.resolution.height()) : "?";
        return QString("%1 (%2 %3)").arg(p.name.isEmpty() ? p.token : p.name, enc, res);
    };

    // low: kis felbontás előre
    QList<OnvifProfile> low = profs;
    std::sort(low.begin(), low.end(), [](const OnvifProfile &a, const OnvifProfile &b)
              { return a.resolution.width() * a.resolution.height() < b.resolution.width() * b.resolution.height(); });
    // high: nagy felbontás előre
    QList<OnvifProfile> high = profs;
    std::sort(high.begin(), high.end(), [](const OnvifProfile &a, const OnvifProfile &b)
              { return a.resolution.width() * a.resolution.height() > b.resolution.width() * b.resolution.height(); });

    lowCombo->clear();
    for (const auto &p : low)
        lowCombo->addItem(fmtProfile(p));
    highCombo->clear();
    for (const auto &p : high)
        highCombo->addItem(fmtProfile(p));
    if (!low.isEmpty())
        lowCombo->setCurrentIndex(0);
    if (!high.isEmpty())
        highCombo->setCurrentIndex(0);

    // Best-effort cache
    cachedLowUri.clear();
    cachedHighUri.clear();
    if (!low.isEmpty())
    {
        QString uri;
        if (cli.getStreamUri(media, user->text(), pass->text(), low.first().token, uri, &err))
            cachedLowUri = uri;
    }
    if (!high.isEmpty())
    {
        QString uri;
        if (cli.getStreamUri(media, user->text(), pass->text(), high.first().token, uri, &err))
            cachedHighUri = uri;
    }

    info->setText("Profilok betöltve. (Mentéskor cache-elés)");
}
