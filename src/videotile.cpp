#include "videotile.h"
#include "language.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QMouseEvent>
#include <QtMultimedia/QVideoFrame>

VideoTile::VideoTile(bool limitFps15, QWidget *parent)
    : QWidget(parent), m_limitFps15(limitFps15)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Player + sink
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);

    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoTile::onVideoFrameChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &VideoTile::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &VideoTile::onErrorOccurred);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &VideoTile::onPlaybackStateChanged);

    // retry timer (alapból leállítva)
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &VideoTile::retryOnce);

    // felület
    rebuildUi();
}

void VideoTile::rebuildUi()
{
    // fő layout (0 margó, a képet mi festjük a paintEvent-ben)
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // overlay konténer (HUD)
    m_overlay = new QWidget(this);
    m_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_overlay->setStyleSheet("background:transparent;");
    m_overlay->raise();

    // HUD tartalom
    m_nameLbl = new QLabel(m_overlay);
    m_nameLbl->setStyleSheet("color:white; font-weight:600; background-color:rgba(0,0,0,110); padding:2px 6px;");

    m_statusDot = new QLabel(m_overlay);
    m_statusDot->setFixedSize(10, 10);
    setStatusError(); // kezdetben „nincs stream”

    m_zoomBtn = new QPushButton(u8"⛶", this);
    m_zoomBtn->setCursor(Qt::PointingHandCursor);
    m_zoomBtn->setFocusPolicy(Qt::NoFocus);
    m_zoomBtn->setStyleSheet(
        "QPushButton{color:white; background-color:rgba(0,0,0,110); border:none; padding:4px 8px;}"
        "QPushButton:hover{background-color:rgba(0,0,0,170);}");
    connect(m_zoomBtn, &QPushButton::clicked, this, &VideoTile::onZoomClicked);

    updateTranslations();
    connect(&Language::instance(), &Language::languageChanged,
            this, &VideoTile::updateTranslations);

    updateHudGeometry();

    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &VideoTile::retryOnce);

    m_teardownDelay.setSingleShot(true);
    connect(&m_teardownDelay, &QTimer::timeout, this, [this]
            {
        // csak itt állítjuk be újra a forrást, a stop() utáni rövid pihenő után
        if (!m_url.isValid() || !m_wantPlay) return;
        qDebug() << "[VideoTile] teardown gap done -> setSource+play" << m_url;
        m_player->setSource(m_url);
        m_player->play(); });
}

void VideoTile::updateTranslations()
{
    if (m_zoomBtn)
        m_zoomBtn->setToolTip(Language::instance().t("menu.zoom", "Teljes nézet"));
}

void VideoTile::updateHudGeometry()
{
    if (!m_overlay)
        return;
    m_overlay->setGeometry(rect());

    const int pad = 8;

    QSize nameSz = m_nameLbl->sizeHint();
    const int dot = m_statusDot->height();
    m_statusDot->move(pad, pad + (nameSz.height() - dot) / 2);
    m_nameLbl->move(pad + dot + 6, pad);
    m_nameLbl->resize(nameSz);

    QSize z = m_zoomBtn->sizeHint();
    m_zoomBtn->move(width() - z.width() - pad, pad);
    m_zoomBtn->resize(z);
}

void VideoTile::setName(const QString &n)
{
    m_name = n;
    if (m_nameLbl)
    {
        m_nameLbl->setText(m_name);
        m_nameLbl->adjustSize();
        updateHudGeometry();
    }
}

void VideoTile::playUrl(const QUrl &url)
{
    m_url = url;
    m_wantPlay = true;

    m_hasFrame = false; // ne őrizze meg az utolsó képet
    m_frame = QImage();
    setStatusConnecting(); // tényleg most kezd próbálkozni
    update();

    m_retryCount = 0; // új URL: kudarcszámláló nullázása
    restartStream();
}

void VideoTile::restartStream()
{
    if (!m_url.isValid())
        return;

    qDebug() << "[VideoTile] restartStream() -> stop, clear, delay, then play" << m_url;

    m_retryTimer.stop(); // ne fusson párhuzamosan
    m_player->stop();

    // teljes forrás-ürítés, hogy az FFmpeg lezárhassa a régi RTSP-t
    m_player->setSource(QUrl());

    // UI: nincs kép a próbálkozás alatt
    m_hasFrame = false;
    m_frame = QImage();
    setStatusConnecting();
    update();

    // rövid szünet a teardown-nak, utána setSource()+play
    m_teardownDelay.start(400); // 400 ms elég szokott lenni
}

void VideoTile::scheduleRetry()
{
    if (!m_wantPlay || !m_url.isValid())
        return;

    if (m_retryTimer.isActive())
    {
        qDebug() << "[VideoTile] scheduleRetry: already active";
        return;
    }

    setStatusError();         // hiba állapot a várakozás alatt
    m_retryTimer.start(5000); // 5 mp múlva retry
}

void VideoTile::retryOnce()
{
    qDebug() << "[VideoTile] retryOnce() shouldPlay=" << m_wantPlay
             << " urlValid=" << m_url.isValid()
             << " retryCount=" << m_retryCount;

    if (!m_wantPlay || !m_url.isValid())
        return;

    // bizonyos számú kudarc után teljes pipeline újraépítése
    if (++m_retryCount % m_recreateEvery == 0)
        recreatePipeline();

    setStatusConnecting(); // most tényleg próbálkozik (sárga)
    restartStream();
}

void VideoTile::stop()
{
    m_wantPlay = false;
    m_retryTimer.stop();

    if (m_player)
        m_player->stop();

    m_hasFrame = false;
    m_frame = QImage();
    setStatusError(); // piros
    update();
}

void VideoTile::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;

    QVideoFrame f(frame);
    if (!f.map(QVideoFrame::ReadOnly))
        return;

    QImage img = f.toImage();
    f.unmap();

    if (!img.isNull())
    {
        m_frame = img.convertToFormat(QImage::Format_RGB32);
        m_hasFrame = true;
        setStatusOk();    // csak tényleges frame-re lesz zöld
        m_retryCount = 0; // siker: nullázás
        update();
    }
}

void VideoTile::onMediaStatusChanged(QMediaPlayer::MediaStatus st)
{
    qDebug() << "[VideoTile] mediaStatusChanged:" << st << " hadFrame=" << m_hasFrame;

    switch (st)
    {
    case QMediaPlayer::LoadingMedia:
    case QMediaPlayer::BufferingMedia:
        if (!m_hasFrame)
            setStatusConnecting();
        break;
    case QMediaPlayer::InvalidMedia:
    case QMediaPlayer::NoMedia:
        setStatusError();
        scheduleRetry();
        break;
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::EndOfMedia:
        setStatusConnecting();
        scheduleRetry();
        break;
    default:
        break; // zöldet csak frame érkezésekor állítunk
    }
}

void VideoTile::onErrorOccurred(QMediaPlayer::Error err, const QString &msg)
{
    qDebug() << "[VideoTile] onErrorOccurred:" << err << msg;
    setStatusError();
    scheduleRetry(); // ha már aktív, nem indít új időzítőt
}

void VideoTile::onPlaybackStateChanged(QMediaPlayer::PlaybackState st)
{
    qDebug() << "[VideoTile] playbackStateChanged:" << st;
    if (st == QMediaPlayer::StoppedState && m_wantPlay)
    {
        // ha akaratunk ellenére leállt, ütemezzük az újrapróbát
        scheduleRetry();
    }
}

void VideoTile::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // háttér (ha nincs frame)
    if (!m_hasFrame || m_frame.isNull())
    {
        p.fillRect(rect(), QColor(10, 12, 20));
        p.setPen(QColor("#5e6a7a"));
        p.drawText(rect(), Qt::AlignCenter, Language::instance().t("status.noimage", "Nincs kép…"));
        return;
    }

    const QRect target = this->rect();
    if (!m_aspectFill)
    {
        // contain
        QImage scaled = m_frame.scaled(target.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPoint topLeft(target.center() - QPoint(scaled.width() / 2, scaled.height() / 2));
        p.fillRect(target, Qt::black);
        p.drawImage(QRect(topLeft, scaled.size()), scaled);
    }
    else
    {
        // cover
        const double sw = m_frame.width();
        const double sh = m_frame.height();
        const double dw = target.width();
        const double dh = target.height();

        const double sAspect = sw / sh;
        const double dAspect = dw / dh;

        QRectF src;
        if (sAspect > dAspect)
        {
            const double newW = sh * dAspect;
            const double x = (sw - newW) / 2.0;
            src = QRectF(x, 0, newW, sh);
        }
        else
        {
            const double newH = sw / dAspect;
            const double y = (sh - newH) / 2.0;
            src = QRectF(0, y, sw, newH);
        }

        p.fillRect(target, Qt::black);
        p.drawImage(target, m_frame, src);
    }
}

void VideoTile::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    updateHudGeometry();
    update();
}

void VideoTile::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton)
        emit fullscreenRequested();
    QWidget::mouseDoubleClickEvent(ev);
}

void VideoTile::onZoomClicked()
{
    emit fullscreenRequested();
}

// --- státusz segédek ---
void VideoTile::setStatusConnecting()
{
    if (m_statusDot)
        m_statusDot->setStyleSheet("background:#ffca28; border-radius:5px;"); // amber/sárga
}
void VideoTile::setStatusOk()
{
    if (m_statusDot)
        m_statusDot->setStyleSheet("background:#4caf50; border-radius:5px;"); // zöld
}
void VideoTile::setStatusError()
{
    if (m_statusDot)
        m_statusDot->setStyleSheet("background:#f44336; border-radius:5px;"); // piros
}
void VideoTile::recreatePipeline()
{
    qDebug() << "[VideoTile] recreatePipeline() – rebuilding player and sink";

    // régi objektumok törlése
    if (m_player)
    {
        m_player->stop();
        m_player->setVideoSink(nullptr);
        m_player->deleteLater();
    }
    if (m_sink)
    {
        m_sink->deleteLater();
    }

    // újak létrehozása
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);

    // jelek visszakötése
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoTile::onVideoFrameChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &VideoTile::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &VideoTile::onErrorOccurred);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &VideoTile::onPlaybackStateChanged);
}
