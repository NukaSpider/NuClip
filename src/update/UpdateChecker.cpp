#include "update/UpdateChecker.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "dist/FeedConfig.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#ifndef NUCLIP_VERSION
#define NUCLIP_VERSION "0.1.0"
#endif

namespace nuclip {
namespace {

QList<int> versionParts(const QString &text)
{
    QList<int> parts;
    const QStringList tokens = text.trimmed().split(QLatin1Char('.'), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        bool ok = false;
        const int value = token.toInt(&ok);
        parts.append(ok ? value : 0);
    }
    return parts;
}

} // namespace

bool newerReleaseVersion(const QString &remote, const QString &local)
{
    const QList<int> left = versionParts(remote);
    const QList<int> right = versionParts(local);
    const int count = qMax(left.size(), right.size());
    for (int i = 0; i < count; ++i) {
        const int a = i < left.size() ? left.at(i) : 0;
        const int b = i < right.size() ? right.at(i) : 0;
        if (a != b)
            return a > b;
    }
    return false;
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
    const QString overrideUrl = qEnvironmentVariable("NUCLIP_FEED_URL");
    m_feed = overrideUrl.isEmpty() ? QString::fromLatin1(NUCLIP_FEED_URL) : overrideUrl;
}

QString UpdateChecker::currentVersion() const
{
    return QString::fromLatin1(NUCLIP_VERSION);
}

QString UpdateChecker::setupPath() const
{
    const QString name = QStringLiteral("NuClipSetup.exe");
    const QString stored = QDir(Paths::dataDir()).filePath(name);
    const QString beside = QDir(QCoreApplication::applicationDirPath()).filePath(name);
    const QFileInfo storedInfo(stored);
    const QFileInfo besideInfo(beside);
    if (storedInfo.exists() && besideInfo.exists())
        return besideInfo.lastModified() > storedInfo.lastModified() ? beside : stored;
    if (storedInfo.exists())
        return stored;
    if (besideInfo.exists())
        return beside;
    return {};
}

void UpdateChecker::check()
{
    if (m_checking || m_skipped)
        return;
    const bool allowFallback = m_feed == QLatin1String(NUCLIP_FEED_URL);
    fetchFeed(m_feed, allowFallback);
}

void UpdateChecker::fetchFeed(const QString &url, bool allowFallback)
{
    m_checking = true;
    auto *network = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NuClip/") + currentVersion());
    request.setTransferTimeout(12000);
    QNetworkReply *reply = network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, allowFallback] {
        m_checking = false;
        const auto finish = [&] {
            reply->deleteLater();
            emit changed();
        };
        if (reply->error() != QNetworkReply::NoError) {
            if (allowFallback && !m_skipped) {
                reply->deleteLater();
                Log::info(QStringLiteral("Primary release feed did not answer. Trying the fallback."));
                fetchFeed(QString::fromLatin1(NUCLIP_FALLBACK_FEED_URL), false);
                return;
            }
            Log::info(QStringLiteral("Update check did not reach the release feed."));
            finish();
            return;
        }
        m_feed = url;
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        const QString version = object.value(QStringLiteral("version")).toString().trimmed();
        QString notes = object.value(QStringLiteral("notes")).toString().trimmed();
        if (notes.size() > 280)
            notes.truncate(280);
        if (version.isEmpty() || !newerReleaseVersion(version, currentVersion())) {
            m_available = false;
            finish();
            return;
        }
        m_version = version;
        m_notes = notes;
        m_available = true;
        Log::info(QStringLiteral("Update available: %1").arg(version));
        finish();
    });
}

void UpdateChecker::skip()
{
    m_skipped = true;
    emit changed();
}

void UpdateChecker::apply()
{
    if (m_applying)
        return;
    if (setupPath().isEmpty()) {
        const bool fallback = m_feed.startsWith(QLatin1String(NUCLIP_FALLBACK_ORIGIN));
        const char *page = fallback ? NUCLIP_FALLBACK_DOWNLOAD_PAGE : NUCLIP_DOWNLOAD_PAGE;
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(page)));
        return;
    }
    m_applying = true;
    m_available = false;
    m_skipped = true;
    emit changed();
    emit applyRequested();
}

} // namespace nuclip
