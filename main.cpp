// Single-file demo using Qt 6 (Widgets + Multimedia + MultimediaWidgets)
// Features:
// - Fullscreen by default, menu to add/remove cameras
// - Stores camera list in an INI file next to the executable (cameras.ini)
// - RTSP support via Qt Multimedia backend (FFmpeg/GStreamer depending on platform)
// - Auth supported via username/password fields (embedded into the RTSP URL safely)
// - Grid layouts: 2×2 (default) and 3×3; remembers last choice in INI
// - If more cameras than fit on a page, auto-rotates pages every 10s
// - Optional 15 FPS limit (default ON) with QVideoSink throttling; remembered in INI
// - Fill without cropping (aspect ignored) to always fill tiles
// - PER-TILE fullscreen: double-click or ⛶ button toggles focused fullscreen view; double-click again to return
// - Basic error overlays per tile
//
// Build (CMake example):
//   cmake_minimum_required(VERSION 3.21)
//   project(CameraWall)
//   set(CMAKE_CXX_STANDARD 17)
//   find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)
//   qt_standard_project_setup()
//   add_executable(CameraWall main.cpp)
//   target_link_libraries(CameraWall PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)
//
// NOTE for Qt 6.9+: QVideoWidget lives in the QtMultimediaWidgets module.
// In Qt6, volume/mute is controlled via QAudioOutput (not QMediaPlayer directly).

#include <QtWidgets>
#include <QtMultimedia>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtMultimedia/QVideoSink>

struct Camera
{
    QString name;
    QUrl url; // includes user/pass if set
};

static QString iniPath()
{
    const QString dir = QCoreApplication::applicationDirPath();
    return QDir(dir).filePath("cameras.ini");
}

class VideoTile : public QWidget
{
    Q_OBJECT
public:
    explicit VideoTile(bool limitFps15 = true, QWidget *parent = nullptr)
        : QWidget(parent), m_player(new QMediaPlayer(this)), m_audio(new QAudioOutput(this)), m_limitFps(limitFps15)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        if (m_limitFps)
        {
            // QLabel + QVideoSink + saját rajzolás → 15 FPS-re korlátozható
            m_canvas = new QLabel(this);
            m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_canvas->setAlignment(Qt::AlignCenter);
            m_canvas->setStyleSheet("background:#000");
            layout->addWidget(m_canvas);

            m_sink = new QVideoSink(this);
            connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoTile::onFrame);

            // 15 FPS throttle timer (66ms)
            m_fpsTimer.setInterval(66);
            m_fpsTimer.setTimerType(Qt::PreciseTimer);
            connect(&m_fpsTimer, &QTimer::timeout, this, &VideoTile::flushFrame);
            m_fpsTimer.start();
        }
        else
        {
            // QVideoWidget gyors path (GPU-gyorsítás lehetséges)
            m_video = new QVideoWidget(this);
            m_video->setAspectRatioMode(Qt::IgnoreAspectRatio); // torzíthat, de nem vág – mindig kitölti
            m_video->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            layout->addWidget(m_video);
        }

        m_player->setAudioOutput(m_audio);
        m_audio->setMuted(true); // alapból némítva

        // Overlay címke
        m_label = new QLabel(this);
        m_label->setStyleSheet("background:rgba(0,0,0,0.5); color:#e8eef7; padding:4px 8px; border-radius:8px;");
        m_label->move(10, 10);
        m_label->raise();

        // Teljes képernyő gomb (jobb alsó sarok)
        btnFS = new QPushButton(QString::fromUtf8("⛶"), this);
        btnFS->setToolTip("Teljes képernyő");
        btnFS->setCursor(Qt::PointingHandCursor);
        btnFS->setFixedSize(28, 28);
        btnFS->setStyleSheet("QPushButton{background:rgba(0,0,0,0.45); color:#e8eef7; border:1px solid rgba(255,255,255,0.25); border-radius:6px;} QPushButton:hover{background:rgba(0,0,0,0.6);} ");
        btnFS->raise();
        connect(btnFS, &QPushButton::clicked, this, [this]
                { emit fullscreenRequested(this); });

        // Hiba felirat
        m_err = new QLabel(this);
        m_err->setStyleSheet("background:rgba(0,0,0,0.6); color:#ffb3b3; padding:6px 10px; border-radius:8px;");
        m_err->setWordWrap(true);
        m_err->setVisible(false);
        m_err->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_err->setMinimumHeight(24);
        m_err->setTextInteractionFlags(Qt::TextSelectableByMouse);

        m_err->adjustSize();
        m_err->move(10, height() - m_err->height() - 10);

        connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &errorString)
                {
            m_err->setText("Hiba: " + errorString);
            m_err->setVisible(true); });
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState st)
                {
            if (st == QMediaPlayer::PlayingState) m_err->setVisible(false); });
    }

    void setCamera(const Camera &cam)
    {
        m_cam = cam;
        m_label->setText(cam.name);
        m_label->adjustSize();

        if (m_limitFps)
        {
            m_player->setVideoSink(m_sink);
        }
        else
        {
            m_player->setVideoOutput(m_video);
        }
        m_player->setSource(cam.url);
        m_player->setLoops(QMediaPlayer::Infinite);
        m_player->play();
    }

    Camera currentCamera() const { return m_cam; }

    void stop() { m_player->stop(); }

signals:
    void fullscreenRequested(VideoTile *self);

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        m_label->move(10, 10);
        m_err->move(10, height() - m_err->height() - 10);
        if (btnFS)
            btnFS->move(width() - btnFS->width() - 10, height() - btnFS->height() - 10);
        // Repaint last frame to match new size when throttling
        if (m_limitFps && !m_lastImage.isNull() && m_canvas)
            paintImage(m_lastImage);
    }

    void mouseDoubleClickEvent(QMouseEvent *e) override
    {
        Q_UNUSED(e)
        emit fullscreenRequested(this);
    }

private slots:
    void onFrame(const QVideoFrame &frame)
    {
        // Tároljuk az utolsó képkockát, és a timer ütemére rajzolunk
        if (!frame.isValid())
            return;
        QImage img = frame.toImage();
        if (img.isNull())
            return;
        m_lastImage = img;
        // Ha nincs elindítva a timer (biztonsági), most azonnal is rajzolhatunk
        if (!m_fpsTimer.isActive())
            flushFrame();
    }

    void flushFrame()
    {
        if (m_lastImage.isNull() || !m_canvas)
            return;
        paintImage(m_lastImage);
    }

private:
    void paintImage(const QImage &img)
    {
        if (!m_canvas)
            return;
        // Torzítva kitöltjük a csempét (IgnoreAspectRatio)
        const QSize target = m_canvas->size().expandedTo(QSize(1, 1));
        QPixmap pm = QPixmap::fromImage(img).scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_canvas->setPixmap(std::move(pm));
    }

    Camera m_cam;
    QMediaPlayer *m_player{};
    QAudioOutput *m_audio{};

    // Gyors út: QVideoWidget (GPU), vagy korlátozott FPS: QLabel+QVideoSink
    bool m_limitFps{true};
    QVideoWidget *m_video{}; // ha !m_limitFps
    QLabel *m_canvas{};      // ha m_limitFps
    QVideoSink *m_sink{};    // ha m_limitFps
    QTimer m_fpsTimer;       // ha m_limitFps
    QImage m_lastImage;      // ha m_limitFps

    QLabel *m_label{};
    QLabel *m_err{};
    QPushButton *btnFS{};
};

class AddCameraDialog : public QDialog
{
    Q_OBJECT
public:
    AddCameraDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Kamera hozzáadása");
        auto *form = new QFormLayout(this);
        name = new QLineEdit;
        form->addRow("Név:", name);
        host = new QLineEdit;
        host->setPlaceholderText("rtsp://example.com:554/stream");
        form->addRow("RTSP URL:", host);
        user = new QLineEdit;
        form->addRow("Felhasználó:", user);
        pass = new QLineEdit;
        pass->setEchoMode(QLineEdit::Password);
        form->addRow("Jelszó:", pass);
        auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(btns);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        resize(420, 0);
    }

    Camera camera() const
    {
        Camera c;
        c.name = name->text().trimmed();
        const QString raw = host->text().trimmed();
        QUrl u = QUrl::fromEncoded(raw.toUtf8(), QUrl::StrictMode);
        const QString lower = raw.toLower();
        const bool urlHasInlineCreds = lower.contains("user=") || lower.contains("username=") || lower.contains("password=");
        if (!urlHasInlineCreds)
        {
            if (!user->text().isEmpty())
                u.setUserName(user->text());
            if (!pass->text().isEmpty())
                u.setPassword(pass->text());
        }
        c.url = u;
        if (c.name.isEmpty())
            c.name = c.url.host();
        return c;
    }

private:
    QLineEdit *name, *host, *user, *pass;
};

class CameraWall : public QMainWindow
{
    Q_OBJECT
public:
    CameraWall()
    {
        setWindowTitle("IP Kamera fal (RTSP)");
        QWidget *central = new QWidget;
        setCentralWidget(central);
        grid = new QGridLayout(central);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(6);

        // Menus
        auto *mCams = menuBar()->addMenu("&Kamerák");
        QAction *actAdd = mCams->addAction("Hozzáadás…", this, &CameraWall::onAdd);
        QAction *actRemove = mCams->addAction("Kijelölt törlése", this, &CameraWall::onRemoveSelected);
        QAction *actClear = mCams->addAction("Összes törlése", this, &CameraWall::onClearAll);
        mCams->addSeparator();
        QAction *actReload = mCams->addAction("Újratöltés", this, &CameraWall::reloadAll);

        auto *mView = menuBar()->addMenu("&Nézet");
        actFull = mView->addAction("Teljes képernyő (ablak)", this, &CameraWall::toggleFullscreen);
        actFull->setShortcut(Qt::Key_F11);

        actFps = mView->addAction("FPS limit 15", this, &CameraWall::toggleFpsLimit);
        actFps->setCheckable(true);

        // Grid submenu with radio options (2×2, 3×3)
        QMenu *mGrid = mView->addMenu("Rács");
        gridGroup = new QActionGroup(this);
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

        statusBar()->showMessage("F11 – teljes képernyő • Duplakatt/⛶: fókusz nézet");

        connect(&rotateTimer, &QTimer::timeout, this, &CameraWall::nextPage);
        rotateTimer.setInterval(10000);

        // Load settings (cameras + view)
        loadFromIni();

        // Default values if not present
        if (gridN != 2 && gridN != 3)
            gridN = 2;
        actGrid2->setChecked(gridN == 2);
        actGrid3->setChecked(gridN == 3);
        actFps->setChecked(m_limitFps15);

        rebuildTiles();
        QTimer::singleShot(0, this, [this]
                           { this->showFullScreen(); });
    }

protected:
    void contextMenuEvent(QContextMenuEvent *e) override
    {
        QMenu menu(this);
        menu.addAction("Hozzáadás…", this, &CameraWall::onAdd);
        menu.addAction("Kijelölt törlése", this, &CameraWall::onRemoveSelected);
        menu.addSeparator();
        menu.addAction("Teljes képernyő váltása (F11)", this, &CameraWall::toggleFullscreen);
        menu.addAction(actFps);
        // Reuse the same grid actions in the context menu
        QMenu *sub = menu.addMenu("Rács");
        sub->addAction(actGrid2);
        sub->addAction(actGrid3);
        menu.exec(e->globalPos());
    }

private slots:
    void onAdd()
    {
        AddCameraDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted)
        {
            cams.push_back(dlg.camera());
            saveCamerasToIni();
            rebuildTiles();
        }
    }

    void onRemoveSelected()
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

    void onClearAll()
    {
        if (QMessageBox::question(this, "Összes törlése", "Biztosan törlöd az összes kamerát?") == QMessageBox::Yes)
        {
            cams.clear();
            selectedIndex = -1;
            saveCamerasToIni();
            rebuildTiles();
        }
    }

    void tileClicked(int flatIndex)
    {
        selectedIndex = flatIndex;
        statusBar()->showMessage(QString("Kijelölt: %1").arg(cams.value(flatIndex).name));
    }

    void toggleFullscreen()
    {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
    }

    void toggleFpsLimit()
    {
        m_limitFps15 = !m_limitFps15;
        actFps->setChecked(m_limitFps15);
        saveViewToIni();
        rebuildTiles();
    }

    void nextPage()
    {
        int pages = qMax(1, (cams.size() + perPage() - 1) / perPage());
        if (pages <= 1)
            return;
        currentPage = (currentPage + 1) % pages;
        rebuildTiles();
    }

    void reloadAll()
    {
        for (auto *t : std::as_const(tiles))
            if (t)
                t->stop();
        rebuildTiles();
    }

    void onTileFullscreenRequested(VideoTile *src)
    {
        if (focusDlg)
        {
            exitFocus();
            return;
        }
        enterFocus(src->currentCamera());
    }

    void exitFocus()
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

private:
    int perPage() const { return gridN * gridN; }

    void setGridN(int n)
    {
        if (n != 2 && n != 3)
            return;
        gridN = n;
        saveViewToIni();
        rebuildTiles();
    }

    void enterFocus(const Camera &cam)
    {
        focusDlg = new QDialog(this, Qt::Window | Qt::FramelessWindowHint);
        focusDlg->setModal(true);
        focusDlg->setAttribute(Qt::WA_DeleteOnClose, false);
        auto *lay = new QVBoxLayout(focusDlg);
        lay->setContentsMargins(0, 0, 0, 0);

        focusTile = new VideoTile(m_limitFps15, focusDlg);
        lay->addWidget(focusTile);
        focusTile->setCamera(cam);
        connect(focusTile, &VideoTile::fullscreenRequested, this, &CameraWall::exitFocus);

        // ESC kilépés
        focusDlg->installEventFilter(this);

        focusDlg->showFullScreen();
    }

    void rebuildTiles()
    {
        // Clear layout
        QLayoutItem *child;
        while ((child = grid->takeAt(0)) != nullptr)
        {
            if (auto *w = child->widget())
                w->deleteLater();
            delete child;
        }
        tiles.clear();
        tileIndexMap.clear();

        // Update row/column stretch to make grid cells even
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
        if (cams.size() > perPage())
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
            tile->setCamera(cams[i]);

            // interactions
            tile->installEventFilter(this); // for single-click selection
            connect(tile, &VideoTile::fullscreenRequested, this, &CameraWall::onTileFullscreenRequested);
            tileIndexMap[tile] = i; // store flat index
            shown++;
        }
        statusBar()->showMessage(QString("%1 kamera • %2/%3 oldal • %4 • Rács: %5×%5 • FPS: %6")
                                     .arg(cams.size())
                                     .arg(currentPage + 1)
                                     .arg(pages)
                                     .arg(cams.size() > perPage() ? "10s váltás" : "minden látható")
                                     .arg(gridN)
                                     .arg(m_limitFps15 ? "15" : "max"));
    }

    bool eventFilter(QObject *obj, QEvent *event) override
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

    void loadFromIni()
    {
        QSettings s(iniPath(), QSettings::IniFormat);

        // Cameras
        cams.clear();
        s.beginGroup("Cameras");
        int count = s.value("count", 0).toInt();
        for (int i = 0; i < count; ++i)
        {
            s.beginGroup(QString("Camera%1").arg(i));
            Camera c;
            c.name = s.value("name").toString();
            QByteArray enc = s.value("url").toString().toUtf8();
            c.url = QUrl::fromEncoded(enc, QUrl::StrictMode);
            if (c.name.isEmpty())
                c.name = c.url.host();
            cams << c;
            s.endGroup();
        }
        s.endGroup();

        // View prefs
        s.beginGroup("View");
        gridN = s.value("gridN", 2).toInt();
        m_limitFps15 = s.value("fpsLimit15", true).toBool();
        s.endGroup();
    }

    void saveCamerasToIni()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        s.beginGroup("Cameras");
        s.remove(""); // clear cameras group only
        s.setValue("count", cams.size());
        for (int i = 0; i < cams.size(); ++i)
        {
            s.beginGroup(QString("Camera%1").arg(i));
            s.setValue("name", cams[i].name);
            s.setValue("url", QString::fromUtf8(cams[i].url.toEncoded()));
            s.endGroup();
        }
        s.endGroup();
        s.sync();
    }

    void saveViewToIni()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        s.beginGroup("View");
        s.setValue("gridN", gridN);
        s.setValue("fpsLimit15", m_limitFps15);
        s.endGroup();
        s.sync();
    }

private:
    QGridLayout *grid{};
    QVector<Camera> cams;
    QVector<VideoTile *> tiles;
    QHash<VideoTile *, int> tileIndexMap;
    int selectedIndex = -1;
    int currentPage = 0;
    QTimer rotateTimer;

    // View prefs
    int gridN = 2;            // 2 or 3
    bool m_limitFps15 = true; // remember in INI
    QAction *actFps{};
    QAction *actFull{};
    QActionGroup *gridGroup{};
    QAction *actGrid2{};
    QAction *actGrid3{};

    // Focus view
    QDialog *focusDlg{};
    VideoTile *focusTile{};
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Dark palette (optional)
    QPalette pal = app.palette();
    pal.setColor(QPalette::Window, QColor(11, 15, 20));
    pal.setColor(QPalette::WindowText, QColor(232, 238, 247));
    pal.setColor(QPalette::Base, QColor(17, 23, 34));
    pal.setColor(QPalette::Text, QColor(232, 238, 247));
    pal.setColor(QPalette::Button, QColor(28, 38, 56));
    pal.setColor(QPalette::ButtonText, QColor(232, 238, 247));
    app.setPalette(pal);

    CameraWall w;
    w.show();
    return app.exec();
}

#include "main.moc"
