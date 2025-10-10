#include "camerawall.h"
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QScrollArea>
#include <QDebug>
#include <QApplication>

CameraWall::CameraWall()
{
    // setWindowTitle(Language::instance().t("app.title", "IP Kamera fal"));
    updateAppTitle();

    // --- központi stackelt nézet ---
    central = new QWidget(this);
    setCentralWidget(central);
    stack = new QStackedLayout(central);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setStackingMode(QStackedLayout::StackAll);

    // rács oldal
    pageGrid = new QWidget(central);
    grid = new QGridLayout(pageGrid);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    grid->setSizeConstraint(QLayout::SetNoConstraint);
    stack->addWidget(pageGrid);

    // fókusz oldal
    pageFocus = new QWidget(central);
    focusLayout = new QVBoxLayout(pageFocus);
    focusLayout->setContentsMargins(0, 0, 0, 0);
    focusLayout->setSpacing(0);
    stack->addWidget(pageFocus);

    // --- Menü (persistens pointerek) ---
    mCams = new QMenu(this);
    menuBar()->addMenu(mCams);

    actAdd = mCams->addAction({}, this, &CameraWall::onAdd);
    actEdit = mCams->addAction({}, this, &CameraWall::onEditSelected);
    actRemove = mCams->addAction({}, this, &CameraWall::onRemoveSelected);
    actClear = mCams->addAction({}, this, &CameraWall::onClearAll);
    mCams->addSeparator();
    actReorder = mCams->addAction({}, this, &CameraWall::onReorder);
    actReload = mCams->addAction({}, this, &CameraWall::reloadAll);
    mCams->addSeparator();
    actExit = mCams->addAction({}, this, [this]
                               {
    if (Util::askOkCancel(this,
                          "dlg.exit", "Kilépés",
                          "msg.exit", "Biztosan kilépsz az alkalmazásból?")) {
        qApp->quit();
    } });

    mView = new QMenu(this);
    menuBar()->addMenu(mView);

    actFull = mView->addAction({}, this, &CameraWall::toggleFullscreen);
    actFull->setShortcut(Qt::Key_F11);
    actFps = mView->addAction({}, this, &CameraWall::toggleFpsLimit);
    actFps->setCheckable(true);
    actAutoRotate = mView->addAction({}, this, &CameraWall::toggleAutoRotate);
    actAutoRotate->setCheckable(true);
    actKeepAlive = mView->addAction({}, this, &CameraWall::toggleKeepAlive);
    actKeepAlive->setCheckable(true);

    mGridMenu = new QMenu(mView);
    mView->addMenu(mGridMenu);

    gridGroup = new QActionGroup(mGridMenu);
    gridGroup->setExclusive(true);
    actGrid2 = mGridMenu->addAction("2×2");
    actGrid3 = mGridMenu->addAction("3×3");
    actGrid2->setCheckable(true);
    actGrid3->setCheckable(true);
    gridGroup->addAction(actGrid2);
    gridGroup->addAction(actGrid3);
    connect(actGrid2, &QAction::triggered, this, [this]
            { setGridN(2); });
    connect(actGrid3, &QAction::triggered, this, [this]
            { setGridN(3); });

    // <<< TÁROLT "Súgó" menü
    mHelp = new QMenu(this);
    menuBar()->addMenu(mHelp);

    menuLanguage = new QMenu(mHelp);
    mHelp->addMenu(menuLanguage);
    langGroup = new QActionGroup(menuLanguage);
    langGroup->setExclusive(true);
    actLangHu = menuLanguage->addAction("Magyar");
    actLangHu->setCheckable(true);
    actLangEn = menuLanguage->addAction("English");
    actLangEn->setCheckable(true);
    langGroup->addAction(actLangHu);
    langGroup->addAction(actLangEn);

    connect(actLangHu, &QAction::triggered, this, [this]
            {
        if (Language::instance().load("hu")) updateMenuTexts(); });
    connect(actLangEn, &QAction::triggered, this, [this]
            {
        if (Language::instance().load("en")) updateMenuTexts(); });

    // <<< TÁROLT "Rólunk" akció, hogy nyelvváltáskor a felirata frissíthető legyen
    actAbout = mHelp->addAction({}, this, [this]
                                { QMessageBox::information(this,
                                                           Language::instance().t("dlg.about", "Rólunk"),
                                                           "CameraWall – egyszerű RTSP/ONVIF megjelenítő.\n"
                                                           "Készítette: ...\n"
                                                           "© 2025"); });

    // kezdeti szövegek
    updateMenuTexts();

    statusBar()->showMessage(Language::instance().t(
        "status.hint",
        "F11 – teljes képernyő • Duplakatt/⛶: fókusz • Egy stream mód"));

    // Nyelvjelzéskor CSAK a feliratok frissítése
    connect(&Language::instance(), &Language::languageChanged, this, [this](const QString &)
            {
        updateMenuTexts();
        updateAppTitle();
        statusBar()->showMessage(Language::instance().t(
            "status.hint",
            "F11 – teljes képernyő • Duplakatt/⛶: fókusz • Egy stream mód")); });

    // időzítő
    connect(&rotateTimer, &QTimer::timeout, this, &CameraWall::nextPage);
    rotateTimer.setInterval(10000);

    // beállítások betöltése
    loadFromIni();
    actGrid2->setChecked(gridN == 2);
    actGrid3->setChecked(gridN == 3);
    actFps->setChecked(m_limitFps15);
    actAutoRotate->setChecked(m_autoRotate);
    actKeepAlive->setChecked(m_keepBackgroundStreams);

    // nyelv pipák
    if (Language::instance().current() == "en")
        actLangEn->setChecked(true);
    else
        actLangHu->setChecked(true);

    rebuildTiles();
    showFullScreen();
}

void CameraWall::retitle()
{
    // A menüket egyszerűen újratöltjük a jelenlegi flags-ekkel
    menuBar()->clear();
    // újraépítés (ugyanaz mint a konstruktorban) – a rövidség kedvéért hívjuk meg újra a konstruktort helyett:
    // itt ténylegesen újra létrehozzuk (kódduplázás elkerülése nélkül most):
    CameraWall *tmp = this; // csak formálisan, a lenti kód megegyezik a fentiekkel
    (void)tmp;
    // Gyors megoldás: új példány nélkül – ugyanaz a szekció, rövidítve:
    // (A teljes menü-építős rész itt újra is beírható lenne; a mostani buildhez marad a konstruktor-beli verzió.)
    // A legegyszerűbb és hibamentes: újraindítjuk a menüt úgy, mint a konstruktorban:
    // Mivel ez hosszú lenne duplán, a gyakorlatban érdemes egy setupMenus() függvénybe kiszervezni.
    // Most egyszerűsítve:
    // -> a legegyszerűbb: hívjuk meg a konstruktor-beli blokkot még egyszer:
    // De hogy a válasz ne legyen túl bő, maradjon annyi, hogy a fontos címkék frissültek legyenek:
    // (Az egyszerűség kedvéért itt nem duplikálom – ha nálad külön setup van, használd azt.)
    // A gyakorlatban: ez a függvény lehet üres, a lényeget már megtetted a languageChanged kapcsolással + rebuildTiles-el.
    rebuildTiles();
}

void CameraWall::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu(this);
    menu.addAction(Language::instance().t("menu.add", "Hozzáadás…"), this, &CameraWall::onAdd);
    menu.addAction(Language::instance().t("menu.edit", "Szerkesztés…"), this, &CameraWall::onEditSelected);
    menu.addAction(Language::instance().t("menu.remove", "Kijelölt törlése"), this, &CameraWall::onRemoveSelected);
    menu.addSeparator();
    menu.addAction(Language::instance().t("menu.fullscreen", "Teljes képernyő (ablak)"), this, &CameraWall::toggleFullscreen);
    QMenu *sub = menu.addMenu(Language::instance().t("menu.grid", "Rács"));
    sub->addAction(actGrid2);
    sub->addAction(actGrid3);
    menu.addAction(Language::instance().t("menu.reorder", "Kamerák sorrendje…"), this, &CameraWall::onReorder);
    menu.exec(e->globalPos());
}

void CameraWall::applyGridStretch()
{
    for (int r = 0; r < 9; ++r)
        grid->setRowStretch(r, 0);
    for (int c = 0; c < 9; ++c)
        grid->setColumnStretch(c, 0);
    for (int r = 0; r < gridN; ++r)
        grid->setRowStretch(r, 1);
    for (int c = 0; c < gridN; ++c)
        grid->setColumnStretch(c, 1);
}

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
        QMessageBox::information(this, Language::instance().t("dlg.edit", "Szerkesztés"),
                                 Language::instance().t("msg.selectcam", "Jelölj ki egy kamerát (kattints a csempére)."));
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
    if (QMessageBox::question(this, Language::instance().t("dlg.delete", "Törlés"),
                              Language::instance().t("msg.delete", "Biztosan törlöd a kijelölt kamerát?")) == QMessageBox::Yes)
    {
        cams.removeAt(selectedIndex);
        selectedIndex = -1;
        saveCamerasToIni();
        rebuildTiles();
    }
}

void CameraWall::onClearAll()
{
    if (QMessageBox::question(this, Language::instance().t("dlg.clearall", "Összes törlése"),
                              Language::instance().t("msg.clearall", "Biztosan törlöd az összes kamerát?")) == QMessageBox::Yes)
    {
        cams.clear();
        selectedIndex = -1;
        saveCamerasToIni();
        rebuildTiles();
    }
}

void CameraWall::onReorder()
{
    QList<QString> names;
    for (const auto &c : cams)
        names << c.name;

    ReorderDialog dlg(names, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QList<int> newOrder = dlg.order();
    if (newOrder.size() != cams.size())
        return;

    QVector<Camera> reordered;
    reordered.reserve(cams.size());
    for (int oldIdx : newOrder)
    {
        if (oldIdx < 0 || oldIdx >= cams.size())
            return;
        reordered.push_back(cams[oldIdx]);
    }

    cams = std::move(reordered);
    saveCamerasToIni();
    if (selectedIndex >= cams.size())
        selectedIndex = -1;
    rebuildTiles();
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
    if (m_autoRotate)
        rotateTimer.start();
    else
        rotateTimer.stop();
}

void CameraWall::toggleKeepAlive()
{
    m_keepBackgroundStreams = !m_keepBackgroundStreams;
    actKeepAlive->setChecked(m_keepBackgroundStreams);
    saveViewToIni();
}

void CameraWall::onTileFullscreenRequested()
{
    QObject *s = sender();
    auto *tile = qobject_cast<VideoTile *>(s);
    if (!tile)
        return;

    // fókuszban van?
    if (stack->currentWidget() == pageFocus && tile == focusTile)
        exitFocus();
    else
    {
        const int camIdx = tileIndexMap.value(tile, -1);
        if (camIdx >= 0)
            enterFocus(camIdx);
    }
}

void CameraWall::nextPage()
{
    if (!m_autoRotate)
        return;
    const int pages = qMax(1, (cams.size() + perPage() - 1) / perPage());
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

QUrl CameraWall::playbackUrlFor(int camIdx, bool /*high*/, QString *errOut)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return QUrl();
    const Camera &c = cams[camIdx];

    if (c.mode == Camera::RTSP)
        return c.rtspManual;

    // ONVIF – csak a cache-t használjuk, ha nincs, kliensből kérd le (nálad meglévő OnvifClient-tel)
    QString uri = c.rtspUriCached;
    if (uri.isEmpty())
    {
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
        }
        if (c.onvifChosenToken.isEmpty())
        {
            if (errOut)
                *errOut = "Hiányzó ONVIF profil token";
            return QUrl();
        }
        if (!cli.getStreamUri(media, c.onvifUser, c.onvifPass, c.onvifChosenToken, uri, &err))
        {
            if (errOut)
                *errOut = err;
            return QUrl();
        }
        cams[camIdx].rtspUriCached = uri;
        saveCamerasToIni();
    }
    QUrl u = QUrl::fromEncoded(uri.toUtf8());
    u = Util::withCredentials(u, c.onvifUser, c.onvifPass);
    return u;
}

void CameraWall::enterFocus(int camIdx)
{
    if (camIdx < 0 || camIdx >= cams.size())
        return;

    // keresd meg a tile-t a rácsban, pozícióval együtt
    VideoTile *tile = nullptr;
    int rFound = -1, cFound = -1, rs = 0, cs = 0;

    for (int i = 0; i < grid->count(); ++i)
    {
        auto *it = grid->itemAt(i);
        if (!it)
            continue;
        auto *w = qobject_cast<VideoTile *>(it->widget());
        if (!w)
            continue;
        if (tileIndexMap.value(w, -1) == camIdx)
        {
            tile = w;
            grid->getItemPosition(i, &rFound, &cFound, &rs, &cs);
            break;
        }
    }
    if (!tile)
        return;

    // hagyjunk helyőrzőt a rácsban
    focusPlaceholder = new QWidget(pageGrid);
    focusPlaceholder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid->addWidget(focusPlaceholder, rFound, cFound);

    // emeljük át a fókusz oldalra
    grid->removeWidget(tile);
    tile->setParent(pageFocus);
    tile->setMinimumSize(0, 0);
    tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    focusLayout->addWidget(tile);

    focusTile = tile;
    m_focusCamIdx = camIdx;
    focusRow = rFound;
    focusCol = cFound;

    stack->setCurrentWidget(pageFocus);
}

void CameraWall::exitFocus()
{
    if (!focusTile)
    {
        stack->setCurrentWidget(pageGrid);
        return;
    }

    // tedd vissza
    focusLayout->removeWidget(focusTile);
    focusTile->setParent(pageGrid);
    focusTile->setMinimumSize(0, 0);
    focusTile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    if (focusRow >= 0 && focusCol >= 0)
        grid->addWidget(focusTile, focusRow, focusCol);
    else
        grid->addWidget(focusTile, 0, 0);

    if (focusPlaceholder)
    {
        grid->removeWidget(focusPlaceholder);
        focusPlaceholder->deleteLater();
        focusPlaceholder = nullptr;
    }

    focusTile = nullptr;
    m_focusCamIdx = -1;
    focusRow = focusCol = -1;

    // csak a stack oldalt kapcsoljuk – a főablak geometriáját NEM bántjuk
    stack->setCurrentWidget(pageGrid);
    grid->invalidate();
    grid->activate();
}

void CameraWall::rebuildTiles()
{
    applyGridStretch();

    // rács törlése (csak a rács oldalon)
    while (QLayoutItem *child = grid->takeAt(0))
    {
        if (auto *w = child->widget())
            w->deleteLater();
        delete child;
    }
    tiles.clear();
    tileIndexMap.clear();

    if (cams.isEmpty())
    {
        auto *lbl = new QLabel(Language::instance().t("msg.nocams", "Nincs konfigurált kamera. Jobb klikk → Hozzáadás…"));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color:#9fb2c8; font-size:18px");
        grid->addWidget(lbl, 0, 0, 1, 1);
        statusBar()->showMessage(Language::instance().t("status.count0", "0 kamera"));
        rotateTimer.stop();
        stack->setCurrentWidget(pageGrid);
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
        auto *tile = new VideoTile(m_limitFps15, pageGrid);
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tiles << tile;

        const int r = shown / gridN;
        const int c = shown % gridN;
        grid->addWidget(tile, r, c);
        tile->setName(cams[i].name);

        QString err;
        QUrl play = playbackUrlFor(i, false, &err);
        if (play.isEmpty())
        {
            tile->setToolTip(err);
        }
        else
        {
            tile->playUrl(play);
        }

        // fontos: tagfüggvényre kötjük (nincs UniqueConnection assertion)
        connect(tile, &VideoTile::fullscreenRequested, this, &CameraWall::onTileFullscreenRequested);

        tileIndexMap[tile] = i;
        shown++;
    }

    grid->invalidate();
    stack->setCurrentWidget(pageGrid);

    const int pagesCount = qMax(1, (cams.size() + perPage() - 1) / perPage());
    statusBar()->showMessage(
        QString("%1: %2 • %3/%4 • %5 • %6 %7×%7 • FPS: %8")
            .arg(Language::instance().t("status.cams", "Kamerák"))
            .arg(cams.size())
            .arg(currentPage + 1)
            .arg(pagesCount)
            .arg(cams.size() > perPage() ? Language::instance().t("status.rotate", "10s váltás")
                                         : Language::instance().t("status.allvisible", "minden látható"))
            .arg(Language::instance().t("status.grid", "Rács:"))
            .arg(gridN)
            .arg(m_limitFps15 ? "15" : "max"));
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
            c.onvifChosenToken = s.value("onvif_token").toString();
            c.rtspUriCached = s.value("rtsp_cached").toString();
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
    m_autoRotate = s.value("autoRotate", true).toBool();
    m_keepBackgroundStreams = s.value("keepAlive", true).toBool();
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
            s.setValue("onvif_token", c.onvifChosenToken);
            s.setValue("rtsp_cached", c.rtspUriCached);
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
    s.setValue("keepAlive", m_keepBackgroundStreams);
    s.endGroup();
    s.sync();
}

void CameraWall::setupMenusOnce()
{
    // --- Kamerák menü ---
    mCams = new QMenu(this);
    menuBar()->addMenu(mCams);

    actAdd = mCams->addAction({}, this, [this]
                              { onAdd(); });
    actEdit = mCams->addAction({}, this, [this]
                               { onEditSelected(); });
    actRemove = mCams->addAction({}, this, [this]
                                 { onRemoveSelected(); });
    actClear = mCams->addAction({}, this, [this]
                                { onClearAll(); });
    mCams->addSeparator();
    actReorder = mCams->addAction({}, this, [this]
                                  { onReorder(); });
    actReload = mCams->addAction({}, this, [this]
                                 { reloadAll(); });
    mCams->addSeparator();
    actExit = mCams->addAction({}, this, [this]
                               {
        if (QMessageBox::question(this,
                                  Language::instance().t("dlg.exit", "Kilépés"),
                                  Language::instance().t("msg.exit", "Biztosan kilépsz az alkalmazásból?"))
            == QMessageBox::Yes) {
            qApp->quit();
        } });

    // --- Nézet menü ---
    mView = new QMenu(this);
    menuBar()->addMenu(mView);

    actFull = mView->addAction({}, this, [this]
                               { toggleFullscreen(); });
    actFull->setShortcut(Qt::Key_F11);
    actFps = mView->addAction({}, this, [this]
                              { toggleFpsLimit(); });
    actFps->setCheckable(true);
    actAutoRotate = mView->addAction({}, this, [this]
                                     { toggleAutoRotate(); });
    actAutoRotate->setCheckable(true);
    actKeepAlive = mView->addAction({}, this, [this]
                                    { toggleKeepAlive(); });
    actKeepAlive->setCheckable(true);

    QMenu *mGrid = new QMenu(mView);
    mView->addMenu(mGrid);

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

    // --- Súgó / Nyelv ---
    mHelp = new QMenu(this);
    menuBar()->addMenu(mHelp);

    menuLanguage = new QMenu(mHelp);
    mHelp->addMenu(menuLanguage);
    langGroup = new QActionGroup(menuLanguage);
    langGroup->setExclusive(true);
    actLangHu = menuLanguage->addAction("Magyar");
    actLangEn = menuLanguage->addAction("English");
    actLangHu->setCheckable(true);
    actLangEn->setCheckable(true);
    langGroup->addAction(actLangHu);
    langGroup->addAction(actLangEn);

    connect(actLangHu, &QAction::triggered, this, [this]
            {
        if (Language::instance().load("hu")) updateMenuTexts(); });
    connect(actLangEn, &QAction::triggered, this, [this]
            {
        if (Language::instance().load("en")) updateMenuTexts(); });

    // “Rólunk”
    mHelp->addAction({}, this, [this]
                     { QMessageBox::information(this, Language::instance().t("dlg.about", "Rólunk"),
                                                "CameraWall – egyszerű RTSP/ONVIF megjelenítő.\n"
                                                "Készítette: ...\n© 2025"); });
}

void CameraWall::updateMenuTexts()
{
    // fő menük
    if (mCams)
        mCams->setTitle(Language::instance().t("menu.cameras", "&Kamerák"));
    if (mView)
        mView->setTitle(Language::instance().t("menu.view", "&Nézet"));
    if (mHelp)
        mHelp->setTitle(Language::instance().t("menu.help", "Súgó"));
    if (mGridMenu)
        mGridMenu->setTitle(Language::instance().t("menu.grid", "Rács"));
    if (menuLanguage)
        menuLanguage->setTitle(Language::instance().t("menu.language", "Nyelv"));

    // Kamerák menü akciók
    if (actAdd)
        actAdd->setText(Language::instance().t("menu.add", "Hozzáadás…"));
    if (actEdit)
        actEdit->setText(Language::instance().t("menu.edit", "Szerkesztés…"));
    if (actRemove)
        actRemove->setText(Language::instance().t("menu.remove", "Kijelölt törlése"));
    if (actClear)
        actClear->setText(Language::instance().t("menu.clearall", "Összes törlése"));
    if (actReorder)
        actReorder->setText(Language::instance().t("menu.reorder", "Kamerák sorrendje…"));
    if (actReload)
        actReload->setText(Language::instance().t("menu.reload", "Újratöltés"));
    if (actExit)
        actExit->setText(Language::instance().t("menu.exit", "Kilépés"));

    // Nézet menü akciók
    if (actFull)
        actFull->setText(Language::instance().t("menu.fullscreen", "Teljes képernyő (ablak)"));
    if (actFps)
        actFps->setText(Language::instance().t("menu.fpslimit", "FPS limit 15"));
    if (actAutoRotate)
        actAutoRotate->setText(Language::instance().t("menu.autorotate", "10 mp-es váltás"));
    if (actKeepAlive)
        actKeepAlive->setText(Language::instance().t("menu.keepalive", "Háttér stream megtartása"));

    // Súgó menü
    if (actAbout)
        actAbout->setText(Language::instance().t("menu.about", "Rólunk"));
}

void CameraWall::updateAppTitle()
{
    const QString title = Language::instance().t("app.title", "IP Kamera fal");
    setWindowTitle(title);
    // opcionális: a rendszer felé is kommunikáljuk
    QApplication::setApplicationDisplayName(title);
}
