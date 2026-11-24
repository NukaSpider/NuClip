#pragma once

#include <QObject>
#include <QSet>
#include <QVariantList>

class QProcess;

namespace nuclip {

class DeviceCatalog : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY monitorsChanged)
    Q_PROPERTY(QVariantList microphones READ microphones NOTIFY audioChanged)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY audioChanged)
    Q_PROPERTY(QVariantList encoders READ encoders NOTIFY encodersChanged)
    Q_PROPERTY(bool probing READ probing NOTIFY probingChanged)

public:
    explicit DeviceCatalog(QObject *parent = nullptr);

    QVariantList monitors() const { return m_monitors; }
    QVariantList microphones() const { return m_microphones; }
    QVariantList outputs() const { return m_outputs; }
    QVariantList encoders() const;
    bool probing() const { return m_probing; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startProbe();
    Q_INVOKABLE int indexForPoint(int x, int y) const;
    Q_INVOKABLE QString monitorLabel(int index) const;
    Q_INVOKABLE int monitorRefresh(int index) const;
    Q_INVOKABLE bool fpsSupported(int fps, int monitorIndex) const;
    Q_INVOKABLE int supportedFps(int fps, int monitorIndex) const;
    Q_INVOKABLE QString encoderLabel(const QString &id) const;
    Q_INVOKABLE QString resolveEncoder(const QString &requested, const QString &codec = QStringLiteral("h264")) const;
    Q_INVOKABLE bool codecSupported(const QString &encoder, const QString &codec) const;
    Q_INVOKABLE QVariantList codecsFor(const QString &encoder) const;
    bool encoderAvailable(const QString &id) const;
    bool monitorGeometry(int index, int *width, int *height, int *refresh) const;

signals:
    void monitorsChanged();
    void audioChanged();
    void encodersChanged();
    void probingChanged();
    void probeFinished();

private:
    void probeNext();
    void enumerateMonitors();
    void enumerateAudio();
    bool familySupports(const QString &encoder, const QString &codec) const;
    QString vendorEncoder() const;

    QVariantList m_monitors;
    QVariantList m_microphones;
    QVariantList m_outputs;
    bool m_nvenc = false;
    bool m_amf = false;
    bool m_qsv = false;
    bool m_probing = false;
    unsigned m_adapterVendor = 0;
    QSet<QString> m_supported;
    QStringList m_probeQueue;
    QProcess *m_probe = nullptr;
};

} // namespace nuclip
