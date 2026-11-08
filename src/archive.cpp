#include "archive.h"

#include <QNetworkRequest>

Archive::Archive(QNetworkAccessManager *network, StatusProvider status, QObject *parent)
    : QAbstractListModel(parent), m_network(network), m_status(std::move(status))
{
}

int Archive::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_episodes.size();
}

QVariant Archive::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_episodes.size())
        return {};
    const Nts::Episode &e = m_episodes.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case DateRole: return e.date;
    case GenresRole: return e.genres;
    case ThumbnailRole: return e.thumbnail;
    case HasAudioRole: return e.hasAudio;
    case StatusRole: return m_status ? m_status(e) : QString();
    case AliasRole: return e.alias;
    case EpisodeRole: return e.toMap();
    case ProgressRole: return m_progress ? m_progress(e) : -1.0;
    }
    return {};
}

QHash<int, QByteArray> Archive::roleNames() const
{
    return {{NameRole, "name"}, {DateRole, "date"}, {GenresRole, "genres"}, {ThumbnailRole, "thumbnail"},
            {HasAudioRole, "hasAudio"}, {StatusRole, "status"}, {AliasRole, "alias"}, {EpisodeRole, "episode"},
            {ProgressRole, "progress"}};
}

QVariantMap Archive::episode(int row) const
{
    return row >= 0 && row < m_episodes.size() ? m_episodes.at(row).toMap() : QVariantMap();
}

QVariantList Archive::pending() const
{
    QVariantList list;
    for (const auto &e : m_episodes)
        if (e.hasAudio && (!m_status || m_status(e).isEmpty() || m_status(e) == "failed"))
            list << e.toMap();
    return list;
}

int Archive::indexOf(const QString &alias) const
{
    for (int i = 0; i < m_episodes.size(); ++i)
        if (m_episodes.at(i).alias == alias)
            return i;
    return -1;
}

void Archive::refreshStatuses()
{
    if (!m_episodes.isEmpty())
        emit dataChanged(index(0), index(m_episodes.size() - 1), {StatusRole, ProgressRole});
}

void Archive::refreshEpisode(const QString &show, const QString &alias)
{
    for (int i = 0; i < m_episodes.size(); ++i)
        if (m_episodes.at(i).alias == alias && m_episodes.at(i).show == show)
            emit dataChanged(index(i), index(i), {ProgressRole, StatusRole});
}

void Archive::setError(const QString &message)
{
    m_error = message;
    m_loading = false;
    emit loadingChanged();
}

void Archive::load(const QString &show)
{
    // A newer load makes any reply still on its way from the last one moot.
    const int generation = ++m_generation;
    beginResetModel();
    m_episodes.clear();
    m_total = 0;
    m_show = Nts::Show{show, {}, {}, {}, {}};
    endResetModel();
    m_error.clear();
    m_loading = true;
    emit showChanged();
    emit loadingChanged();

    QNetworkRequest request(Nts::showUrl(show));
    request.setHeader(QNetworkRequest::UserAgentHeader, "rewind/0.1");
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, show, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            return setError(status == 404 ? tr("NTS has no show called “%1”.").arg(show)
                                          : tr("Couldn't reach NTS: %1").arg(reply->errorString()));
        }
        m_show = Nts::showFromJson(reply->readAll());
        if (m_show.alias.isEmpty())
            m_show.alias = show;
        if (m_show.name.isEmpty())
            m_show.name = show;
        emit showChanged();
        fetchPage();
    });
}

void Archive::loadMore()
{
    if (!canLoadMore())
        return;
    m_loading = true;
    m_error.clear();
    emit loadingChanged();
    fetchPage();
}

void Archive::fetchPage()
{
    const int generation = m_generation;
    QNetworkRequest request(Nts::episodesUrl(m_show.alias, m_episodes.size(), PageSize));
    request.setHeader(QNetworkRequest::UserAgentHeader, "rewind/0.1");
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (reply->error() != QNetworkReply::NoError)
            return setError(tr("Couldn't load episodes: %1").arg(reply->errorString()));
        int total = 0;
        QList<Nts::Episode> page = Nts::episodesFromJson(reply->readAll(), &total);
        for (auto &e : page) {
            e.showName = m_show.name;
            if (e.show.isEmpty())
                e.show = m_show.alias;
        }
        if (!page.isEmpty()) {
            beginInsertRows({}, m_episodes.size(), m_episodes.size() + page.size() - 1);
            m_episodes += page;
            endInsertRows();
        }
        // An empty page means the count overshot; stop asking.
        m_total = page.isEmpty() ? m_episodes.size() : total;
        m_loading = false;
        emit showChanged();
        emit loadingChanged();
        emit pageLoaded();
    });
}
