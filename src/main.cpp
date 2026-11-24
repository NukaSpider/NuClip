#include "app/SelfTest.h"
#include "core/Log.h"
#include "core/Settings.h"
#include "core/Theme.h"
#include "editor/EditorSession.h"
#include "export/Exporter.h"
#include "library/ClipFilterModel.h"
#include "library/ClipRepository.h"
#include "platform/DeviceCatalog.h"
#include "platform/DiscordPresence.h"
#include "platform/GameDetector.h"
#include "platform/HotkeyManager.h"
#include "platform/OverlayService.h"
#include "platform/TrayController.h"
#include "platform/WinUtil.h"
#include "recording/RecordingEngine.h"
#include "storage/StorageMonitor.h"
#include "update/UpdateChecker.h"

#include <QApplication>
#include <QIcon>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QWindow>

#ifndef NUCLIP_VERSION
#define NUCLIP_VERSION "0.1.0"
#endif

int main(int argc, char *argv[])
{
    nuclip::WinUtil::attachParentConsole();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NuClip"));
    QApplication::setOrganizationName(QStringLiteral("NuClip"));
    QApplication::setApplicationVersion(QStringLiteral(NUCLIP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/resources/nuclip.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    nuclip::Log::init();

    if (app.arguments().contains(QStringLiteral("--self-test")))
        return nuclip::runSelfTest(app);

    QLocalSocket ping;
    ping.connectToServer(QStringLiteral("NuClipInstance"));
    if (ping.waitForConnected(150)) {
        ping.write("show");
        ping.waitForBytesWritten(200);
        return 0;
    }
    QLocalServer::removeServer(QStringLiteral("NuClipInstance"));
    QLocalServer instance;
    if (!instance.listen(QStringLiteral("NuClipInstance")))
        nuclip::Log::warn(QStringLiteral("Single-instance server was not created."));

    nuclip::Settings settings;
    settings.load();
    nuclip::WinUtil::setLaunchOnStartup(settings.launchOnStartup());

    nuclip::Theme theme(&settings);
    nuclip::DeviceCatalog devices;
    devices.refresh();
    devices.startProbe();

    nuclip::GameDetector detector(&devices, &settings);
    QTimer detectorTimer;
    QObject::connect(&detectorTimer, &QTimer::timeout, &detector, &nuclip::GameDetector::poll);
    detectorTimer.start(1000);
    detector.poll();

    nuclip::ClipRepository library(&settings);
    nuclip::ClipFilterModel clipFilter;
    clipFilter.watch(&library);
    nuclip::ClipFilterModel recentClips;
    recentClips.watch(&library);

    nuclip::RecordingEngine recording(&settings, &devices, &detector, &library);
    nuclip::Exporter exporter(&settings, &devices, &library);
    nuclip::EditorSession editor(&settings, &library, &exporter);
    nuclip::StorageMonitor storage(&settings);
    nuclip::HotkeyManager hotkeys(&settings);
    nuclip::TrayController tray(&settings, &recording);

    QObject::connect(&hotkeys, &nuclip::HotkeyManager::triggered, &recording, [&](const QString &action) {
        if (action == QLatin1String("clip"))
            recording.saveClip(0);
        else if (action == QLatin1String("clip15"))
            recording.saveClip(15);
        else if (action == QLatin1String("clip30"))
            recording.saveClip(30);
        else if (action == QLatin1String("clip60"))
            recording.saveClip(60);
        else if (action == QLatin1String("clip120"))
            recording.saveClip(120);
        else if (action == QLatin1String("screenshot"))
            recording.screenshot();
        else if (action == QLatin1String("toggle"))
            recording.toggle();
        else if (action == QLatin1String("pause"))
            recording.togglePause();
    });

    library.rescan();
    storage.refresh();
    QObject::connect(&recording, &nuclip::RecordingEngine::clipSaved, &storage, [&storage](const QString &) {
        storage.refresh();
    });
    QTimer::singleShot(800, &recording, [&recording] { recording.startup(); });

    QQmlApplicationEngine engine;
    auto *context = engine.rootContext();
    nuclip::DiscordPresence discord(&settings, &detector, &recording, &editor, &exporter);
    nuclip::OverlayService overlay(&settings, &theme, &devices, &detector, &recording);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &discord, &nuclip::DiscordPresence::shutdown);
    context->setContextProperty(QStringLiteral("settings"), &settings);
    context->setContextProperty(QStringLiteral("theme"), &theme);
    context->setContextProperty(QStringLiteral("devices"), &devices);
    context->setContextProperty(QStringLiteral("detector"), &detector);
    context->setContextProperty(QStringLiteral("library"), &library);
    context->setContextProperty(QStringLiteral("clipFilter"), &clipFilter);
    context->setContextProperty(QStringLiteral("recentClips"), &recentClips);
    context->setContextProperty(QStringLiteral("recording"), &recording);
    context->setContextProperty(QStringLiteral("editor"), &editor);
    context->setContextProperty(QStringLiteral("exporter"), &exporter);
    context->setContextProperty(QStringLiteral("storage"), &storage);
    context->setContextProperty(QStringLiteral("hotkeys"), &hotkeys);

    context->setContextProperty(QStringLiteral("tray"), &tray);
    QWindow *window = nullptr;
    context->setContextProperty(QStringLiteral("discord"), &discord);
    nuclip::UpdateChecker updates;
    context->setContextProperty(QStringLiteral("updates"), &updates);
    QObject::connect(&updates, &nuclip::UpdateChecker::applyRequested, &app, [&](const QString &version) {
        QStringList arguments{QStringLiteral("--update"), QStringLiteral("--feed"), updates.feedUrl()};
        if (!version.isEmpty())
            arguments << QStringLiteral("--version") << version;
        QProcess::startDetached(updates.setupPath(), arguments);
        tray.quit();
    });
    QTimer::singleShot(0, &discord, [&discord] { discord.startup(); });
    QTimer::singleShot(1500, &updates, [&updates] { updates.check(); });
    QObject::connect(&instance, &QLocalServer::newConnection, &app, [&] {
        while (QLocalSocket *socket = instance.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, &tray, &window] {
                const QByteArray data = socket->readAll();
                if (data.contains("quit"))
                    tray.quit();
                else if (window) {
                    window->show();
                    window->raise();
                    window->requestActivate();
                }
                socket->disconnectFromServer();
                socket->deleteLater();
            });
        }
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [&](QObject *object, const QUrl &) {
                         window = qobject_cast<QWindow *>(object);
                         if (!window) {
                             QCoreApplication::exit(1);
                             return;
                         }
                         hotkeys.setWindow(window);
                         tray.attach(window);
                         const auto styleTitle = [&] {
                             nuclip::WinUtil::setTitleBarColors(window, theme.caption(), theme.text(), theme.cardBorder(),
                                                                theme.dark());
                         };
                         styleTitle();
                         QObject::connect(&theme, &nuclip::Theme::changed, window, styleTitle);
                         QObject::connect(&tray, &nuclip::TrayController::showRequested, window, [window] {
                             window->show();
                             window->raise();
                             window->requestActivate();
                         });
                     });

    engine.loadFromModule(QStringLiteral("NuClip"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
