#pragma once

#include <QMainWindow>
#include <QGridLayout>
#include <QTimer>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include "videotile.h"
#include "editcameradialog.h"
#include "onvifclient.h"
#include "util.h"

class QGridLayout;
class QDialog;
class QActionGroup;

class CameraWall : public QMainWindow
{
    Q_OBJECT
public:
    CameraWall();

protected:
    void contextMenuEvent(QContextMenuEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    int perPage() const;
    void setGridN(int n);
    void onAdd();
    void onEditSelected();
    void onRemoveSelected();
    void onClearAll();
    void tileClicked(int flatIndex);
    void toggleFullscreen();
    void toggleFpsLimit();
    void nextPage();
    void reloadAll();

    void onTileFullscreenRequested(VideoTile *src);
    void exitFocus();
    void enterFocus(int camIdx);
    void rebuildTiles();

    // ÚJ: sorrendező párbeszéd és az automata lapozás kezelése
    void showReorderDialog();
    void updateRotationTimer(int pages);

    QUrl playbackUrlFor(int camIdx, bool high, QString *errOut = nullptr);

    void loadFromIni();
    void saveCamerasToIni();
    void saveViewToIni();

private:
    QGridLayout *grid{};
    QVector<Camera> cams;
    QVector<VideoTile *> tiles;
    QHash<VideoTile *, int> tileIndexMap;
    int selectedIndex = -1;
    int currentPage = 0;
    QTimer rotateTimer;
    int gridN = 2;
    bool m_limitFps15 = true;

    // Actions
    QAction *actFps{};
    QAction *actFull{};
    QAction *actEdit{};
    QActionGroup *gridGroup{};
    QAction *actGrid2{};
    QAction *actGrid3{};
    QAction *actAutoRotate{}; // ÚJ

    // Fókusz nézet
    QDialog *focusDlg{};
    VideoTile *focusTile{};

    // Nézet beállítások
    bool m_autoRotate = true; // ÚJ
};
