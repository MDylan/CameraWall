#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QImage>
#include <QPushButton>
#include <QtMultimedia/QVideoSink>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include <QVBoxLayout>

class VideoTile : public QWidget
{
    Q_OBJECT
public:
    explicit VideoTile(bool limitFps15 = true, QWidget *parent = nullptr);

    void setName(const QString &name);
    // Régi API-k kompatibilitás miatt (nem használjuk, ha háttérlejátszót kötünk rá):
    void playUrl(const QUrl &url);
    void stop();

    // ÚJ: egy külső QMediaPlayer képét erre a csempére irányítja
    void applyToPlayer(QMediaPlayer *player);

signals:
    void fullscreenRequested();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void onFrame(const QVideoFrame &frame);
    void flushFrame();
    void paintImage(const QImage &img);
    void setConnected(bool ok);
    void switchToWidgetFallback(); // ÚJ
    QVBoxLayout *m_layout{};       // ÚJ: a root layout eltárolása
    bool m_widgetFallback{false};  // ÚJ: egyszer már átálltunk-e widgetre

private:
    QMediaPlayer *m_player{};
    QAudioOutput *m_audio{};
    bool m_limitFps{true};

    // Kimenetek
    QVideoWidget *m_video{}; // ha !m_limitFps
    QVideoSink *m_sink{};    // ha m_limitFps
    QLabel *m_canvas{};      // saját festés (m_sink esetén)

    QTimer m_fpsTimer;
    QImage m_lastImage;

    // UI rétegek
    QLabel *m_dot{};
    QLabel *m_label{};
    QLabel *m_err{};
    QPushButton *btnFS{};

    bool m_connected{false};
};
