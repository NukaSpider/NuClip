#pragma once

#include <QObject>
#include <QString>

namespace nuclip {

bool newerReleaseVersion(const QString &remote, const QString &local);

class UpdateChecker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    bool available() const { return m_available && !m_skipped; }
    QString version() const { return m_version; }
    QString notes() const { return m_notes; }
    QString currentVersion() const;
    QString feedUrl() const { return m_feed; }
    QString setupPath() const;

    Q_INVOKABLE void check();
    Q_INVOKABLE void skip();
    Q_INVOKABLE void apply();

private:
    void fetchFeed(const QString &url, bool allowFallback);

signals:
    void changed();
    void applyRequested();

private:
    bool m_available = false;
    bool m_skipped = false;
    bool m_checking = false;
    bool m_applying = false;
    QString m_version;
    QString m_notes;
    QString m_feed;
};

} // namespace nuclip
