#pragma once
#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QVideoWidget>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QImage>
#include <QVideoFrame>

class VideoTile : public QWidget
{
    Q_OBJECT
public:
    explicit VideoTile(bool limitFps15 = true, QWidget *parent = nullptr);

    void setName(const QString &name);
    void playUrl(const QUrl &url);
    void stop();

signals:
    void fullscreenRequested();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private slots:
    void onFrame(const QVideoFrame &frame);

private:
    void flushFrame();
    void paintImage(const QImage &img);
    void setConnected(bool ok);

    QMediaPlayer *m_player{};
    QAudioOutput *m_audio{};
    bool m_limitFps{true};
    QVideoWidget *m_video{};
    QLabel *m_canvas{};
    QVideoSink *m_sink{};
    QTimer m_fpsTimer;
    QImage m_lastImage;
    QLabel *m_dot{};
    QLabel *m_label{};
    QLabel *m_err{};
    QPushButton *btnFS{};
    bool m_connected{false};
};
