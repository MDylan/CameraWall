#pragma once

#include <QWidget>
#include <QUrl>
#include <QImage>
#include <QPointer>

#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>

class QLabel;
class QToolButton;
class QVBoxLayout;

class VideoTile : public QWidget
{
    Q_OBJECT
public:
    explicit VideoTile(bool limitFps15, QWidget *parent = nullptr);
    ~VideoTile() override;

    void playUrl(const QUrl &url);
    void stop();

    void setName(const QString &name);
    QString name() const { return m_name; }

    void setFpsLimited(bool on);

signals:
    void fullscreenRequested(); // nagyítás/kicsinyítés kérés (dupla kattintás vagy gomb)

protected:
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private slots:
    void onFrame(const QVideoFrame &frame);

private:
    void buildUi();
    void updateOverlayGeometry();
    void updatePreviewPixmap(const QImage &img);

private:
    // lejátszás
    QMediaPlayer *m_player{nullptr};
    QVideoSink *m_sink{nullptr};

    // UI
    QVBoxLayout *m_rootLayout{nullptr};
    QLabel *m_preview{nullptr}; // ide rajzoljuk a képet (pixmap)
    QLabel *m_nameLabel{nullptr};
    QLabel *m_statusDot{nullptr};
    QToolButton *m_btnFullscreen{nullptr};

    // állapot
    QString m_name;
    bool m_limitFps{true};
    QImage m_lastImage;
};
