#include "app/SelfTest.h"

#include "core/Log.h"
#include "core/Settings.h"
#include "library/ClipRepository.h"
#include "platform/AppCatalog.h"
#include "platform/DiscordPresence.h"
#include "platform/DeviceCatalog.h"
#include "platform/GameDetector.h"
#include "recording/FfmpegLocator.h"
#include "recording/RecordingEngine.h"
#include "update/UpdateChecker.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

namespace nuclip {

int runSelfTest(QCoreApplication &app)
{
    QFile resultFile(QCoreApplication::applicationDirPath() + QStringLiteral("/self-test.txt"));
    resultFile.remove();
    const auto report = [&](const QString &line) {
        qInfo().noquote() << line;
        if (resultFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            resultFile.write(line.toUtf8());
            resultFile.write("\n");
            resultFile.close();
        }
    };

    if (!newerReleaseVersion(QStringLiteral("0.2.0"), QStringLiteral("0.1.0"))
        || newerReleaseVersion(QStringLiteral("0.1.0"), QStringLiteral("0.1.0"))
        || newerReleaseVersion(QStringLiteral("0.1.0"), QStringLiteral("0.2.0"))
        || !newerReleaseVersion(QStringLiteral("1.0"), QStringLiteral("0.9.9"))) {
        report(QStringLiteral("FAIL version compare"));
        return 1;
    }

    QTemporaryDir temp;
    if (!temp.isValid()) {
        report(QStringLiteral("FAIL could not create a temporary directory"));
        return 1;
    }

    const struct Sample {
        const char *exe;
        const char *title;
        const char *product;
        const char *expect;
    } samples[] = {
        {"RobloxPlayerBeta.exe", "Roblox Game Client", "Roblox", "Roblox"},
        {"VALORANT-Win64-Shipping.exe", "", "", "VALORANT"},
        {"FortniteClient-Win64-Shipping.exe", "", "", "Fortnite"},
        {"javaw.exe", "Minecraft 1.21.1", "Java(TM) Platform SE binary", "Minecraft"},
        {"javaw.exe", "My Utility", "Java(TM) Platform SE binary", "My Utility"},
        {"cs2.exe", "", "", "Counter-Strike 2"},
        {"r5apex.exe", "", "", "Apex Legends"},
        {"DayZ_x64.exe", "", "", "DayZ"},
        {"SomeUnknown-Win64-Shipping.exe", "", "", "SomeUnknown"},
    };
    for (const Sample &sample : samples) {
        AppFacts facts;
        facts.exeName = QString::fromLatin1(sample.exe);
        facts.windowTitle = QString::fromLatin1(sample.title);
        facts.productName = QString::fromLatin1(sample.product);
        const AppMatch match = AppCatalog::resolve(facts);
        if (match.displayName != QLatin1String(sample.expect)) {
            report(QStringLiteral("FAIL name %1 -> %2, expected %3").arg(sample.exe, match.displayName, QString::fromLatin1(sample.expect)));
            return 1;
        }
    }
    if (AppCatalog::displayForStored(QStringLiteral("Roblox Game Client")) != QLatin1String("Roblox")
        || AppCatalog::displayForStored(QStringLiteral("FortniteClient-Win64-Shipping")) != QLatin1String("Fortnite")
        || AppCatalog::fileToken(QStringLiteral("Apex Legends")) != QLatin1String("Apex_Legends")) {
        report(QStringLiteral("FAIL stored application names"));
        return 1;
    }
    report(QStringLiteral("PASS application names"));

    Settings settings;
    settings.load();
    settings.setPersistEnabled(false);
    settings.setAppDisplayName(QStringLiteral("app:roblox"), QStringLiteral("My Roblox"));
    if (settings.appDisplayName(QStringLiteral("app:roblox")) != QLatin1String("My Roblox")) {
    AppFacts roblox;
    roblox.exeName = QStringLiteral("RobloxPlayerBeta.exe");
    AppFacts discord;
    discord.exeName = QStringLiteral("Discord.exe");
    if (!AppCatalog::isGame(AppCatalog::resolve(roblox).stableId) || AppCatalog::isGame(AppCatalog::resolve(discord).stableId)) {
        report(QStringLiteral("FAIL game priority"));
        return 1;
    }
        report(QStringLiteral("FAIL custom application name"));
        return 1;
    }
    settings.setAppDisplayName(QStringLiteral("app:roblox"), QString());
    settings.setSaveDirectory(temp.path());
    settings.setSystemAudio(true);
    settings.setMicEnabled(false);
    settings.setQuality(QStringLiteral("custom"));
    settings.setWidth(1280);
    settings.setHeight(720);
    settings.setFps(30);
    settings.setVideoBitrateKbps(6000);

    DeviceCatalog devices;
    devices.refresh();
    devices.startProbe();
    GameDetector detector(&devices);
    ClipRepository library(&settings);
    RecordingEngine engine(&settings, &devices, &detector, &library);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(45000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();

    if (devices.probing()) {
        QObject::connect(&devices, &DeviceCatalog::probeFinished, &loop, &QEventLoop::quit);
        loop.exec();
        QObject::disconnect(&devices, &DeviceCatalog::probeFinished, &loop, &QEventLoop::quit);
    }
    report(QStringLiteral("encoder %1").arg(devices.resolveEncoder(QStringLiteral("auto"))));
    if (!timeout.isActive()) {
        report(QStringLiteral("FAIL encoder probe timed out"));
        return 1;
    }

    QString savedPath;
    QObject::connect(&engine, &RecordingEngine::clipSaved, &loop, [&](const QString &path) {
        savedPath = path;
        loop.quit();
    });
    QObject::connect(&engine, &RecordingEngine::errorChanged, &loop, [&] {
        if (engine.state() == RecordingEngine::Error)
            loop.quit();
    });

    engine.startReplay();
    QTimer poll;
    bool asked = false;
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (!asked && engine.state() == RecordingEngine::Recording) {
            asked = true;
            QTimer::singleShot(3000, &app, [&] { engine.saveClip(3); });
        }
    });
    poll.start(200);
    loop.exec();
    poll.stop();
    engine.shutdown();

    if (savedPath.isEmpty() || !QFileInfo::exists(savedPath)) {
        report(QStringLiteral("FAIL no clip was saved"));
        report(engine.error());
        report(engine.details().right(600));
        return 1;
    }
    const qint64 size = QFileInfo(savedPath).size();
    report(QStringLiteral("clip %1").arg(savedPath));
    report(QStringLiteral("size %1").arg(size));

    QProcess probe;
    probe.start(ffprobeExecutable(), {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"),
                                      QStringLiteral("v:0"), QStringLiteral("-show_entries"),
                                      QStringLiteral("stream=codec_name,width,height"), QStringLiteral("-of"),
                                      QStringLiteral("default=nw=1"), savedPath});
    probe.waitForFinished(10000);
    const QString probeText = QString::fromLocal8Bit(probe.readAllStandardOutput());
    report(probeText.trimmed());
    const bool pass = size > 50000 && probeText.contains(QStringLiteral("codec_name=h264"));
    report(pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
    return pass ? 0 : 1;
}

} // namespace nuclip
