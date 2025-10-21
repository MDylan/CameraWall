#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>

#include "util.h"
#include "onvifclient.h"
#include "language.h"
#include "videotile.h"

// A teljes app Camera modellje
struct Camera
{
    enum Mode
    {
        RTSP,
        ONVIF
    } mode = RTSP;
    QString name;

    // RTSP (kézi)
    QUrl rtspManual;

    // ONVIF
    QUrl onvifDeviceXAddr; // http://IP/onvif/device_service
    QUrl onvifMediaXAddr;  // GetCapabilities-ből
    QString onvifUser, onvifPass;

    // Mostantól EGY választott profil tokenjét használjuk mindenhol
    QString onvifChosenToken;

    // Cache-elt (feloldott) RTSP URI (ha már lekértük)
    QString rtspUriCached;

    VideoTile::AspectMode aspectMode = VideoTile::AspectMode::Fit;
    VideoTile::AspectMode aspectModeRtsp = VideoTile::AspectMode::Fit;
};

class EditCameraDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditCameraDialog(const Camera *existing = nullptr, QWidget *parent = nullptr);

    void setFromCamera(const Camera &c);
    Camera cameraResult() const;
    void setAspectModeInt(int mode); // 0/1/2
    int aspectModeInt() const;       // 0/1/2
    int aspectModeRtspInt() const;       // 0/1/2

private:
    void fetchProfiles();

private:
    QTabWidget *tabs{};
    // RTSP tab
    QLineEdit *nameRtsp{}, *urlRtsp{};
    // ONVIF tab
    QLineEdit *nameOnvif{}, *ip{}, *user{}, *pass{};
    QSpinBox *port{};
    QComboBox *profileCombo{}; // egyetlen legördülő: a választott profil
    QComboBox *cbAspect = nullptr;
    QComboBox *cbAspectRtsp = nullptr;
    QLabel *info{};

    QList<OnvifProfile> fetchedProfiles;
    QString lastMediaXAddr;
    QString cachedUri; // best-effort előtöltés
    QString preselectedToken;
};
