#include "videotile.h"

#include <QtMultimedia/QVideoFrame> // FONTOS: QVideoFrame teljes definíció
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

VideoTile::VideoTile(bool limitFps15, QWidget *parent)
    : QWidget(parent),
      m_limitFps(limitFps15)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    buildUi();

    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);

    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoTile::onFrame);
    connect(m_btnFullscreen, &QToolButton::clicked, this, &VideoTile::fullscreenRequested);
}

VideoTile::~VideoTile()
{
    stop();
}

void VideoTile::buildUi()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // preview (kép helye)
    m_preview = new QLabel(this);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_preview->setMinimumSize(20, 20);
    m_preview->setStyleSheet("background-color:#000;");
    m_rootLayout->addWidget(m_preview, /*stretch*/ 1);

    // overlay sáv (név + státusz + nagyítás gomb)
    QWidget *bar = new QWidget(this);
    bar->setAutoFillBackground(true);
    bar->setStyleSheet("background:rgba(0,0,0,0.35);");
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(6, 3, 6, 3);
    h->setSpacing(6);

    m_statusDot = new QLabel(bar);
    m_statusDot->setFixedSize(10, 10);
    m_statusDot->setStyleSheet("background:#2ecc71; border-radius:5px;"); // default: zöld
    h->addWidget(m_statusDot);

    m_nameLabel = new QLabel(bar);
    m_nameLabel->setStyleSheet("color:white; font-weight:600;");
    m_nameLabel->setText("Camera");
    h->addWidget(m_nameLabel, 1);

    m_btnFullscreen = new QToolButton(bar);
    m_btnFullscreen->setText("⛶");
    m_btnFullscreen->setToolTip(tr("Nagyítás/kicsinyítés"));
    m_btnFullscreen->setStyleSheet("color:white;");
    h->addWidget(m_btnFullscreen, 0, Qt::AlignRight);

    m_rootLayout->addWidget(bar, /*stretch*/ 0);
}

void VideoTile::setName(const QString &name)
{
    m_name = name;
    if (m_nameLabel)
        m_nameLabel->setText(name);
}

void VideoTile::playUrl(const QUrl &url)
{
    if (!m_player)
        return;
    m_player->stop();
    m_player->setSource(url);
    m_player->play();

    if (m_statusDot)
        m_statusDot->setStyleSheet("background:#2ecc71; border-radius:5px;");
}

void VideoTile::stop()
{
    if (!m_player)
        return;
    m_player->stop();
    if (m_statusDot)
        m_statusDot->setStyleSheet("background:#e74c3c; border-radius:5px;");
}

void VideoTile::setFpsLimited(bool on)
{
    m_limitFps = on;
    // itt most nincs valódi FPS throttle – a meglévő logikáddal összekötheted, ha van
}

void VideoTile::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        emit fullscreenRequested();
    QWidget::mouseDoubleClickEvent(e);
}

void VideoTile::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (!m_lastImage.isNull())
        updatePreviewPixmap(m_lastImage);
}

void VideoTile::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);
    // a tényleges képet a QLabel pixmap kezeli, itt nincs rajzolás
}

void VideoTile::onFrame(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;

    const QImage img = frame.toImage();
    if (img.isNull())
        return;

    m_lastImage = img;
    updatePreviewPixmap(img);
}

void VideoTile::updatePreviewPixmap(const QImage &img)
{
    if (!m_preview)
        return;
    const QSize targetSize = m_preview->size();
    if (!targetSize.isValid())
        return;
    const QImage scaled = img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_preview->setPixmap(QPixmap::fromImage(scaled));
}
