#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include "onvifclient.h"
#include "util.h"

struct Camera
{
    enum Mode
    {
        RTSP,
        ONVIF
    } mode = RTSP;
    QString name;

    // RTSP kézi
    QUrl rtspManual;

    // ONVIF mezők
    QUrl onvifDeviceXAddr; // http://IP/onvif/device_service
    QUrl onvifMediaXAddr;  // GetCapabilities-ből
    QString onvifUser, onvifPass;
    QString onvifLowToken, onvifHighToken; // kiválasztott profilok

    // Cache-elt RTSP
    QString rtspUriLowCached;
    QString rtspUriHighCached;
};

class EditCameraDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditCameraDialog(const Camera *existing = nullptr, QWidget *parent = nullptr);

    void setFromCamera(const Camera &c);
    Camera cameraResult() const;

private:
    void fetchProfiles();

    QTabWidget *tabs{};
    // RTSP tab
    QLineEdit *nameRtsp{}, *urlRtsp{};
    // ONVIF tab
    QLineEdit *nameOnvif{}, *ip{}, *user{}, *pass{};
    QSpinBox *port{};
    QComboBox *lowCombo{}, *highCombo{};
    QLabel *info{};
    QList<OnvifProfile> fetchedProfiles;
    QString lastMediaXAddr;
    QString cachedLowUri, cachedHighUri;
};
