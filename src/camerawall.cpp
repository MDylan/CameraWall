#include "camerawall.h"
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QScrollArea>
#include <QDebug>
#include <QApplication>
#include <QKeyEvent> // + ESC kezelés
#include <QShortcut> // + ESC gyorsbillentyű

namespace
{
    QIcon loadAppIcon()
    {
        QIcon ico(":/icons/res/app.ico");
        if (!ico.isNull())
            return ico;
        QIcon png(":/icons/res/app_256.png");
        return png;
    }
}

CameraWall::CameraWall()
{
    setWindowIcon(loadAppIcon());
    updateAppTitle();

    // --- központi stack ---
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

    // ESC gyorsbillentyű
    shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(shortcutEsc, &QShortcut::activated, this, [this]
            {
        if (stack && stack->currentWidget() == pageFocus)
            exitFocus(); });

    // --- Menü ---
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
        if (Util::askOkCancel(this,"dlg.exit","Kilépés","msg.exit","Biztosan kilépsz az alkalmazásból?")) {
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

    // Grid menü (ÚJ: 3×2 is)
    mGridMenu = new QMenu(mView);
    mView->addMenu(mGridMenu);

    gridGroup = new QActionGroup(mGridMenu);
    gridGroup->setExclusive(true);
    actGrid22 = mGridMenu->addAction("2×2");
    actGrid33 = mGridMenu->addAction("3×3");
    actGrid32 = mGridMenu->addAction("3×2");
    for (auto *a : {actGrid22, actGrid33, actGrid32})
        a->setCheckable(true);
    gridGroup->addAction(actGrid22);
    gridGroup->addAction(actGrid33);
    gridGroup->addAction(actGrid32);
    connect(actGrid22, &QAction::triggered, this, [this]
            { setGridN(22); });
    connect(actGrid33, &QAction::triggered, this, [this]
            { setGridN(33); });
    connect(actGrid32, &QAction::triggered, this, [this]
            { setGridN(32); });

    // Súgó + Nyelv (változatlan)
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
            { if (Language::instance().load("hu")) updateMenuTexts(); });
    connect(actLangEn, &QAction::triggered, this, [this]
            { if (Language::instance().load("en")) updateMenuTexts(); });

    actAbout = mHelp->addAction({}, this, [this]
                                {
        const QIcon appIcon = loadAppIcon();
        QPixmap pm = appIcon.pixmap(64, 64);
        auto *box = new QMessageBox(this);
        box->setWindowIcon(appIcon);
        box->setIconPixmap(pm);
        box->setWindowTitle(Language::instance().t("dlg.about", "Rólunk"));
        box->setTextFormat(Qt::RichText);
        box->setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::LinksAccessibleByKeyboard);
        box->setText(
            "CameraWall<br>"
            "Just a simple IP camera software. Enjoy! :)<br>"
            "Created by: Dávid Molnár<br>"
            "<a href=\"https://github.com/MDylan/CameraWall\">github.com/MDylan/CameraWall</a><br>"
            "<br>© 2025");
        box->setStandardButtons(QMessageBox::Ok);
        box->exec(); });

    updateMenuTexts();

    statusBar()->showMessage(Language::instance().t(
        "status.hint",
        "F11 – teljes képernyő • Duplakatt/⛶: fókusz • Egy stream mód"));

    connect(&Language::instance(), &Language::languageChanged, this, [this](const QString &)
            {
        updateMenuTexts();
        updateAppTitle();
        statusBar()->showMessage(Language::instance().t(
            "status.hint",
            "F11 – teljes képernyő • Duplakatt/⛶: fókusz • Egy stream mód")); });

    connect(&rotateTimer, &QTimer::timeout, this, &CameraWall::nextPage);
    rotateTimer.setInterval(10000);

    // beállítások
    loadFromIni();
    // jelöld ki a megfelelő rácsot
    updateGridChecks();
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
    menuBar()->clear();
    CameraWall *tmp = this;
    (void)tmp;
    rebuildTiles();
}

void CameraWall::contextMenuEvent(QContextMenuEvent *e)
{
    QWidget *hit = QApplication::widgetAt(e->globalPos());
    VideoTile *vt = nullptr;
    for (QWidget *p = hit; p; p = p->parentWidget())
    {
        if ((vt = qobject_cast<VideoTile *>(p)))
            break;
    }
    const int ctxIdx = vt ? tileIndexMap.value(vt, -1) : -1;

    QMenu menu(this);
    menu.addAction(Language::instance().t("menu.add", "Hozzáadás…"), this, &CameraWall::onAdd);
    menu.addAction(Language::instance().t("menu.edit", "Szerkesztés…"),
                   this, [this, ctxIdx]
                   {
        if (ctxIdx >= 0) selectedIndex = ctxIdx;
        onEditSelected(); });

    menu.addAction(Language::instance().t("menu.remove", "Kijelölt törlése"),
                   this, [this, ctxIdx]
                   {
        if (ctxIdx >= 0) selectedIndex = ctxIdx;
        onRemoveSelected(); });
    menu.addSeparator();
    menu.addAction(Language::instance().t("menu.fullscreen", "Teljes képernyő (ablak)"), this, &CameraWall::toggleFullscreen);
    QMenu *sub = menu.addMenu(Language::instance().t("menu.grid", "Rács"));
    sub->addAction(actGrid22);
    sub->addAction(actGrid32);
    sub->addAction(actGrid33);    
    menu.addAction(Language::instance().t("menu.reorder", "Kamerák sorrendje…"), this, &CameraWall::onReorder);
    menu.exec(e->globalPos());
}

// + ESC fallback: ha valami elnyeli a shortcutot, akkor is működjön
void CameraWall::keyPressEvent(QKeyEvent *e)
{
    qDebug() << "[keyPressEvent]" << e->key();
    if (stack && stack->currentWidget() == pageFocus)
    {
        if (e->key() == Qt::Key_Escape)
        {
            exitFocus();
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_Right)
        {
            focusShow(m_focusCamIdx + 1);
            return;
        }
        if (e->key() == Qt::Key_Left)
        {
            focusShow(m_focusCamIdx - 1);
            return;
        }
    }
    QMainWindow::keyPressEvent(e);
}

void CameraWall::applyGridStretch()
{
    for (int r = 0; r < 9; ++r)
        grid->setRowStretch(r, 0);
    for (int c = 0; c < 9; ++c)
        grid->setColumnStretch(c, 0);
    for (int r = 0; r < gridRows; ++r)
        grid->setRowStretch(r, 1);
    for (int c = 0; c < gridCols; ++c)
        grid->setColumnStretch(c, 1);
}

void CameraWall::setGridN(int rc)
{
    // rc = C*10 + R  (C = columns, R = rows), pl. 32 -> 3 oszlop × 2 sor
    int cols = rc / 10;
    int rows = rc % 10;
    if (rows <= 0 || cols <= 0 || rows > 9 || cols > 9)
        return;

    gridCols = cols;
    gridRows = rows;

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
    qDebug() << "Edit selected:" << selectedIndex;
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

        // FONTOS: téglalap rácshoz a sor = shown / gridCols, oszlop = shown % gridCols
        const int r = shown / gridCols;
        const int c = shown % gridCols;
        grid->addWidget(tile, r, c);
        tile->setName(cams[i].name);

        QString err;
        QUrl play = playbackUrlFor(i, false, &err);
        if (play.isEmpty())
            tile->setToolTip(err);
        else
            tile->playUrl(play);

        connect(tile, &VideoTile::fullscreenRequested, this, &CameraWall::onTileFullscreenRequested);
        tileIndexMap[tile] = i;
        shown++;
    }

    grid->invalidate();
    stack->setCurrentWidget(pageGrid);

    const int pagesCount = qMax(1, (cams.size() + perPage() - 1) / perPage());
    statusBar()->showMessage(
        QString("%1: %2 • %3/%4 • %5 • %6 %7×%8 • FPS: %9")
            .arg(Language::instance().t("status.cams", "Kamerák"))
            .arg(cams.size())
            .arg(currentPage + 1)
            .arg(pagesCount)
            .arg(cams.size() > perPage() ? Language::instance().t("status.rotate", "10s váltás")
                                         : Language::instance().t("status.allvisible", "minden látható"))
            .arg(Language::instance().t("status.grid", "Rács:"))
            .arg(gridCols)
            .arg(gridRows)
            .arg(m_limitFps15 ? "15" : "max"));
    updateGridChecks();
}

void CameraWall::loadFromIni()
{
    QSettings s(Util::iniPath(), QSettings::IniFormat);

    // Cameras (változatlan)
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

    // View – ÚJ: gridRC (egy számként, pl. 32)
    s.beginGroup("View");
    const int rc = s.value("gridN", 22).toInt(); // visszafelé kompatibilitás: ugyanaz a kulcs
    int cols = rc / 10;
    int rows = rc % 10;
    if (cols <= 0)
        cols = 2;
    if (rows <= 0)
        rows = 2;
    gridCols = cols;
    gridRows = rows;

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
    s.setValue("gridN", gridCols * 10 + gridRows); // pl. 32 = 3 oszlop × 2 sor
    s.setValue("fpsLimit15", m_limitFps15);
    s.setValue("autoRotate", m_autoRotate);
    s.setValue("keepAlive", m_keepBackgroundStreams);
    s.endGroup();
    s.sync();
}

void CameraWall::setupMenusOnce()
{
    // (változatlan – itt most nem használjuk)
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
    QApplication::setApplicationDisplayName(title);
}
void CameraWall::focusShow(int camIdx)
{
    qDebug() << "[focusShow] in camIdx =" << camIdx
             << " tiles.size()=" << tiles.size()
             << " m_focusCamIdx=" << m_focusCamIdx;

    if (tiles.isEmpty())
    {
        qDebug() << "[focusShow] -> tiles is empty, return";
        return;
    }

    // Körkörös index (-1 -> utolsó; túlcsordulás -> 0)
    if (camIdx < 0)
    {
        camIdx = tiles.size() - 1;
        qDebug() << "[focusShow] wrapped to last =>" << camIdx;
    }
    else if (camIdx >= tiles.size())
    {
        camIdx = 0;
        qDebug() << "[focusShow] wrapped to first =>" << camIdx;
    }

    qDebug() << "[focusShow] #1 normalized camIdx =" << camIdx;

    // Ha még nem fókusz módban vagyunk: egyszerűen lépjünk be oda
    if (m_focusCamIdx == -1)
    {
        qDebug() << "[focusShow] not in focus mode -> enterFocus(" << camIdx << ")";
        enterFocus(camIdx);
        return;
    }

    qDebug() << "[focusShow] #2 currently focused =" << m_focusCamIdx;

    // Ugyanarra ne csináljunk semmit
    if (camIdx == m_focusCamIdx)
    {
        qDebug() << "[focusShow] target equals current, nothing to do";
        return;
    }

    // --- Itt már fókusz módban vagyunk, és másik csempére váltunk ---
    VideoTile *newTile = tiles[camIdx];
    VideoTile *oldTile = focusTile;

    if (!newTile || !oldTile || !grid || !focusLayout || !pageFocus || !pageGrid)
    {
        qDebug() << "[focusShow] missing ptrs ->"
                 << "newTile?" << (newTile != nullptr)
                 << "oldTile?" << (oldTile != nullptr)
                 << "grid?" << (grid != nullptr)
                 << "focusLayout?" << (focusLayout != nullptr);
        return;
    }

    // Keressük meg az új tile rácsbeli pozícióját
    int n = grid->count();
    int newRow = -1, newCol = -1, rs = 0, cs = 0;
    for (int i = 0; i < n; ++i)
    {
        QLayoutItem *it = grid->itemAt(i);
        if (!it)
            continue;
        if (it->widget() == newTile)
        {
            grid->getItemPosition(i, &newRow, &newCol, &rs, &cs);
            break;
        }
    }
    qDebug() << "[focusShow] grid items =" << n
             << " -> new pos row=" << newRow << " col=" << newCol;

    if (newRow < 0 || newCol < 0)
    {
        // Ha valamiért nem találjuk (pl. nem ezen az oldalon lenne a csempe),
        // essünk vissza egy egyszerű exit+enter műveletre.
        qDebug() << "[focusShow] newTile not found in current grid -> exit+enter fallback";
        exitFocus();
        enterFocus(camIdx);
        return;
    }

    // 1) Az aktuális fókusz csempét visszatesszük a rácsba a placeholder régi helyére
    qDebug() << "[focusShow] put oldTile back to grid at" << focusRow << focusCol;
    focusLayout->removeWidget(oldTile);
    oldTile->setParent(pageGrid);
    grid->addWidget(oldTile, focusRow, focusCol);

    // 2) A placeholder-t átrakjuk az új csempe helyére (hogy tudjuk, hova térjünk vissza legközelebb)
    if (focusPlaceholder)
    {
        qDebug() << "[focusShow] move placeholder to" << newRow << newCol;
        grid->addWidget(focusPlaceholder, newRow, newCol);
    }
    else
    {
        qDebug() << "[focusShow] WARNING: focusPlaceholder is null!";
    }

    // 3) Az új csempét kiemeljük fókuszba
    qDebug() << "[focusShow] move newTile to focus";
    grid->removeWidget(newTile);
    newTile->setParent(pageFocus);
    newTile->setMinimumSize(0, 0);
    newTile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    focusLayout->addWidget(newTile);

    // 4) Állapot frissítés
    focusTile = newTile;
    m_focusCamIdx = camIdx;
    focusRow = newRow;
    focusCol = newCol;

    qDebug() << "[focusShow] updated m_focusCamIdx =" << m_focusCamIdx
             << " focusRow=" << focusRow << " focusCol=" << focusCol;
}

void CameraWall::updateGridChecks()
{
    // Első számjegy = oszlopok, második = sorok
    if (actGrid22)
        actGrid22->setChecked(gridCols == 2 && gridRows == 2);
    if (actGrid33)
        actGrid33->setChecked(gridCols == 3 && gridRows == 3);
    if (actGrid32)
        actGrid32->setChecked(gridCols == 3 && gridRows == 2); // 3 oszlop × 2 sor
}
