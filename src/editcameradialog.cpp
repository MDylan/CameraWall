#include "editcameradialog.h"
#include "onvifclient.h"
#include "util.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>

EditCameraDialog::EditCameraDialog(const Camera *existing, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(existing ? Language::instance().t("editcamera.edit", "Edit Camera") : Language::instance().t("editcamera.add", "Add new Camera"));
    tabs = new QTabWidget(this);

    // RTSP tab
    QWidget *rtspTab = new QWidget;
    auto *rtForm = new QFormLayout(rtspTab);
    nameRtsp = new QLineEdit;
    rtForm->addRow(Language::instance().t("editcamera.name", "Name:"), nameRtsp);
    urlRtsp = new QLineEdit;
    urlRtsp->setPlaceholderText("rtsp://user:pass@host:554/stream");
    rtForm->addRow("RTSP URL:", urlRtsp);

    // ONVIF tab
    QWidget *onvifTab = new QWidget;
    auto *ovForm = new QFormLayout(onvifTab);
    nameOnvif = new QLineEdit;
    ovForm->addRow(Language::instance().t("editcamera.name", "Name:"), nameOnvif);
    ip = new QLineEdit;
    ip->setPlaceholderText("192.168.1.10");
    ovForm->addRow("IP:", ip);
    port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(80);
    ovForm->addRow("Port:", port);
    user = new QLineEdit;
    ovForm->addRow(Language::instance().t("editcamera.username", "Username:"), user);
    pass = new QLineEdit;
    pass->setEchoMode(QLineEdit::Password);
    ovForm->addRow(Language::instance().t("editcamera.password", "Password:"), pass);
    QPushButton *btnFetch = new QPushButton(Language::instance().t("editcamera.retrieveprofiles", "Get Profiles"));
    ovForm->addRow(btnFetch);

    cbAspect = new QComboBox(this);
    cbAspect->addItem(Language::instance().t("aspect.fit", "Fit"), (int)VideoTile::AspectMode::Fit);
    cbAspect->addItem(Language::instance().t("aspect.stretch", "Stretch"), (int)VideoTile::AspectMode::Stretch);
    cbAspect->addItem(Language::instance().t("aspect.fill", "Fill"), (int)VideoTile::AspectMode::Fill);
    ovForm->addRow(Language::instance().t("label.aspect", "Aspect ratio"), cbAspect);

    cbAspectRtsp = new QComboBox(this);
    cbAspectRtsp->addItem(Language::instance().t("aspect.fit", "Fit"), (int)VideoTile::AspectMode::Fit);
    cbAspectRtsp->addItem(Language::instance().t("aspect.stretch", "Stretch"), (int)VideoTile::AspectMode::Stretch);
    cbAspectRtsp->addItem(Language::instance().t("aspect.fill", "Fill"), (int)VideoTile::AspectMode::Fill);
    rtForm->addRow(Language::instance().t("label.aspect", "Aspect ratio"), cbAspectRtsp);

     profileCombo = new QComboBox;
     ovForm->addRow(Language::instance().t("editcamera.profileuse", "Profile to use:"), profileCombo);
     info = new QLabel;
     info->setStyleSheet("color:#9fb2c8");
     ovForm->addRow(info);

     tabs->addTab(rtspTab, Language::instance().t("editcamera.rtsp_manual", "RTSP (manual)"));
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
        cbAspectRtsp->setCurrentIndex(c.aspectModeRtsp);
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
        
        // előre beállított token megjelölése (ha volt)
        preselectedToken = c.onvifChosenToken;
        fetchProfiles();
        cbAspect->setCurrentIndex(c.aspectMode);
    }
}

Camera EditCameraDialog::cameraResult() const
{
    Camera c;
    if (tabs->currentIndex() == 0)
    {
        c.mode = Camera::RTSP;
        c.name = nameRtsp->text().trimmed();
        c.rtspManual = Util::urlFromEncoded(urlRtsp->text().trimmed());
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
        // kiválasztott profil token
        if (profileCombo->currentIndex() >= 0 && profileCombo->currentIndex() < fetchedProfiles.size())
            c.onvifChosenToken = fetchedProfiles[profileCombo->currentIndex()].token;
        if (c.name.isEmpty())
            c.name = device.host();
        c.rtspUriCached.clear(); // újra kérjük majd szükség esetén
        c.aspectMode = (VideoTile::AspectMode)cbAspect->currentData().toInt();
    }
    
    return c;
}

void EditCameraDialog::fetchProfiles()
{
    info->setText(Language::instance().t("editcamera.connecting", "Connecting…"));
    info->repaint();
    QCoreApplication::processEvents();
    QUrl device(QString("http://%1:%2/onvif/device_service").arg(ip->text().trimmed()).arg(port->value()));
    OnvifClient cli;
    QString err;
    QUrl media;
    if (!cli.getCapabilities(device, user->text(), pass->text(), media, &err))
    {
        info->setText(Language::instance().t("editcamera.error_getcapabilities", "GetCapabilities error:") + err);
        return;
    }
    lastMediaXAddr = media.toString();
    QList<OnvifProfile> profs;
    if (!cli.getProfiles(media, user->text(), pass->text(), profs, &err))
    {
        info->setText(Language::instance().t("editcamera.error_getprofiles", "GetProfiles error:") + err);
        return;
    }
    fetchedProfiles = profs;

    auto fmtProfile = [](const OnvifProfile &p)
    {
        const QString enc = p.encoding.isEmpty() ? "?" : p.encoding;
        const QString res = p.resolution.isValid()
                                ? QString("%1x%2").arg(p.resolution.width()).arg(p.resolution.height())
                                : "?";
        return QString("%1 (%2 %3)").arg(p.name.isEmpty() ? p.token : p.name, enc, res);
    };

    profileCombo->clear();
    for (const auto &p : profs)
        profileCombo->addItem(fmtProfile(p));

    // ha volt előre beállított token, próbáljuk kiválasztani
    if (!preselectedToken.isEmpty())
    {
        for (int i = 0; i < profs.size(); ++i)
        {
            if (profs[i].token == preselectedToken)
            {
                profileCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    info->setText(Language::instance().t("editcamera.profiles_loaded", "Profiles loaded."));
}
void EditCameraDialog::setAspectModeInt(int mode)
{
    if (!cbAspect)
        return;
    int idx = cbAspect->findData(mode);
    if (idx < 0)
        idx = 0;
    cbAspect->setCurrentIndex(idx);
    cbAspectRtsp->setCurrentIndex(idx);
}

int EditCameraDialog::aspectModeInt() const
{
    if (!cbAspect)
        return 0;
    return cbAspect->currentData().toInt();
}
int EditCameraDialog::aspectModeRtspInt() const
{
    if (!cbAspectRtsp)
        return 0;
    return cbAspectRtsp->currentData().toInt();
}
