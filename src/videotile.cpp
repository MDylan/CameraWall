#include "videotile.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QtMultimedia/QVideoFrame>

VideoTile::VideoTile(bool limitFps15, QWidget *parent)
    : QWidget(parent),
      m_player(new QMediaPlayer(this)),
      m_audio(new QAudioOutput(this)),
      m_limitFps(limitFps15)
{
    auto *layout = new QVBoxLayout(this);
    m_layout = layout; // ÚJ
    layout->setContentsMargins(0, 0, 0, 0);

    if (m_limitFps)
    {
        m_canvas = new QLabel(this);
        m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_canvas->setAlignment(Qt::AlignCenter);
        m_canvas->setStyleSheet("background:#000");
        layout->addWidget(m_canvas);

        m_sink = new QVideoSink(this);
        connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoTile::onFrame);

        m_fpsTimer.setInterval(66);
        m_fpsTimer.setTimerType(Qt::PreciseTimer);
        connect(&m_fpsTimer, &QTimer::timeout, this, &VideoTile::flushFrame);
        m_fpsTimer.start();
    }
    else
    {
        m_video = new QVideoWidget(this);
        m_video->setAspectRatioMode(Qt::IgnoreAspectRatio);
        m_video->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(m_video);
    }

    m_player->setAudioOutput(m_audio);
    m_audio->setMuted(true);

    // státusz pötty + címke
    m_dot = new QLabel(this);
    m_dot->setFixedSize(10, 10);
    m_dot->setStyleSheet("background:#ff6b6b; border-radius:5px; border:1px solid rgba(255,255,255,0.25);");
    m_dot->raise();

    m_label = new QLabel(this);
    m_label->setStyleSheet("background:rgba(0,0,0,0.5); color:#e8eef7; padding:2px 8px; border-radius:8px;");
    m_label->raise();

    // teljes képernyő gomb
    btnFS = new QPushButton(QString::fromUtf8("⛶"), this);
    btnFS->setToolTip("Teljes képernyő");
    btnFS->setCursor(Qt::PointingHandCursor);
    btnFS->setFixedSize(28, 28);
    btnFS->setStyleSheet(
        "QPushButton{background:rgba(0,0,0,0.45); color:#e8eef7; border:1px solid rgba(255,255,255,0.25); border-radius:6px;}"
        "QPushButton:hover{background:rgba(0,0,0,0.6);}");
    btnFS->raise();
    connect(btnFS, &QPushButton::clicked, this, [this]
            { emit fullscreenRequested(); });

    // hiba label
    m_err = new QLabel(this);
    m_err->setStyleSheet("background:rgba(0,0,0,0.6); color:#ffb3b3; padding:6px 10px; border-radius:8px;");
    m_err->setWordWrap(true);
    m_err->setVisible(false);
    m_err->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_err->setMinimumHeight(24);
    m_err->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_err->adjustSize();
    m_err->move(10, height() - m_err->height() - 10);

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &s)
            {
                m_err->setText("Hiba: " + s);
                m_err->setVisible(true);
                setConnected(false);
            });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState st)
            {
                if (st == QMediaPlayer::PlayingState)
                {
                    m_err->setVisible(false);
                    setConnected(true);
                }
            });

    // alapértelmezett kimenet: saját lejátszó a saját kimenetre
    if (m_limitFps)
        m_player->setVideoSink(m_sink);
    else
        m_player->setVideoOutput(m_video);
}

void VideoTile::setName(const QString &name)
{
    m_label->setText(name);
    m_label->adjustSize();
}

void VideoTile::playUrl(const QUrl &url)
{
    setConnected(false);
    if (m_limitFps)
        m_player->setVideoSink(m_sink);
    else
        m_player->setVideoOutput(m_video);
    m_player->setSource(url);
    m_player->setLoops(QMediaPlayer::Infinite);
    m_player->play();
}

void VideoTile::stop()
{
    m_player->stop();
}

void VideoTile::applyToPlayer(QMediaPlayer *player)
{
    if (!player)
        return;
    // az audio-t is rákötjük, némítva
    player->setAudioOutput(m_audio);
    m_audio->setMuted(true);

    if (m_limitFps)
        player->setVideoSink(m_sink);
    else
        player->setVideoOutput(m_video);
}

void VideoTile::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    const int pad = 10;
    m_dot->move(pad, pad);
    m_label->move(pad + m_dot->width() + 6, pad - 4);
    m_err->move(10, height() - m_err->height() - 10);
    if (btnFS)
        btnFS->move(width() - btnFS->width() - 10, height() - btnFS->height() - 10);
    if (m_limitFps && !m_lastImage.isNull() && m_canvas)
        paintImage(m_lastImage);
}

void VideoTile::mouseDoubleClickEvent(QMouseEvent *e)
{
    Q_UNUSED(e)
    emit fullscreenRequested();
}

void VideoTile::onFrame(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;
    const QImage img = frame.toImage();
    if (img.isNull())
    {
        if (!m_widgetFallback)
        {
            switchToWidgetFallback();
            // a lejátszás már megy tovább ugyanazzal a playerrel, mostantól közvetlenül rajzol
        }
        return;
    }
    m_lastImage = img;
    setConnected(true);
    if (!m_fpsTimer.isActive())
        flushFrame();
}

void VideoTile::flushFrame()
{
    if (m_lastImage.isNull() || !m_canvas)
        return;
    paintImage(m_lastImage);
}

void VideoTile::paintImage(const QImage &img)
{
    const QSize target = m_canvas->size().expandedTo(QSize(1, 1));
    QPixmap pm = QPixmap::fromImage(img).scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_canvas->setPixmap(std::move(pm));
}

void VideoTile::setConnected(bool ok)
{
    m_connected = ok;
    const char *col = ok ? "#43d18b" : "#ff6b6b";
    m_dot->setStyleSheet(QString("background:%1; border-radius:5px; border:1px solid rgba(255,255,255,0.25);").arg(col));
}

void VideoTile::switchToWidgetFallback()
{
    m_widgetFallback = true;

    // rejtsük el a vásznat (ha volt), és álljunk át közvetlen videóra
    if (!m_video)
    {
        m_video = new QVideoWidget(this);
        m_video->setAspectRatioMode(Qt::IgnoreAspectRatio);
        m_video->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        if (m_layout)
            m_layout->addWidget(m_video);
    }
    if (m_canvas)
        m_canvas->hide();

    // váltsunk sink → video output
    m_player->setVideoSink(nullptr);
    m_player->setVideoOutput(m_video);
}
