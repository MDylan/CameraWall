#include "camerawall.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include "editcameradialog.h"
#include "onvifclient.h"
#include "util.h" // iniPath(), urlFromEncoded(), stb.

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
    // ÚJ: sorrendezés
    mCams->addSeparator();
    mCams->addAction("Kamerák sorrendje…", this, [this]
                     { showReorderDialog(); });

    auto *mView = menuBar()->addMenu("&Nézet");
    actFull = mView->addAction("Teljes képernyő (ablak)", this, [this]
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

    // ÚJ: automatikus oldalváltás kapcsoló
    actAutoRotate = mView->addAction("Automatikus oldalváltás (10s)");
    actAutoRotate->setCheckable(true);
    connect(actAutoRotate, &QAction::toggled, this, [this](bool on)
            {
                m_autoRotate = on;
                saveViewToIni();
                int pages = qMax(1, (cams.size() + perPage() - 1) / perPage());
                updateRotationTimer(pages);
                rebuildTiles(); // státusz frissítéshez is
            });

    statusBar()->showMessage("F11 – teljes képernyő • Duplakatt/⛶: fókusz • ONVIF: rács=low, fókusz=high");

    connect(&rotateTimer, &QTimer::timeout, this, [this]
            { nextPage(); });
    rotateTimer.setInterval(10000);

    // Betöltés és induló állapotok
    loadFromIni();
    if (gridN != 2 && gridN != 3)
        gridN = 2;
    actGrid2->setChecked(gridN == 2);
    actGrid3->setChecked(gridN == 3);
    actFps->setChecked(m_limitFps15);
    actAutoRotate->setChecked(m_autoRotate); // ÚJ

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
    // ÚJ: automata lapozás menüből is elérhető
    menu.addAction(actAutoRotate);
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

int CameraWall::perPage() const { return gridN * gridN; }

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

void CameraWall::nextPage()
{
    int pages = qMax(1, (cams.size() + perPage() - 1) / perPage());
    if (pages <= 1)
        return;
    currentPage = (currentPage + 1) % pages;
    rebuildTiles();
}

void CameraWall::reloadAll()
{
    for (auto *t : std::as_const(tiles))
        if (t)
            t->stop();
    rebuildTiles();
}

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
    if (focusTile)
    {
        focusTile->stop();
        focusTile->deleteLater();
        focusTile = nullptr;
    }
    focusDlg->close();
    focusDlg->deleteLater();
    focusDlg = nullptr;
}

QUrl CameraWall::playbackUrlFor(int camIdx, bool high, QString *errOut)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return QUrl();
    const Camera &c = cams[camIdx];
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
        cams[camIdx].onvifMediaXAddr = media;
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
        cams[camIdx].rtspUriHighCached = uri;
    else
        cams[camIdx].rtspUriLowCached = uri;
    saveCamerasToIni();
    return Util::urlFromEncoded(uri);
}

void CameraWall::enterFocus(int camIdx)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return;
    QString err;
    QUrl url = playbackUrlFor(camIdx, /*high*/ true, &err);
    if (url.isEmpty())
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
    focusTile->playUrl(url);
    connect(focusTile, &VideoTile::fullscreenRequested, this, &CameraWall::exitFocus);
    focusDlg->installEventFilter(this);
    focusDlg->showFullScreen();
}

void CameraWall::rebuildTiles()
{
    // Layout ürítése
    QLayoutItem *child;
    while ((child = grid->takeAt(0)) != nullptr)
    {
        if (auto *w = child->widget())
            w->deleteLater();
        delete child;
    }
    tiles.clear();
    tileIndexMap.clear();

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
    updateRotationTimer(pages);

    if (currentPage >= pages)
        currentPage = 0;

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
        QUrl play = playbackUrlFor(i, /*high*/ false, &err);
        if (play.isEmpty())
        {
            tile->setToolTip(err);
        }
        else
        {
            tile->playUrl(play);
        }
        tile->installEventFilter(this);
        connect(tile, &VideoTile::fullscreenRequested, this, [this, tile]()
                { onTileFullscreenRequested(tile); });
        tileIndexMap[tile] = i;
        shown++;
    }

    const QString pagesStr =
        cams.size() > perPage()
            ? (m_autoRotate ? "10s váltás" : "váltás: ki")
            : "minden látható";

    statusBar()->showMessage(QString("%1 kamera • %2/%3 oldal • %4 • Rács: %5×%5 • FPS: %6")
                                 .arg(cams.size())
                                 .arg(currentPage + 1)
                                 .arg(pages)
                                 .arg(pagesStr)
                                 .arg(gridN)
                                 .arg(m_limitFps15 ? "15" : "max"));
}

void CameraWall::showReorderDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Kamerák sorrendje");
    auto *lay = new QVBoxLayout(&dlg);

    auto *list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setAlternatingRowColors(true);
    for (int i = 0; i < cams.size(); ++i)
    {
        auto *it = new QListWidgetItem(cams[i].name, list);
        it->setData(Qt::UserRole, i);
    }
    lay->addWidget(list);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QVector<Camera> reordered;
    reordered.reserve(cams.size());
    for (int row = 0; row < list->count(); ++row)
    {
        int oldIdx = list->item(row)->data(Qt::UserRole).toInt();
        reordered.push_back(cams[oldIdx]);
    }
    cams = std::move(reordered);
    selectedIndex = -1;
    saveCamerasToIni();
    rebuildTiles();
}

void CameraWall::updateRotationTimer(int pages)
{
    if (pages > 1 && m_autoRotate)
        rotateTimer.start();
    else
        rotateTimer.stop();
}

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
    m_autoRotate = s.value("autoRotate", true).toBool(); // ÚJ
    s.endGroup();
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
    s.setValue("autoRotate", m_autoRotate); 
    s.endGroup();
    s.sync();
}
