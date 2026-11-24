#pragma once

#include <QSortFilterProxyModel>

namespace nuclip {

class ClipRepository;

class ClipFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY criteriaChanged)
    Q_PROPERTY(QString game READ game WRITE setGame NOTIFY criteriaChanged)
    Q_PROPERTY(QString dateRange READ dateRange WRITE setDateRange NOTIFY criteriaChanged)

public:
    explicit ClipFilterModel(QObject *parent = nullptr);

    int count() const;
    QString search() const { return m_search; }
    QString game() const { return m_game; }
    QString dateRange() const { return m_dateRange; }

    void setSearch(const QString &value);
    void setGame(const QString &value);
    void setDateRange(const QString &value);
    Q_INVOKABLE void setSortMode(const QString &mode);
    void watch(ClipRepository *source);

signals:
    void countChanged();
    void criteriaChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    void resort();

    QString m_search;
    QString m_game = QStringLiteral("All games");
    QString m_dateRange = QStringLiteral("all");
    int m_sortRole = 0;
    Qt::SortOrder m_order = Qt::DescendingOrder;
};

} // namespace nuclip
