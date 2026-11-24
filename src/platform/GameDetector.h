#pragma once

#include <QObject>
#include <QVariantMap>

namespace nuclip {

class DeviceCatalog;
class Settings;

class GameDetector : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString exe READ exe NOTIFY changed)
    Q_PROPERTY(QString stableId READ stableId NOTIFY changed)
    Q_PROPERTY(QString icon READ icon NOTIFY changed)
    Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY changed)
    Q_PROPERTY(QString productName READ productName NOTIFY changed)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY changed)
    Q_PROPERTY(bool live READ live NOTIFY changed)
    Q_PROPERTY(int monitorIndex READ monitorIndex NOTIFY changed)

public:
    explicit GameDetector(DeviceCatalog *devices, Settings *settings = nullptr, QObject *parent = nullptr);

    QString name() const;
    QString exe() const { return m_exe; }
    QString stableId() const;
    QString icon() const;
    QString windowTitle() const { return m_windowTitle; }
    QString productName() const { return m_productName; }
    bool fullscreen() const { return m_fullscreen; }
    bool live() const { return m_live; }
    int monitorIndex() const { return m_monitorIndex; }
    bool gameOpen() const { return m_gameOpen; }
    QString gameName() const { return m_gameName; }
    QString gameStableId() const { return m_gameStableId; }
    QVariantMap source() const;

    Q_INVOKABLE void poll();
    Q_INVOKABLE void setCustomName(const QString &name);
    Q_INVOKABLE void clearCustomName();

private:
    bool updatePreferredGame(unsigned long foregroundPid);

signals:
    void changed();

private:
    DeviceCatalog *m_devices = nullptr;
    Settings *m_settings = nullptr;
    QString m_name;
    QString m_exe;
    QString m_stableId;
    QString m_icon;
    QString m_windowTitle;
    QString m_productName;
    QString m_exePath;
    QString m_fileDescription;
    QString m_companyName;
    QString m_windowClass;
    bool m_fullscreen = false;
    bool m_live = false;
    int m_monitorIndex = 0;
    bool m_gameOpen = false;
    unsigned long m_gamePid = 0;
    QString m_gameName;
    QString m_gameStableId;
    QString m_gamePath;
    QString m_gameIcon;
};

} // namespace nuclip
