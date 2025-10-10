#include <QApplication>
#include "camerawall.h"
#include "language.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QScreen>
#include <QWindow>
#include <QIcon>
#include <QLoggingCategory>

static QFile gLogFile;
static void fileLogHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    if (!gLogFile.isOpen())
    {
        gLogFile.setFileName(QCoreApplication::applicationDirPath() + "/CameraWall.log");
        gLogFile.open(QIODevice::Append | QIODevice::Text);
    }
    QTextStream ts(&gLogFile);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ")
       << msg << '\n';
    ts.flush();
}

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

    // disable error messages
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

    // --- Command line: --screen=<index-or-name> ---
    QCommandLineParser parser;
    parser.setApplicationDescription("CameraWall");
    parser.addHelpOption();

    QCommandLineOption optScreen(QStringList() << "screen",
                                 "Screen index (0..N-1) or name substring.",
                                 "index-or-name");
    parser.addOption(optScreen);

    QCommandLineOption debugOpt(QStringList() << "d" << "debug",
                                "Enable file logging to CameraWall.log");
    parser.addOption(debugOpt);

    parser.process(app);

    if (parser.isSet(debugOpt))
    {
        qInstallMessageHandler(fileLogHandler);
        qDebug() << "[main] File logging enabled at"
                 << (QCoreApplication::applicationDirPath() + "/CameraWall.log");
    }

    // Kijelző kiválasztása, ha megadták
    QScreen *targetScreen = nullptr;
    if (parser.isSet(optScreen))
    {
        const QString value = parser.value(optScreen).trimmed();
        const auto screens = QGuiApplication::screens();

        bool ok = false;
        int idx = value.toInt(&ok);
        if (ok)
        {
            if (idx >= 0 && idx < screens.size())
                targetScreen = screens.at(idx);
        }
        else
        {
            for (QScreen *s : screens)
            {
                if (s->name().contains(value, Qt::CaseInsensitive))
                {
                    targetScreen = s;
                    break;
                }
            }
        }
    }

    CameraWall w;

    // A konstruktorod már fullscreenre teheti az ablakot.
    // Ilyenkor gondoskodunk a native windowról, majd átvisszük a kívánt képernyőre.
    if (targetScreen)
    {
        if (!w.windowHandle())
            w.createWinId(); // biztosítja a native ablakot

        if (w.windowHandle())
            w.windowHandle()->setScreen(targetScreen);

        // Ha nem fullscreenez a konstruktor, ez jól pozicionál:
        // (ha igen, akkor is átteszi a megfelelő képernyőre)
        w.setGeometry(targetScreen->availableGeometry());
    }

    w.show(); // a konstruktora már full screenre teszi, de ez itt ártalmatlan
    return app.exec();
}
