#pragma once

#include <QMainWindow>
#include <QGridLayout>
#include <QActionGroup>
#include <QDialog>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>

#include "util.h"
#include "videotile.h"
#include "editcameradialog.h" // <- Itt van a Camera definíciója
#include "onvifclient.h"

class CameraWall : public QMainWindow
{
    Q_OBJECT
public:
    CameraWall();

protected:
    void contextMenuEvent(QContextMenuEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    int perPage() const { return gridN * gridN; }
    void setGridN(int n);

    void onAdd();
    void onEditSelected();
    void onRemoveSelected();
    void onClearAll();
    void tileClicked(int flatIndex);
    void toggleFullscreen();
    void toggleFpsLimit();
    void toggleAutoRotate();
    void toggleKeepAlive();
    void nextPage();
    void reloadAll();

    void onTileFullscreenRequested(VideoTile *src);
    void exitFocus();
    void enterFocus(int camIdx);

    QUrl playbackUrlFor(int camIdx, bool high, QString *errOut = nullptr);

    void rebuildTiles();
    void loadFromIni();
    void saveCamerasToIni();
    void saveViewToIni();
    QMetaObject::Connection m_highFirstFrameConn{};
    int m_focusCamIdx{-1};
    void prepareModelMutation(int removingIdx = -1); // stop lejátszók, fókusz kilépés

    // ---- Háttér stream pool ----
    struct StreamEntry
    {
        QMediaPlayer *low = nullptr;
        QMediaPlayer *high = nullptr;
        QVideoSink *dummyLow = nullptr;
        QVideoSink *dummyHigh = nullptr;
    };
    QVector<StreamEntry> m_streams;

    void ensureStreamsSize();
    bool ensureLowConnected(int camIdx, QString *err = nullptr);
    bool ensureHighConnected(int camIdx, QString *err = nullptr);
    void attachLowToTile(int camIdx, VideoTile *tile);
    void detachLow(int camIdx);
    void attachHighToTile(int camIdx, VideoTile *tile);
    void detachHigh(int camIdx);
    void stopAndDeleteAllStreams();

private:
    QGridLayout *grid{};
    QVector<Camera> cams; // <- a Camera típust a editcameradialog.h adja
    QVector<VideoTile *> tiles;
    QHash<VideoTile *, int> tileIndexMap;
    int selectedIndex = -1;
    int currentPage = 0;
    QTimer rotateTimer;
    int gridN = 2;
    bool m_limitFps15 = true;

    // menük
    QAction *actFps{};
    QAction *actFull{};
    QAction *actEdit{};
    QActionGroup *gridGroup{};
    QAction *actGrid2{};
    QAction *actGrid3{};
    QAction *actAutoRotate{};
    QAction *actKeepAlive{};

    QDialog *focusDlg{};
    VideoTile *focusTile{};

    bool m_autoRotate = true;
    bool m_keepBackgroundStreams = true;

    VideoTile *tileForIndex(int camIdx) const;
};
