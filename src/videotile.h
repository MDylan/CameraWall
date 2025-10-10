#pragma once
#include <QWidget>
#include <QImage>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>
#include <QLabel>
#include <QPushButton>

class VideoTile : public QWidget
{
    Q_OBJECT
public:
    explicit VideoTile(bool limitFps15, QWidget *parent = nullptr);

    void setName(const QString &n);
    void playUrl(const QUrl &url);
    void stop();

    // Ha valaha „contain” módot szeretnél, ezzel lehet váltani.
    void setAspectFill(bool fill)
    {
        m_aspectFill = fill;
        update();
    }

signals:
    void fullscreenRequested(); // gomb vagy dupla katt

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private slots:
    void onFrame(const QVideoFrame &frame);
    void onZoomClicked();

private:
    void rebuildUi();         // overlay gombok, név, státusz
    void updateHudGeometry(); // overlay elemek pozícionálása

private:
    // lejátszás
    QMediaPlayer *m_player{};
    QVideoSink *m_sink{};

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
};
