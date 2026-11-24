#include "library/ClipFilterModel.h"

#include "library/ClipRepository.h"

#include <QDate>

namespace nuclip {

ClipFilterModel::ClipFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    m_sortRole = ClipRepository::DateMsRole;
    sort(0, Qt::DescendingOrder);
}

int ClipFilterModel::count() const
{
    return rowCount();
}

void ClipFilterModel::watch(ClipRepository *source)
{
    setSourceModel(source);
    const auto bump = [this] { emit countChanged(); };
    connect(source, &QAbstractItemModel::modelReset, this, bump);
    connect(source, &QAbstractItemModel::rowsInserted, this, bump);
    connect(source, &QAbstractItemModel::rowsRemoved, this, bump);
    connect(this, &QAbstractItemModel::layoutChanged, this, bump);
    setSortMode(QStringLiteral("newest"));
}

void ClipFilterModel::setSearch(const QString &value)
{
    if (m_search == value)
        return;
    m_search = value;
    invalidateFilter();
    emit criteriaChanged();
    emit countChanged();
}

void ClipFilterModel::setGame(const QString &value)
{
    const QString next = value.isEmpty() ? QStringLiteral("All games") : value;
    if (m_game == next)
        return;
    m_game = next;
    invalidateFilter();
    emit criteriaChanged();
    emit countChanged();
}

void ClipFilterModel::setDateRange(const QString &value)
{
    if (m_dateRange == value)
        return;
    m_dateRange = value;
    invalidateFilter();
    emit criteriaChanged();
    emit countChanged();
}

void ClipFilterModel::setSortMode(const QString &mode)
{
    m_order = Qt::DescendingOrder;
    m_sortRole = ClipRepository::DateMsRole;
    if (mode == QLatin1String("oldest")) {
        m_order = Qt::AscendingOrder;
        m_sortRole = ClipRepository::DateMsRole;
    } else if (mode == QLatin1String("name")) {
        m_order = Qt::AscendingOrder;
        m_sortRole = ClipRepository::NameRole;
    } else if (mode == QLatin1String("largest")) {
        m_sortRole = ClipRepository::SizeRole;
    } else if (mode == QLatin1String("longest")) {
        m_sortRole = ClipRepository::DurationRole;
    } else if (mode == QLatin1String("game")) {
        m_order = Qt::AscendingOrder;
        m_sortRole = ClipRepository::GameRole;
    }
    resort();
}

void ClipFilterModel::resort()
{
    sort(0, m_order);
    emit countChanged();
}

bool ClipFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString name = sourceModel()->data(index, ClipRepository::NameRole).toString();
    const QString game = sourceModel()->data(index, ClipRepository::GameRole).toString();
    if (!m_search.trimmed().isEmpty()) {
        const QString needle = m_search.trimmed();
        if (!name.contains(needle, Qt::CaseInsensitive) && !game.contains(needle, Qt::CaseInsensitive))
            return false;
    }
    if (m_game != QLatin1String("All games") && game != m_game)
        return false;
    if (m_dateRange != QLatin1String("all")) {
        const QDate date = QDateTime::fromMSecsSinceEpoch(
                               sourceModel()->data(index, ClipRepository::DateMsRole).toLongLong())
                               .date();
        const QDate today = QDate::currentDate();
        if (m_dateRange == QLatin1String("today") && date != today)
            return false;
        if (m_dateRange == QLatin1String("7") && date < today.addDays(-6))
            return false;
        if (m_dateRange == QLatin1String("30") && date < today.addDays(-29))
            return false;
    }
    return true;
}

bool ClipFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const QVariant a = sourceModel()->data(left, m_sortRole);
    const QVariant b = sourceModel()->data(right, m_sortRole);
    if (m_sortRole == ClipRepository::NameRole || m_sortRole == ClipRepository::GameRole)
        return a.toString().localeAwareCompare(b.toString()) < 0;
    return a.toDouble() < b.toDouble();
}

} // namespace nuclip
