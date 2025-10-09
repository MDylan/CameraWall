#pragma once

#include <QMainWindow>
#include <QGridLayout>
#include <QStackedLayout>
#include <QActionGroup>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <QSettings>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>

#include "videotile.h"
#include "editcameradialog.h" // Camera struct itt van
#include "reorderdialog.h"
#include "language.h"
#include "util.h"

class CameraWall : public QMainWindow
{
    Q_OBJECT
public:
    CameraWall();

protected:
    void contextMenuEvent(QContextMenuEvent *e) override;

private slots:
    // menü / működés
    void onAdd();
    void onEditSelected();
    void onRemoveSelected();
    void onClearAll();
    void onReorder();
    void toggleFullscreen();
    void toggleFpsLimit();
    void toggleAutoRotate();
    void toggleKeepAlive();
    void onTileFullscreenRequested(); // tagfüggvény slot – UniqueConnection elkerüléséhez
    void nextPage();
    void reloadAll();

private:
    // layout / nézet
    int perPage() const { return gridN * gridN; }
    void applyGridStretch();
    void setGridN(int n);
    void rebuildTiles();
    void enterFocus(int camIdx);
    void exitFocus();

    // adatok
    QUrl playbackUrlFor(int camIdx, bool high, QString *errOut = nullptr);
    void loadFromIni();
    void saveCamerasToIni();
    void saveViewToIni();

    // nyelvi címkék újrarakása
    void retitle();

private:
    // --- központi stack: 0 = rács, 1 = fókusz ---
    QWidget *central{};
    QStackedLayout *stack{};
    // rács oldal
    QWidget *pageGrid{};
    QGridLayout *grid{};
    // fókusz oldal
    QWidget *pageFocus{};
    QVBoxLayout *focusLayout{};
    QWidget *focusPlaceholder{}; // a rácsban hagyott hely

    // kamera/nézet állapot
    QVector<Camera> cams;
    QVector<VideoTile *> tiles;
    QHash<VideoTile *, int> tileIndexMap;
    int selectedIndex{-1};
    int currentPage{0};
    QTimer rotateTimer;
    int gridN{2};
    bool m_limitFps15{true};
    bool m_autoRotate{true};
    bool m_keepBackgroundStreams{true};

    // fókusz állapot
    int m_focusCamIdx{-1};
    VideoTile *focusTile{nullptr};
    int focusRow{-1}, focusCol{-1};

    // menük
    QAction *actFps{}, *actFull{}, *actEdit{}, *actKeepAlive{}, *actAutoRotate{}, *actGrid2{}, *actGrid3{}, *actReorder{};
    QActionGroup *gridGroup{};
    QMenu *menuLanguage{};
    QActionGroup *langGroup{};
    QAction *actLangHu{}, *actLangEn{};
};
