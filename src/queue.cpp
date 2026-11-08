#include "queue.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

Queue::Queue(QNetworkAccessManager *network, const Tools &tools, QObject *parent)
    : QAbstractListModel(parent), m_network(network), m_tools(tools)
{
}

int Queue::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant Queue::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Download *d = m_items.at(index.row());
    const Nts::Episode &e = d->episode();
    switch (role) {
    case TitleRole: return e.name.isEmpty() ? e.alias : e.name;
    case ShowNameRole: return e.showName.isEmpty() ? e.show : e.showName;
    case DateRole: return e.date;
    case StateRole: return int(d->state());
    case ProgressRole: return d->progress();
    case ErrorRole: return d->error();
    case PathRole: return d->path();
    case SkippedRole: return d->skipped();
    case AliasRole: return e.alias;
    case ThumbnailRole: return e.thumbnail;
    case EpisodeRole: return e.toMap();
    case KeyRole: return e.show + "/" + e.alias;
    }
    return {};
}

QHash<int, QByteArray> Queue::roleNames() const
{
    return {{TitleRole, "title"}, {ShowNameRole, "showName"}, {DateRole, "date"}, {StateRole, "state"},
            {ProgressRole, "progress"}, {ErrorRole, "error"}, {PathRole, "path"}, {SkippedRole, "skipped"},
            {AliasRole, "alias"}, {ThumbnailRole, "thumbnail"},
            {EpisodeRole, "episode"}, {KeyRole, "key"}};
}

int Queue::activeCount() const
{
    int n = 0;
    for (const auto *d : m_items)
        n += !d->isFinished();
    return n;
}

int Queue::finishedCount() const
{
    return m_items.size() - activeCount();
}

QString Queue::statusFor(const QString &show, const QString &alias) const
{
    // The newest entry for an episode wins, so a retry shows as waiting.
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const Download *d = m_items.at(i);
        if (d->episode().alias != alias || d->episode().show != show)
            continue;
        switch (d->state()) {
        case Download::Waiting: return "waiting";
        case Download::Fetching:
        case Download::Downloading:
        case Download::Saving: return "downloading";
        case Download::Done: return "done";
        case Download::Failed: return "failed";
        case Download::Cancelled: return "";
        }
    }
    return {};
}

// Downloading is nearly all the time; fetching details and saving are quick.
static double overall(const Download *d)
{
    const double p = qMax(0.0, d->progress());
    switch (d->state()) {
    case Download::Fetching: return 0.0;
    case Download::Downloading: return p * 0.95;
    case Download::Saving: return 0.95 + p * 0.05;
    default: return -1;
    }
}

double Queue::progressFor(const QString &show, const QString &alias) const
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const Download *d = m_items.at(i);
        if (d->episode().alias == alias && d->episode().show == show)
            return overall(d);
    }
    return -1;
}

double Queue::currentProgress() const
{
    for (const auto *d : m_items)
        if (overall(d) >= 0)
            return overall(d);
    return 0;
}

void Queue::watch(Download *download)
{
    connect(download, &Download::changed, this, [this, download] {
        const int row = m_items.indexOf(download);
        if (row >= 0)
            emit dataChanged(index(row), index(row));
        emit progressed(download->episode().show, download->episode().alias);
    });
    connect(download, &Download::finished, this, [this, download] {
        emit countsChanged();
        emit statusesChanged();
        emit downloadFinished(download);
        startNext();
    });
}

bool Queue::add(const Nts::Episode &episode)
{
    const QString status = statusFor(episode.show, episode.alias);
    if (status == "waiting" || status == "downloading")
        return false;
    auto *download = new Download(episode, m_directory, m_tools, m_network, this);
    watch(download);
    beginInsertRows({}, m_items.size(), m_items.size());
    m_items << download;
    endInsertRows();
    emit countsChanged();
    emit statusesChanged();
    startNext();
    return true;
}

bool Queue::enqueue(const QVariantMap &episode)
{
    return add(Nts::Episode::fromMap(episode));
}

void Queue::startNext()
{
    for (auto *d : m_items)
        if (!d->isFinished() && d->state() != Download::Waiting)
            return;
    for (auto *d : m_items) {
        if (d->state() == Download::Waiting) {
            d->start();
            emit statusesChanged();
            return;
        }
    }
}

void Queue::cancel(int row)
{
    if (row >= 0 && row < m_items.size())
        m_items.at(row)->cancel();
}

void Queue::cancelCurrent()
{
    for (auto *d : m_items)
        if (!d->isFinished() && d->state() != Download::Waiting)
            return d->cancel();
}

void Queue::cancelEpisode(const QString &show, const QString &alias)
{
    for (auto *d : m_items)
        if (!d->isFinished() && d->episode().alias == alias && d->episode().show == show)
            d->cancel();
}

QString Queue::currentTitle() const
{
    for (const auto *d : m_items)
        if (!d->isFinished() && d->state() != Download::Waiting)
            return d->episode().name.isEmpty() ? d->episode().alias : d->episode().name;
    return {};
}

void Queue::cancelAll()
{
    // Waiting ones first, so cancelling the running one doesn't start the next.
    for (auto *d : m_items)
        if (d->state() == Download::Waiting)
            d->cancel();
    cancelCurrent();
}

void Queue::retry(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    const Download *d = m_items.at(row);
    if (d->state() == Download::Failed || d->state() == Download::Cancelled)
        add(d->episode());
}

void Queue::retryFailed()
{
    const auto items = m_items;
    for (const auto *d : items)
        if (d->state() == Download::Failed && statusFor(d->episode().show, d->episode().alias) == "failed")
            add(d->episode());
}

void Queue::clearFinished()
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!m_items.at(i)->isFinished())
            continue;
        beginRemoveRows({}, i, i);
        m_items.takeAt(i)->deleteLater();
        endRemoveRows();
    }
    emit countsChanged();
    emit statusesChanged();
}

void Queue::reveal(int row) const
{
    if (row < 0 || row >= m_items.size())
        return;
    const QString path = m_items.at(row)->path();
    if (!path.isEmpty() && QFileInfo::exists(QFileInfo(path).absolutePath()))
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}
