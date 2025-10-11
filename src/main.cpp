#include <QApplication>
#include "camerawall.h"
#include "language.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QScreen>
#include <QWindow>
#include <QIcon>
#include <QLoggingCategory>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

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
        Language::instance().t("app.title", "IP Camera Wall"));

    // --- ini fájl (exe mellett) ---
    const QString iniPath = QCoreApplication::applicationDirPath() + "/CameraWall.ini";
    QSettings settings(iniPath, QSettings::IniFormat);

    // --- Command line: --screen=<index-or-name>, --debug ---
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

    // --- Cél képernyő meghatározása ---
    const auto screens = QGuiApplication::screens();
    QScreen *targetScreen = nullptr;

    auto findScreenByName = [&](const QString &name) -> QScreen *
    {
        for (QScreen *s : screens)
            if (s->name().contains(name, Qt::CaseInsensitive))
                return s;
        return nullptr;
    };

    if (parser.isSet(optScreen))
    {
        // 1) Parancssor elsőbbséget élvez -> resolve + mentés ini-be
        const QString value = parser.value(optScreen).trimmed();
        bool ok = false;
        int idx = value.toInt(&ok);
        if (ok)
        {
            if (idx >= 0 && idx < screens.size())
                targetScreen = screens.at(idx);
        }
        else
        {
            targetScreen = findScreenByName(value);
        }

        if (targetScreen)
        {
            // Mentsük a név + index párost
            settings.setValue("ui/screenName", targetScreen->name());
            settings.setValue("ui/screenIndex", screens.indexOf(targetScreen));
            settings.sync();
            qDebug() << "[main] --screen set -> will use screen:"
                     << targetScreen->name()
                     << "index:" << settings.value("ui/screenIndex").toInt();
        }
        else
        {
            qDebug() << "[main] --screen value could not be resolved:" << value;
        }
    }
    else
    {
        // 2) Nincs parancssor -> olvasd az ini-ből
        const QString savedName = settings.value("ui/screenName").toString();
        const int savedIdx = settings.value("ui/screenIndex", -1).toInt();

        if (!savedName.isEmpty())
        {
            targetScreen = findScreenByName(savedName);
            qDebug() << "[main] ini ui/screenName =" << savedName
                     << " -> resolved:" << (targetScreen ? targetScreen->name() : QString("<none>"));
        }

        if (!targetScreen && savedIdx >= 0 && savedIdx < screens.size())
        {
            targetScreen = screens.at(savedIdx);
            qDebug() << "[main] ini ui/screenIndex =" << savedIdx
                     << " -> fallback resolved:" << (targetScreen ? targetScreen->name() : QString("<none>"));
        }
    }

    // --- Ablak létrehozás + képernyőre helyezés ---
    CameraWall w;

    if (targetScreen)
    {
        if (!w.windowHandle())
            w.createWinId(); // biztosítja a native ablakot

        if (w.windowHandle())
            w.windowHandle()->setScreen(targetScreen);

        // ha a konstruktor még nem tette full screenre, ez akkor is jó kezdő pozíció
        w.setGeometry(targetScreen->availableGeometry());

        qDebug() << "[main] Using screen:" << targetScreen->name()
                 << "geometry:" << targetScreen->availableGeometry();
    }
    else
    {
        qDebug() << "[main] No target screen selected (cmd/ini). Using default screen.";
    }

    w.show(); // a konstruktora már full screenre teheti, ez ártalmatlan
    return app.exec();
}
