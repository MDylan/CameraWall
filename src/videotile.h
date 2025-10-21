#pragma once
#include <QWidget>
#include <QImage>
#include <QUrl>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include "language.h"

// előre deklaráció, hogy a headerben ne kelljen QVideoFrame-et includolni
class QVideoFrame;

class VideoTile : public QWidget
{
    Q_OBJECT
public:
    enum AspectMode
    {
        Fit = 0,
        Stretch = 1,
        Fill = 2
    };
    Q_ENUM(AspectMode)

    explicit VideoTile(bool limitFps15, QWidget *parent = nullptr);

    void setName(const QString &n);
    void playUrl(const QUrl &url);
    void stop();

    void setAspectMode(AspectMode m);
    AspectMode aspectMode() const { return m_aspectMode; }

    // VISSZAFELÉ KOMPATIBILITÁS (ha bárhol még hívod):
    void setAspectFill(bool on) { setAspectMode(on ? Fill : Fit); }

signals:
    void fullscreenRequested(); // gomb vagy dupla katt

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private slots:
    void onVideoFrameChanged(const QVideoFrame &frame);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus st);
    void onErrorOccurred(QMediaPlayer::Error err, const QString &msg);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState st);
    void onZoomClicked();
    void retryOnce();
    void updateTranslations();

private:
    void rebuildUi();         // overlay gombok, név, státusz
    void updateHudGeometry(); // overlay elemek pozícionálása
    void setStatusConnecting();
    void setStatusOk();
    void setStatusError();
    void restartStream();
    void scheduleRetry();
    void recreatePipeline();

private:
    // lejátszás
    QMediaPlayer *m_player{};
    QVideoSink *m_sink{};
    AspectMode m_aspectMode = Fit; // alapértelmezett
    AspectMode m_aspectModeRtsp = Fit; // alapértelmezett
    // megjelenítés
    QImage m_frame; // utolsó kép
    bool m_hasFrame{false};
    bool m_aspectFill{true}; // true: „cover”, false: „contain”

    // overlay (név, státusz, nagyítás gomb)
    QWidget *m_overlay{};
    QLabel *m_nameLbl{};
    QLabel *m_statusDot{};
    QPushButton *m_zoomBtn{};

    // egyebek
    QString m_name;
    bool m_limitFps15{true};

    // reconnect/állapot
    QUrl m_url;
    bool m_wantPlay{false};
    QTimer m_retryTimer; // 5 mp-enként újrapróbál, ha hiba volt
    int m_retryCount{0}; // egymás utáni kudarcok száma
    int m_recreateEvery{3}; // ennyi kudarc után teljes újraépítés
    QTimer m_teardownDelay; // <-- ÚJ: rövid szünet stop után
};
