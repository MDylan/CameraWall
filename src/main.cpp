#include <QApplication>
#include "camerawall.h"
#include "language.h"

int main(int argc, char **argv)
{

    // Szélesebb Qt/FFmpeg log-szabályok
    qputenv("QT_LOGGING_RULES",
            QByteArray("qt.multimedia.ffmpeg=false\n"
                       "qt.multimedia.ffmpeg.*=false\n"
                       "qt.multimedia.audio.*=false\n"));

    // Ha a Qt ezt kezeli a te buildedben, ezek lejjebb tekerik az FFmpeg saját logját:
    qputenv("QT_FFMPEG_LOG", "0");
    qputenv("QT_FFMPEG_LOG_LEVEL", "quiet"); // vagy: "error"

    QApplication app(argc, argv);

    //disable error messages
    QLoggingCategory::setFilterRules(QStringLiteral(
        "qt.multimedia.debug=false\n"
        "qt.multimedia.info=false\n"
        "qt.multimedia.ffmpeg=false\n"
        "qt.multimedia.ffmpeg.*=false\n"
        "qt.multimedia.audio.*=false\n"));

    app.setWindowIcon(QIcon(":/icons/app.ico"));
    Language::instance().loadFromArgs(app);
    QApplication::setApplicationDisplayName(
        Language::instance().t("app.title", "IP Kamera fal"));

    // ha van nálad olyan, hogy parancssorból nyelv: --lang=hu/en, azt a Language osztályod kezeli.
    // Itt csak megpróbáljuk betölteni a mentettet (ha a Language ezt tudja),P
    // ha nincs ilyen API-d, ez a sor maradhat üresen.
    // Példa (ha van):
    // Language::instance().loadFromArgs(app);  // ha nálad létezik
    // egyébként próbáljuk a mentettet:
    // (ha nincs ilyen metódusod, kommenteld ki)
    // Language::instance().loadSaved();

    CameraWall w;
    w.show(); // a konstruktora már full screenre teszi, de ez itt ártalmatlan
    return app.exec();
}
