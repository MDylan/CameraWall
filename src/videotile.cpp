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
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoTile::onFrame);

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
    m_statusDot->setStyleSheet("background:#4caf50; border-radius:5px;");

    m_zoomBtn = new QPushButton(u8"⛶", this);
    m_zoomBtn->setCursor(Qt::PointingHandCursor);
    m_zoomBtn->setFocusPolicy(Qt::NoFocus);
    m_zoomBtn->setStyleSheet(
        "QPushButton{color:white; background-color:rgba(0,0,0,110); border:none; padding:4px 8px;}"
        "QPushButton:hover{background-color:rgba(0,0,0,170);}");
    updateTranslations();
    connect(m_zoomBtn, &QPushButton::clicked, this, &VideoTile::onZoomClicked);
    connect(&Language::instance(), &Language::languageChanged,
            this, &VideoTile::updateTranslations);

    updateHudGeometry();
}

void VideoTile::updateTranslations()
{
    if (m_zoomBtn)
    {
        m_zoomBtn->setToolTip(
            Language::instance().t("menu.zoom", "Zoom KI/BE"));
    }
    // Ha van más szöveges elem a csempén (pl. státusz tooltip),
    // azokat is itt érdemes frissíteni.
}

void VideoTile::updateHudGeometry()
{
    if (!m_overlay)
        return;
    m_overlay->setGeometry(rect());

    // Egyszerű elrendezés: bal felsőn név + státusz, jobb felsőn zoom
    const int pad = 8;

    // név és dot egy sávban
    QSize nameSz = m_nameLbl->sizeHint();
    const int dot = m_statusDot->height();
    m_statusDot->move(pad, pad + (nameSz.height() - dot) / 2);
    m_nameLbl->move(pad + dot + 6, pad);
    m_nameLbl->resize(nameSz);

    // zoom gomb jobb felső sarok
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
    m_hasFrame = false;
    m_frame = QImage();
    update();

    m_player->stop();
    m_player->setSource(url);
    if (m_limitFps15)
        m_player->setPlaybackRate(1.0); // (ha volt fps limit logikád, itt lehetne finomítani)
    m_player->play();
}

void VideoTile::stop()
{
    if (m_player)
        m_player->stop();
    m_hasFrame = false;
    m_frame = QImage();
    update();
}

void VideoTile::onFrame(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;

    QVideoFrame f(frame);
    if (!f.map(QVideoFrame::ReadOnly))
        return;

    QImage img = f.toImage(); // Qt 6: közvetlenül kérhetjük
    f.unmap();

    if (!img.isNull())
    {
        m_frame = img.convertToFormat(QImage::Format_RGB32);
        m_hasFrame = true;
        update(); // újrarajzolás
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
        // egyszerű „no signal” jelzés
        p.setPen(QColor("#5e6a7a"));
        p.drawText(rect(), Qt::AlignCenter, tr("Nincs kép…"));
        return;
    }

    // „contain” = teljes kép látszik, fekete sávok lehetnek
    // „cover”   = a csempe teljesen kitöltve, a kép középről vágva
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
        // számoljuk ki a forrás kivágást: a csempe arányához illesztett középső téglalap
        const double sw = m_frame.width();
        const double sh = m_frame.height();
        const double dw = target.width();
        const double dh = target.height();

        const double sAspect = sw / sh;
        const double dAspect = dw / dh;

        QRectF src;
        if (sAspect > dAspect)
        {
            // forrás túl széles -> vágjunk a szélekből
            const double newW = sh * dAspect;
            const double x = (sw - newW) / 2.0;
            src = QRectF(x, 0, newW, sh);
        }
        else
        {
            // forrás túl magas -> vágjunk felül/alul
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
    update(); // újraszámoljuk a skálázást
}

void VideoTile::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton)
        emit fullscreenRequested();
    QWidget::mouseDoubleClickEvent(ev);
}

void VideoTile::onZoomClicked()
{
    qDebug() << "Zoomclicked";
    emit fullscreenRequested();
}
