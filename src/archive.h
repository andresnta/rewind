#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <functional>

#include "nts.h"

// One show's past episodes, newest first, a page at a time from the NTS API.
class Archive : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString showAlias READ showAlias NOTIFY showChanged)
    Q_PROPERTY(QString showName READ showName NOTIFY showChanged)
    Q_PROPERTY(QString location READ location NOTIFY showChanged)
    Q_PROPERTY(QString thumbnail READ thumbnail NOTIFY showChanged)
    Q_PROPERTY(int total READ total NOTIFY showChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY loadingChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, DateRole, GenresRole, ThumbnailRole, HasAudioRole, StatusRole,
                AliasRole, EpisodeRole, ProgressRole };

    // The status of an episode as the queue and the disk see it.
    using StatusProvider = std::function<QString(const Nts::Episode &)>;
    // How far its download is, 0..1, or -1 when none is running.
    using ProgressProvider = std::function<double(const Nts::Episode &)>;

    Archive(QNetworkAccessManager *network, StatusProvider status, QObject *parent = nullptr);
    void setProgressProvider(ProgressProvider progress) { m_progress = std::move(progress); }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString showAlias() const { return m_show.alias; }
    QString showName() const { return m_show.name; }
    QString location() const { return m_show.location; }
    QString thumbnail() const { return m_show.thumbnail; }
    int total() const { return m_total; }
    bool loading() const { return m_loading; }
    bool canLoadMore() const { return !m_show.alias.isEmpty() && !m_loading && m_episodes.size() < m_total; }
    QString error() const { return m_error; }

    void load(const QString &show);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE QVariantMap episode(int row) const;
    // Episodes in the list still worth downloading: they have audio and
    // aren't saved, waiting, or running.
    Q_INVOKABLE QVariantList pending() const;
    Q_INVOKABLE int indexOf(const QString &alias) const;
    void refreshStatuses();
    void refreshEpisode(const QString &show, const QString &alias);

    static constexpr int PageSize = 24;

signals:
    void showChanged();
    void loadingChanged();
    void pageLoaded();

private:
    void fetchPage();
    void setError(const QString &message);

    QNetworkAccessManager *m_network;
    StatusProvider m_status;
    ProgressProvider m_progress;
    Nts::Show m_show;
    QList<Nts::Episode> m_episodes;
    int m_total = 0;
    bool m_loading = false;
    QString m_error;
    int m_generation = 0;
};
