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
    }
    return {};
}

QHash<int, QByteArray> Queue::roleNames() const
{
    return {{TitleRole, "title"}, {ShowNameRole, "showName"}, {DateRole, "date"}, {StateRole, "state"},
            {ProgressRole, "progress"}, {ErrorRole, "error"}, {PathRole, "path"}, {SkippedRole, "skipped"},
            {AliasRole, "alias"}};
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
        case Download::Converting: return "downloading";
        case Download::Done: return "done";
        case Download::Failed: return "failed";
        case Download::Cancelled: return "";
        }
    }
    return {};
}

void Queue::watch(Download *download)
{
    connect(download, &Download::changed, this, [this, download] {
        const int row = m_items.indexOf(download);
        if (row >= 0)
            emit dataChanged(index(row), index(row));
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
