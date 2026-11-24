#pragma once

#include <QObject>

namespace nuclip {

class Settings;

class StorageMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString usedText READ usedText NOTIFY changed)
    Q_PROPERTY(QString freeText READ freeText NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(qreal usedRatio READ usedRatio NOTIFY changed)

public:
    explicit StorageMonitor(Settings *settings, QObject *parent = nullptr);

    QString usedText() const { return m_usedText; }
    QString freeText() const { return m_freeText; }
    QString path() const { return m_path; }
    qreal usedRatio() const { return m_ratio; }

    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    Settings *m_settings = nullptr;
    QString m_usedText = QStringLiteral("—");
    QString m_freeText = QStringLiteral("—");
    QString m_path;
    qreal m_ratio = 0;
    int m_generation = 0;
};

} // namespace nuclip
