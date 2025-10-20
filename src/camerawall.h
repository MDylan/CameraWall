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
#include <QShortcut>
#include <QLabel>
#include <QPixmap>
#include <QFileDialog>

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
    void keyPressEvent(QKeyEvent *e) override;

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
    void onTileFullscreenRequested(); // tagfüggvény slot
    void nextPage();
    void reloadAll();
    void chooseBackgroundImage();
    void clearBackgroundImage();
    void toggleStatusbarVisible();

private:
    // layout / nézet
    int perPage() const { return gridRows * gridCols; }
    void applyGridStretch();
    void setGridN(int rc); // rc = rows*10 + cols, pl. 22, 33, 32
    void rebuildTiles();
    void enterFocus(int camIdx);
    void exitFocus();

    // adatok
    QUrl playbackUrlFor(int camIdx, bool high, QString *errOut = nullptr);
    void loadFromIni();
    void saveCamerasToIni();
    void saveViewToIni();

    // nyelvi címkék
    void retitle();
    void focusShow(int camIdx);
    void updateGridChecks();

    // --- ÚJ: háttérkép segédek ---
    void applyBackgroundImage(const QString &path); 
    void updateBackgroundVisible();
    void showDefaultStatusHint(); // alap státuszszöveg (rács / fókusz szerint)
    void applyStatusbarVisible();

private:
    // --- központi stack: 0 = rács, 1 = fókusz ---
    QWidget *central{};
    QStackedLayout *stack{};
    // háttér
    QLabel *backgroundLabel{};  
    QString backgroundPath{};   
    bool backgroundCleared = false;
    QString backgroundFromIni{};   
    QPixmap backgroundPixmap{}; 
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

    // ÚJ: téglalap rács
    int gridRows{2};
    int gridCols{2};

    bool m_limitFps15{true};
    bool m_autoRotate{true};
    bool m_keepBackgroundStreams{true};
    bool m_statusbarVisible{true};

    // fókusz állapot
    int m_focusCamIdx{-1};
    VideoTile *focusTile{nullptr};
    int focusRow{-1}, focusCol{-1};

    // menük
    QAction *actFps{}, *actFull{}, *actEdit{}, *actKeepAlive{}, *actAutoRotate{},
        *actGrid22{}, *actGrid33{}, *actGrid32{}, *actReorder{};
    QAction *actLangHu{}, *actLangEn{}, *actBackground{}, *actBackgroundClear{}, *actStatusbar{};

    QMenu *mCams{}, *mView{}, *mHelp{}, *menuLanguage{}, *mGridMenu{};
    QActionGroup *gridGroup{}, *langGroup{};
    QAction *actAdd{}, *actRemove{}, *actClear{}, *actReload{}, *actExit{}, *actAbout{};

    // ESC gyorsbillentyű
    QShortcut *shortcutEsc{nullptr};

    void setupMenusOnce();
    void updateMenuTexts();
    void updateAppTitle();
};
