#include "update/UpdateChecker.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "dist/FeedConfig.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariantMap>

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

QString UpdateChecker::selectedNotes() const
{
    for (const QVariant &item : m_versions) {
        const QVariantMap release = item.toMap();
        if (release.value(QStringLiteral("id")).toString() == m_selected)
            return release.value(QStringLiteral("notes")).toString();
    }
    return {};
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
        QVariantList versions;
        const auto appendRelease = [&](const QJsonObject &release) {
            const QString releaseVersion = release.value(QStringLiteral("version")).toString().trimmed();
            const QString package = release.value(QStringLiteral("package")).toString().trimmed();
            if (releaseVersion.isEmpty() || package.isEmpty())
                return;
            QString releaseNotes = release.value(QStringLiteral("notes")).toString().trimmed();
            if (releaseNotes.size() > 280)
                releaseNotes.truncate(280);
            QString label = releaseVersion;
            if (versions.isEmpty())
                label += QStringLiteral("    latest");
            versions.append(QVariantMap{
                {QStringLiteral("id"), releaseVersion},
                {QStringLiteral("label"), label},
                {QStringLiteral("notes"), releaseNotes},
            });
        };
        const QJsonArray listed = object.value(QStringLiteral("versions")).toArray();
        for (const QJsonValue &value : listed)
            appendRelease(value.toObject());
        if (versions.isEmpty())
            appendRelease(object);
        m_versions = versions;
        if (!versions.isEmpty() && m_selected.isEmpty())
            m_selected = versions.first().toMap().value(QStringLiteral("id")).toString();
        const QString version = versions.isEmpty()
            ? object.value(QStringLiteral("version")).toString().trimmed()
            : versions.first().toMap().value(QStringLiteral("id")).toString();
        const QString notes = versions.isEmpty()
            ? object.value(QStringLiteral("notes")).toString().trimmed()
            : versions.first().toMap().value(QStringLiteral("notes")).toString();
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

void UpdateChecker::choose(const QString &version)
{
    for (const QVariant &item : m_versions) {
        if (item.toMap().value(QStringLiteral("id")).toString() != version)
            continue;
        m_selected = version;
        emit changed();
        return;
    }
}

void UpdateChecker::apply()
{
    applyVersion(m_version);
}

void UpdateChecker::applyVersion(const QString &version)
{
    if (m_applying || version.isEmpty() || version == currentVersion())
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
    emit applyRequested(version);
}

} // namespace nuclip
