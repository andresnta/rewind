#pragma once

#include <QAbstractListModel>
#include <QList>

#include "download.h"

// Downloads, one at a time in the order they were added: NTS episodes run
// for hours, and running them side by side only makes every one slower.
class Queue : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY countsChanged)
    Q_PROPERTY(int finishedCount READ finishedCount NOTIFY countsChanged)
    Q_PROPERTY(int count READ count NOTIFY countsChanged)

public:
    enum Role { TitleRole = Qt::UserRole + 1, ShowNameRole, DateRole, StateRole, ProgressRole, ErrorRole,
                PathRole, SkippedRole, AliasRole };

    Queue(QNetworkAccessManager *network, const Tools &tools, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_items.size(); }
    int activeCount() const;
    int finishedCount() const;

    void setDirectory(const QString &directory) { m_directory = directory; }
    void setTools(const Tools &tools) { m_tools = tools; }

    // Queues an episode unless it's already waiting or running; returns
    // whether it was added.
    bool add(const Nts::Episode &episode);
    Q_INVOKABLE bool enqueue(const QVariantMap &episode);
    Q_INVOKABLE void cancel(int row);
    Q_INVOKABLE void cancelCurrent();
    Q_INVOKABLE void cancelAll();
    Q_INVOKABLE void retry(int row);
    Q_INVOKABLE void retryFailed();
    Q_INVOKABLE void clearFinished();
    Q_INVOKABLE void reveal(int row) const;

    // "waiting", "downloading", "done", "failed", or "" for an episode this
    // session hasn't touched.
    QString statusFor(const QString &show, const QString &alias) const;
    const QList<Download *> &items() const { return m_items; }

signals:
    void countsChanged();
    void statusesChanged();
    void downloadFinished(Download *download);

private:
    void startNext();
    void watch(Download *download);

    QNetworkAccessManager *m_network;
    Tools m_tools;
    QString m_directory;
    QList<Download *> m_items;
};
