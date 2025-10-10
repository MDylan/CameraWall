#include <QApplication>
#include "camerawall.h"
#include "language.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Language::instance().loadFromArgs(app);

    // ha van nálad olyan, hogy parancssorból nyelv: --lang=hu/en, azt a Language osztályod kezeli.
    // Itt csak megpróbáljuk betölteni a mentettet (ha a Language ezt tudja),
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
