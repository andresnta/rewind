#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QUrl>

#include "archive.h"
#include "queue.h"

#include <memory>

// What the window talks to: the show being browsed, the download queue,
// the player, and where episodes are saved.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(Archive *archive READ archive CONSTANT)
    Q_PROPERTY(Queue *queue READ queue CONSTANT)
    Q_PROPERTY(QString directory READ directory WRITE setDirectory NOTIFY directoryChanged)
    Q_PROPERTY(QString displayDirectory READ displayDirectory NOTIFY directoryChanged)
    Q_PROPERTY(QStringList missingTools READ missingTools NOTIFY toolsChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(bool tracksTimed READ tracksTimed NOTIFY tracksChanged)

public:
    // An empty directory means the saved setting, or ~/Music/NTS.
    explicit Backend(const QString &directory = {}, const Tools &tools = Tools::find(), QObject *parent = nullptr);

    Archive *archive() { return &m_archive; }
    Queue *queue() { return &m_queue; }
    QString directory() const { return m_directory; }
    void setDirectory(const QString &directory);
    QString displayDirectory() const;
    QStringList missingTools() const { return m_tools.missing(); }
    QString message() const { return m_message; }

    static QString defaultDirectory();

    // Opens a show's archive, and queues the episode too when the link is
    // one. Returns false, with a message, when the text isn't an NTS link.
    Q_INVOKABLE bool open(const QString &text);
    Q_INVOKABLE void openFolder() const;
    Q_INVOKABLE void checkTools();
    Q_INVOKABLE void clearMessage();

    // The episode's audio on disk, in whatever format it was saved, or "".
    Q_INVOKABLE QString savedPath(const QVariantMap &episode) const;
    Q_INVOKABLE QUrl fileUrl(const QString &path) const;
    // Asks yt-dlp where an episode that isn't saved can be streamed from;
    // answers with streamReady or streamFailed, tagged with the episode key.
    // A newer request replaces an older one still running.
    Q_INVOKABLE void resolveStream(const QVariantMap &episode);
    // "<show>/<episode>": playback positions and tracks follow the episode,
    // whether it's streamed or played from disk.
    Q_INVOKABLE QString episodeKey(const QVariantMap &episode) const;
    // Where playback of an episode stopped, so a two-hour show picks up there.
    Q_INVOKABLE qint64 resumePosition(const QString &key) const;
    Q_INVOKABLE void savePosition(const QString &key, qint64 milliseconds);
    Q_INVOKABLE void forgetPosition(const QString &key);
    // Loads the playing episode's tracklist from NTS, or from the text file
    // beside a saved episode when NTS can't be reached.
    Q_INVOKABLE void loadTracks(const QVariantMap &episode, const QString &path);
    Q_INVOKABLE void clearTracks();
    QVariantList tracks() const { return m_tracks; }
    bool tracksTimed() const { return m_tracksTimed; }

signals:
    void directoryChanged();
    void toolsChanged();
    void messageChanged();
    void tracksChanged();
    void streamReady(const QString &key, const QUrl &url);
    void streamFailed(const QString &key, const QString &error);

private:
    void setMessage(const QString &message);
    QString status(const Nts::Episode &episode) const;

    QNetworkAccessManager m_network;
    Tools m_tools;
    QString m_directory;
    Queue m_queue;
    Archive m_archive;
    QString m_message;
    bool m_persist;
    // Positions live apart from the folder setting, and a test run can
    // point them at a temporary file.
    std::unique_ptr<QSettings> m_positions;
    QPointer<QProcess> m_resolver;
    QVariantList m_tracks;
    bool m_tracksTimed = false;
    int m_tracksGeneration = 0;
    void setTracks(const QList<Nts::Track> &tracks);
};
