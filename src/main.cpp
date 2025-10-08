#include <QApplication>
#include "camerawall.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // (Opcionális) sötét paletta
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
