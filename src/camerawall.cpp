#include "camerawall.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QMenu>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QContextMenuEvent>

// ======= ctor =======
CameraWall::CameraWall()
{
    setWindowTitle("IP Kamera fal (RTSP/ONVIF)");
    QWidget *central = new QWidget;
    setCentralWidget(central);
    grid = new QGridLayout(central);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);

    // Menük
    auto *mCams = menuBar()->addMenu("&Kamerák");
    mCams->addAction("Hozzáadás…", this, [this]
                     { onAdd(); });
    actEdit = mCams->addAction("Szerkesztés…", this, [this]
                               { onEditSelected(); });
    mCams->addAction("Kijelölt törlése", this, [this]
                     { onRemoveSelected(); });
    mCams->addAction("Összes törlése", this, [this]
                     { onClearAll(); });
    mCams->addSeparator();
    mCams->addAction("Újratöltés", this, [this]
                     { reloadAll(); });

    auto *mView = menuBar()->addMenu("&Nézet");
    actFull = mView->addAction("Teljes képernyő (F11)", this, [this]
                               { toggleFullscreen(); });
    actFull->setShortcut(Qt::Key_F11);
    actFps = mView->addAction("FPS limit 15", this, [this]
                              { toggleFpsLimit(); });
    actFps->setCheckable(true);

    QMenu *mGrid = mView->addMenu("Rács");
    gridGroup = new QActionGroup(mGrid);
    gridGroup->setExclusive(true);
    actGrid2 = mGrid->addAction("2×2");
    actGrid3 = mGrid->addAction("3×3");
    actGrid2->setCheckable(true);
    actGrid3->setCheckable(true);
    gridGroup->addAction(actGrid2);
    gridGroup->addAction(actGrid3);
    connect(actGrid2, &QAction::triggered, this, [this]
            { setGridN(2); });
    connect(actGrid3, &QAction::triggered, this, [this]
            { setGridN(3); });

    // 10s automatikus váltás
    actAutoRotate = mView->addAction("Automatikus váltás 10s", this, [this]
                                     { toggleAutoRotate(); });
    actAutoRotate->setCheckable(true);

    // ÚJ: háttér stream megtartása (gyorsabb váltás)
    actKeepAlive = mView->addAction("Gyors váltás (háttér stream megtartása)", this, [this]
                                    { toggleKeepAlive(); });
    actKeepAlive->setCheckable(true);

    statusBar()->showMessage("F11 – teljes képernyő • Duplakatt/⛶: fókusz • ONVIF: rács=low, fókusz=high");

    connect(&rotateTimer, &QTimer::timeout, this, [this]
            { nextPage(); });
    rotateTimer.setInterval(10000);

    // Beállítások betöltése
    loadFromIni();

    if (gridN != 2 && gridN != 3)
        gridN = 2;
    actGrid2->setChecked(gridN == 2);
    actGrid3->setChecked(gridN == 3);
    actFps->setChecked(m_limitFps15);
    actAutoRotate->setChecked(m_autoRotate);
    actKeepAlive->setChecked(m_keepBackgroundStreams);

    ensureStreamsSize();
    rebuildTiles();

    QTimer::singleShot(0, [this]
                       { this->showFullScreen(); });
}

void CameraWall::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu(this);
    menu.addAction("Hozzáadás…", this, [this]
                   { onAdd(); });
    menu.addAction("Szerkesztés…", this, [this]
                   { onEditSelected(); });
    menu.addAction("Kijelölt törlése", this, [this]
                   { onRemoveSelected(); });
    menu.addSeparator();
    menu.addAction("Teljes képernyő váltása (F11)", this, [this]
                   { toggleFullscreen(); });
    menu.addAction(actFps);
    QMenu *sub = menu.addMenu("Rács");
    sub->addAction(actGrid2);
    sub->addAction(actGrid3);
    menu.addSeparator();
    menu.addAction(actAutoRotate);
    menu.addAction(actKeepAlive);
    menu.exec(e->globalPos());
}

bool CameraWall::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == focusDlg && event->type() == QEvent::KeyPress)
    {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape)
        {
            exitFocus();
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonRelease)
    {
        if (auto *w = qobject_cast<QWidget *>(obj))
        {
            if (auto *vt = qobject_cast<VideoTile *>(w))
            {
                int idx = tileIndexMap.value(vt, -1);
                if (idx >= 0)
                    tileClicked(idx);
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// ======= alap műveletek =======

void CameraWall::setGridN(int n)
{
    if (n != 2 && n != 3)
        return;
    gridN = n;
    saveViewToIni();
    rebuildTiles();
}

void CameraWall::onAdd()
{
    EditCameraDialog dlg(nullptr, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        cams.push_back(dlg.cameraResult());
        ensureStreamsSize();
        saveCamerasToIni();
        rebuildTiles();
    }
}

void CameraWall::onEditSelected()
{
    if (selectedIndex < 0 || selectedIndex >= cams.size())
    {
        QMessageBox::information(this, "Szerkesztés", "Jelölj ki egy kamerát (kattints a csempére).");
        return;
    }
    Camera cur = cams[selectedIndex];
    EditCameraDialog dlg(&cur, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        cams[selectedIndex] = dlg.cameraResult();
        ensureStreamsSize();
        saveCamerasToIni();
        rebuildTiles();
    }
}

void CameraWall::onRemoveSelected()
{
    if (selectedIndex < 0 || selectedIndex >= cams.size())
        return;
    if (QMessageBox::question(this, "Törlés", "Biztosan törlöd a kijelölt kamerát?") == QMessageBox::Yes)
    {
        cams.removeAt(selectedIndex);
        selectedIndex = -1;
        ensureStreamsSize();
        saveCamerasToIni();
        rebuildTiles();
    }
}

void CameraWall::onClearAll()
{
    if (QMessageBox::question(this, "Összes törlése", "Biztosan törlöd az összes kamerát?") == QMessageBox::Yes)
    {
        cams.clear();
        selectedIndex = -1;
        stopAndDeleteAllStreams();
        saveCamerasToIni();
        rebuildTiles();
    }
}

void CameraWall::tileClicked(int flatIndex)
{
    selectedIndex = flatIndex;
    statusBar()->showMessage(QString("Kijelölt: %1").arg(cams.value(flatIndex).name));
}

void CameraWall::toggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void CameraWall::toggleFpsLimit()
{
    m_limitFps15 = !m_limitFps15;
    actFps->setChecked(m_limitFps15);
    saveViewToIni();
    rebuildTiles();
}

void CameraWall::toggleAutoRotate()
{
    m_autoRotate = !m_autoRotate;
    actAutoRotate->setChecked(m_autoRotate);
    saveViewToIni();
    if (!m_autoRotate)
        rotateTimer.stop();
    else if (cams.size() > perPage())
        rotateTimer.start();
}

void CameraWall::toggleKeepAlive()
{
    m_keepBackgroundStreams = !m_keepBackgroundStreams;
    actKeepAlive->setChecked(m_keepBackgroundStreams);
    saveViewToIni();

    if (!m_keepBackgroundStreams)
    {
        // kapcs. ki: csak a látható csempék legyenek kötve; a háttérben ne fusson
        for (int i = 0; i < m_streams.size(); ++i)
        {
            detachLow(i);
            detachHigh(i);
            if (m_streams[i].low)
            {
                m_streams[i].low->stop();
            }
            if (m_streams[i].high)
            {
                m_streams[i].high->stop();
            }
        }
    }
    else
    {
        // kapcs. be: a jelenlegi oldalon biztosan legyen kapcsolat, a többin előtöltünk szükség szerint
        for (int i = 0; i < cams.size(); ++i)
        {
            QString err;
            ensureLowConnected(i, &err); // legalább a low legyen bekapcsolva
        }
    }
}

void CameraWall::nextPage()
{
    if (!m_autoRotate)
        return;
    int pages = qMax(1, (cams.size() + perPage() - 1) / perPage());
    if (pages <= 1)
        return;
    currentPage = (currentPage + 1) % pages;
    rebuildTiles();
}

void CameraWall::reloadAll()
{
    // cache-eket nem töröljük itt, csak újraindítjuk a lejátszást
    for (int i = 0; i < m_streams.size(); ++i)
    {
        if (m_streams[i].low && m_streams[i].low->source().isValid())
        {
            m_streams[i].low->stop();
            m_streams[i].low->play();
        }
        if (m_streams[i].high && m_streams[i].high->source().isValid())
        {
            m_streams[i].high->stop();
            m_streams[i].high->play();
        }
    }
}

// ======= fókusz =======

void CameraWall::onTileFullscreenRequested(VideoTile *src)
{
    if (focusDlg)
    {
        exitFocus();
        return;
    }
    int idx = tileIndexMap.value(src, -1);
    enterFocus(idx);
}

void CameraWall::exitFocus()
{
    if (!focusDlg)
        return;

    // válaszd le a high playert a dummy sinkre, hogy a stream megmaradjon a háttérben
    int idx = tileIndexMap.value(focusTile, -1);
    if (idx >= 0)
        detachHigh(idx);

    if (focusTile)
    {
        focusTile->deleteLater();
        focusTile = nullptr;
    }
    focusDlg->close();
    focusDlg->deleteLater();
    focusDlg = nullptr;
}

void CameraWall::enterFocus(int camIdx)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return;

    QString err;
    if (!ensureHighConnected(camIdx, &err))
    {
        QMessageBox::warning(this, "Fókusz nézet", "Nem tudtam lekérni a fókusz (high) streamet: " + err);
        return;
    }

    focusDlg = new QDialog(this, Qt::Window | Qt::FramelessWindowHint);
    auto *lay = new QVBoxLayout(focusDlg);
    lay->setContentsMargins(0, 0, 0, 0);
    focusTile = new VideoTile(m_limitFps15, focusDlg);
    lay->addWidget(focusTile);
    focusTile->setName(cams[camIdx].name);

    // A high lejátszót a fókusz csempére kötjük
    attachHighToTile(camIdx, focusTile);

    connect(focusTile, &VideoTile::fullscreenRequested, this, &CameraWall::exitFocus);
    focusDlg->installEventFilter(this);
    focusDlg->showFullScreen();
}

// ======= stream pool =======

void CameraWall::ensureStreamsSize()
{
    // összeigazítjuk a kamerák számával
    int old = m_streams.size();
    m_streams.resize(cams.size());
    // az új bejegyzésekhez semmit nem kell azonnal létrehozni; lazán inicializálunk
    if (cams.size() < old)
    {
        // a fölös playereket leállítjuk (biztonság kedvéért)
        for (int i = cams.size(); i < old; ++i)
        {
            if (m_streams[i].low)
                m_streams[i].low->stop();
            if (m_streams[i].high)
                m_streams[i].high->stop();
        }
    }
}

bool CameraWall::ensureLowConnected(int camIdx, QString *err)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return false;
    auto &E = m_streams[camIdx];

    if (!E.low)
    {
        E.low = new QMediaPlayer(this);
        E.low->setLoops(QMediaPlayer::Infinite);
        auto *aud = new QAudioOutput(this);
        aud->setVolume(0.0);
        E.low->setAudioOutput(aud);
    }
    if (!E.dummyLow)
    {
        E.dummyLow = new QVideoSink(this);
    }
    // alapállapotban a dummy-ra irányítjuk (háttérben fusson)
    E.low->setVideoSink(E.dummyLow);

    if (!E.low->isPlaying() || !E.low->source().isValid())
    {
        QString e;
        QUrl url = playbackUrlFor(camIdx, /*high*/ false, &e);
        if (!url.isValid())
        {
            if (err)
                *err = e;
            return false;
        }
        E.low->setSource(url);
        E.low->play();
    }
    return true;
}

bool CameraWall::ensureHighConnected(int camIdx, QString *err)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return false;
    auto &E = m_streams[camIdx];

    if (!E.high)
    {
        E.high = new QMediaPlayer(this);
        E.high->setLoops(QMediaPlayer::Infinite);
        auto *aud = new QAudioOutput(this);
        aud->setVolume(0.0);
        E.high->setAudioOutput(aud);
    }
    if (!E.dummyHigh)
    {
        E.dummyHigh = new QVideoSink(this);
    }
    E.high->setVideoSink(E.dummyHigh);

    if (!E.high->isPlaying() || !E.high->source().isValid())
    {
        QString e;
        QUrl url = playbackUrlFor(camIdx, /*high*/ true, &e);
        if (!url.isValid())
        {
            if (err)
                *err = e;
            return false;
        }
        E.high->setSource(url);
        E.high->play();
    }
    return true;
}

void CameraWall::attachLowToTile(int camIdx, VideoTile *tile)
{
    if (camIdx < 0 || camIdx >= m_streams.size() || !tile)
        return;
    auto &E = m_streams[camIdx];
    if (!E.low)
        return;
    tile->applyToPlayer(E.low);
}

void CameraWall::detachLow(int camIdx)
{
    if (camIdx < 0 || camIdx >= m_streams.size())
        return;
    auto &E = m_streams[camIdx];
    if (E.low && E.dummyLow)
        E.low->setVideoSink(E.dummyLow);
}

void CameraWall::attachHighToTile(int camIdx, VideoTile *tile)
{
    if (camIdx < 0 || camIdx >= m_streams.size() || !tile)
        return;
    auto &E = m_streams[camIdx];
    if (!E.high)
        return;
    tile->applyToPlayer(E.high);
}

void CameraWall::detachHigh(int camIdx)
{
    if (camIdx < 0 || camIdx >= m_streams.size())
        return;
    auto &E = m_streams[camIdx];
    if (E.high && E.dummyHigh)
        E.high->setVideoSink(E.dummyHigh);
}

void CameraWall::stopAndDeleteAllStreams()
{
    for (auto &E : m_streams)
    {
        if (E.low)
        {
            E.low->stop();
            E.low->deleteLater();
            E.low = nullptr;
        }
        if (E.high)
        {
            E.high->stop();
            E.high->deleteLater();
            E.high = nullptr;
        }
        if (E.dummyLow)
        {
            E.dummyLow->deleteLater();
            E.dummyLow = nullptr;
        }
        if (E.dummyHigh)
        {
            E.dummyHigh->deleteLater();
            E.dummyHigh = nullptr;
        }
    }
    m_streams.clear();
}

// ======= layout (csempék) =======

void CameraWall::rebuildTiles()
{
    // 1) a jelenlegi csempék leválasztása a playerekről, mielőtt törölnénk őket
    for (auto *t : std::as_const(tiles))
    {
        int idx = tileIndexMap.value(t, -1);
        if (idx >= 0)
            detachLow(idx);
    }

    // 2) layout ürítése
    QLayoutItem *child;
    while ((child = grid->takeAt(0)) != nullptr)
    {
        if (auto *w = child->widget())
            w->deleteLater();
        delete child;
    }
    tiles.clear();
    tileIndexMap.clear();

    // 3) stretch beállítások
    for (int r = 0; r < 9; ++r)
        grid->setRowStretch(r, 0);
    for (int c = 0; c < 9; ++c)
        grid->setColumnStretch(c, 0);
    for (int r = 0; r < gridN; ++r)
        grid->setRowStretch(r, 1);
    for (int c = 0; c < gridN; ++c)
        grid->setColumnStretch(c, 1);

    if (cams.isEmpty())
    {
        auto *lbl = new QLabel("Nincs konfigurált kamera. Jobb klikk → Hozzáadás…");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color:#9fb2c8; font-size:18px");
        grid->addWidget(lbl, 0, 0, 1, 1);
        statusBar()->showMessage("0 kamera");
        rotateTimer.stop();
        return;
    }

    const int pages = qMax(1, (cams.size() + perPage() - 1) / perPage());
    if (currentPage >= pages)
        currentPage = 0;
    if (m_autoRotate && cams.size() > perPage())
        rotateTimer.start();
    else
        rotateTimer.stop();

    const int start = currentPage * perPage();
    const int end = qMin(start + perPage(), cams.size());
    int shown = 0;

    for (int i = start; i < end; ++i)
    {
        auto *tile = new VideoTile(m_limitFps15);
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tiles << tile;
        const int r = shown / gridN;
        const int c = shown % gridN;
        grid->addWidget(tile, r, c);
        tile->setName(cams[i].name);

        QString err;
        bool ok = ensureLowConnected(i, &err);
        if (!ok)
        {
            tile->setToolTip(err);
        }
        else
        {
            attachLowToTile(i, tile);
        }

        tile->installEventFilter(this);
        connect(tile, &VideoTile::fullscreenRequested, this, [this, tile]()
                { onTileFullscreenRequested(tile); });

        tileIndexMap[tile] = i;
        shown++;
    }

    statusBar()->showMessage(QString("%1 kamera • %2/%3 oldal • %4 • Rács: %5×%5 • FPS: %6 • Gyors váltás: %7")
                                 .arg(cams.size())
                                 .arg(currentPage + 1)
                                 .arg(pages)
                                 .arg((m_autoRotate && cams.size() > perPage()) ? "10s váltás" : "nincs automatikus váltás")
                                 .arg(gridN)
                                 .arg(m_limitFps15 ? "15" : "max")
                                 .arg(m_keepBackgroundStreams ? "be" : "ki"));
}

// ======= INI =======

void CameraWall::loadFromIni()
{
    QSettings s(Util::iniPath(), QSettings::IniFormat);
    // Cameras
    cams.clear();
    s.beginGroup("Cameras");
    int count = s.value("count", 0).toInt();
    for (int i = 0; i < count; ++i)
    {
        s.beginGroup(QString("Camera%1").arg(i));
        Camera c;
        c.name = s.value("name").toString();
        c.mode = (s.value("mode", "rtsp").toString() == "onvif") ? Camera::ONVIF : Camera::RTSP;
        if (c.mode == Camera::RTSP)
        {
            c.rtspManual = Util::urlFromEncoded(s.value("rtsp").toString());
        }
        else
        {
            c.onvifDeviceXAddr = QUrl(s.value("onvif_device_xaddr").toString());
            c.onvifMediaXAddr = QUrl(s.value("onvif_media_xaddr").toString());
            c.onvifUser = s.value("onvif_user").toString();
            c.onvifPass = s.value("onvif_pass").toString();
            c.onvifLowToken = s.value("onvif_low_token").toString();
            c.onvifHighToken = s.value("onvif_high_token").toString();
            c.rtspUriLowCached = s.value("rtsp_low_cached").toString();
            c.rtspUriHighCached = s.value("rtsp_high_cached").toString();
        }
        if (c.name.isEmpty())
            c.name = (c.mode == Camera::RTSP ? c.rtspManual.host() : c.onvifDeviceXAddr.host());
        cams << c;
        s.endGroup();
    }
    s.endGroup();

    // View
    s.beginGroup("View");
    gridN = s.value("gridN", 2).toInt();
    m_limitFps15 = s.value("fpsLimit15", true).toBool();
    m_autoRotate = s.value("autoRotate10s", true).toBool();
    m_keepBackgroundStreams = s.value("keepBackground", true).toBool();
    s.endGroup();

    ensureStreamsSize();
}

void CameraWall::saveCamerasToIni()
{
    QSettings s(Util::iniPath(), QSettings::IniFormat);
    s.beginGroup("Cameras");
    s.remove("");
    s.setValue("count", cams.size());
    for (int i = 0; i < cams.size(); ++i)
    {
        s.beginGroup(QString("Camera%1").arg(i));
        const Camera &c = cams[i];
        s.setValue("name", c.name);
        s.setValue("mode", c.mode == Camera::ONVIF ? "onvif" : "rtsp");
        if (c.mode == Camera::RTSP)
        {
            s.setValue("rtsp", QString::fromUtf8(c.rtspManual.toEncoded()));
        }
        else
        {
            s.setValue("onvif_device_xaddr", c.onvifDeviceXAddr.toString());
            s.setValue("onvif_media_xaddr", c.onvifMediaXAddr.toString());
            s.setValue("onvif_user", c.onvifUser);
            s.setValue("onvif_pass", c.onvifPass);
            s.setValue("onvif_low_token", c.onvifLowToken);
            s.setValue("onvif_high_token", c.onvifHighToken);
            s.setValue("rtsp_low_cached", c.rtspUriLowCached);
            s.setValue("rtsp_high_cached", c.rtspUriHighCached);
        }
        s.endGroup();
    }
    s.endGroup();
    s.sync();
}

void CameraWall::saveViewToIni()
{
    QSettings s(Util::iniPath(), QSettings::IniFormat);
    s.beginGroup("View");
    s.setValue("gridN", gridN);
    s.setValue("fpsLimit15", m_limitFps15);
    s.setValue("autoRotate10s", m_autoRotate);
    s.setValue("keepBackground", m_keepBackgroundStreams);
    s.endGroup();
    s.sync();
}

// ======= URL/ONVIF =======

QUrl CameraWall::playbackUrlFor(int camIdx, bool high, QString *errOut)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return QUrl();
    Camera &c = cams[camIdx];
    if (c.mode == Camera::RTSP)
        return c.rtspManual;

    QString cached = high ? c.rtspUriHighCached : c.rtspUriLowCached;
    if (!cached.isEmpty())
        return Util::urlFromEncoded(cached);

    OnvifClient cli;
    QString err;
    QUrl media = c.onvifMediaXAddr;
    if (media.isEmpty())
    {
        QUrl med;
        if (!cli.getCapabilities(c.onvifDeviceXAddr, c.onvifUser, c.onvifPass, med, &err))
        {
            if (errOut)
                *errOut = err;
            return QUrl();
        }
        media = med;
        c.onvifMediaXAddr = media;
    }
    QString token = high ? c.onvifHighToken : c.onvifLowToken;
    if (token.isEmpty())
    {
        if (errOut)
            *errOut = "Hiányzó ONVIF profil token";
        return QUrl();
    }

    QString uri;
    if (!cli.getStreamUri(media, c.onvifUser, c.onvifPass, token, uri, &err))
    {
        if (errOut)
            *errOut = err;
        return QUrl();
    }
    if (high)
        c.rtspUriHighCached = uri;
    else
        c.rtspUriLowCached = uri;
    saveCamerasToIni();
    return Util::urlFromEncoded(uri);
}
