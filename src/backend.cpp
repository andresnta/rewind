#include "backend.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

Backend::Backend(const QString &directory, const Tools &tools, QObject *parent)
    : QObject(parent),
      m_tools(tools),
      m_queue(&m_network, tools),
      m_archive(&m_network, [this](const Nts::Episode &e) { return status(e); }),
      m_persist(directory.isEmpty())
{
    QString saved = directory;
    if (saved.isEmpty())
        saved = QSettings().value("directory").toString();
    m_directory = saved.isEmpty() ? defaultDirectory() : saved;
    m_queue.setDirectory(m_directory);

    connect(&m_queue, &Queue::statusesChanged, &m_archive, &Archive::refreshStatuses);
    connect(&m_archive, &Archive::loadingChanged, this, [this] {
        if (!m_archive.loading() && !m_archive.error().isEmpty() && m_archive.rowCount() == 0)
            setMessage(m_archive.error());
    });

    m_positions = m_persist ? std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, "rewind", "positions")
                            : std::make_unique<QSettings>(m_directory + "/.rewind-positions.ini", QSettings::IniFormat);
}

QString Backend::savedPath(const QVariantMap &episode) const
{
    return Nts::savedFile(m_directory, Nts::Episode::fromMap(episode));
}

QUrl Backend::fileUrl(const QString &path) const
{
    return QUrl::fromLocalFile(path);
}

QString Backend::episodeKey(const QVariantMap &episode) const
{
    return episode.value("show").toString() + "/" + episode.value("alias").toString();
}

void Backend::resolveStream(const QVariantMap &map)
{
    const Nts::Episode episode = Nts::Episode::fromMap(map);
    const QString key = episodeKey(map);
    if (m_resolver) {
        m_resolver->disconnect(this);
        m_resolver->kill();
        m_resolver->deleteLater();
    }
    if (m_tools.ytdlp.isEmpty()) {
        emit streamFailed(key, tr("%1 isn't installed.").arg("yt-dlp"));
        return;
    }
    if (!episode.hasAudio) {
        emit streamFailed(key, tr("NTS has no audio for this episode."));
        return;
    }

    auto *process = new QProcess(this);
    m_resolver = process;
    // The same format a download picks, so streaming sounds the same.
    const QStringList args{"--ignore-config", "--no-playlist", "--no-warnings",
                           "-f", "bestaudio[ext=m4a]/bestaudio/best", "-g", episode.pageUrl()};
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::finished, this, [this, process, key](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (m_resolver == process)
            m_resolver = nullptr;
        const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        const QUrl url(out.section('\n', 0, 0).trimmed());
        if (status == QProcess::NormalExit && code == 0 && url.isValid() && !url.scheme().isEmpty()) {
            emit streamReady(key, url);
            return;
        }
        QString reason;
        for (const auto &line : QString::fromUtf8(process->readAllStandardError()).split('\n'))
            if (line.startsWith("ERROR:"))
                reason = line.mid(6).trimmed();
        emit streamFailed(key, reason.isEmpty() ? tr("yt-dlp couldn't find a stream for this episode.")
                                                : tr("yt-dlp: %1").arg(reason));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, key](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        if (m_resolver == process)
            m_resolver = nullptr;
        emit streamFailed(key, tr("Couldn't start yt-dlp."));
    });
    process->start(m_tools.ytdlp, args);
    timeout->start(60000);
}

void Backend::setTracks(const QList<Nts::Track> &tracks)
{
    m_tracks.clear();
    m_tracksTimed = false;
    for (const auto &track : tracks) {
        m_tracks << track.toMap();
        m_tracksTimed |= track.start >= 0;
    }
    emit tracksChanged();
}

void Backend::clearTracks()
{
    ++m_tracksGeneration;
    setTracks({});
}

void Backend::loadTracks(const QVariantMap &map, const QString &path)
{
    const int generation = ++m_tracksGeneration;
    setTracks({});
    const Nts::Episode episode = Nts::Episode::fromMap(map);
    if (episode.show.isEmpty() || episode.alias.isEmpty())
        return;

    // The text file beside a saved episode, for when NTS is out of reach.
    const QString sidecar = path.isEmpty() ? QString() : QFileInfo(path).path() + "/" + QFileInfo(path).completeBaseName() + ".txt";
    QNetworkRequest request(Nts::tracklistUrl(episode.show, episode.alias));
    request.setHeader(QNetworkRequest::UserAgentHeader, "rewind/0.1");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, sidecar] {
        reply->deleteLater();
        if (generation != m_tracksGeneration)
            return;
        if (reply->error() == QNetworkReply::NoError) {
            setTracks(Nts::tracksFromJson(reply->readAll()));
            return;
        }
        QFile file(sidecar);
        if (!sidecar.isEmpty() && file.open(QIODevice::ReadOnly))
            setTracks(Nts::tracksFromText(QString::fromUtf8(file.readAll())));
    });
}

// Paths are hashed into keys: QSettings reads slashes as groups.
static QString positionKey(const QString &path)
{
    return QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
}

qint64 Backend::resumePosition(const QString &path) const
{
    return path.isEmpty() ? 0 : m_positions->value(positionKey(path), 0).toLongLong();
}

void Backend::savePosition(const QString &path, qint64 milliseconds)
{
    if (path.isEmpty())
        return;
    // The first few seconds aren't worth resuming from.
    if (milliseconds < 5000)
        m_positions->remove(positionKey(path));
    else
        m_positions->setValue(positionKey(path), milliseconds);
}

void Backend::forgetPosition(const QString &path)
{
    if (!path.isEmpty())
        m_positions->remove(positionKey(path));
}

QString Backend::defaultDirectory()
{
    QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (music.isEmpty() || music == QDir::homePath())
        music = QDir::homePath() + "/Music";
    return music + "/NTS";
}

void Backend::setDirectory(const QString &value)
{
    QString directory = value.trimmed();
    if (directory.startsWith("~/") || directory == "~")
        directory = QDir::homePath() + directory.mid(1);
    directory = QDir::cleanPath(directory);
    if (directory.isEmpty() || directory == m_directory)
        return;
    if (!QDir::isAbsolutePath(directory)) {
        setMessage(tr("The folder needs a full path, like ~/Music/NTS."));
        return;
    }
    m_directory = directory;
    m_queue.setDirectory(directory);
    if (m_persist)
        QSettings().setValue("directory", directory);
    emit directoryChanged();
    m_archive.refreshStatuses();
}

QString Backend::displayDirectory() const
{
    const QString home = QDir::homePath();
    return m_directory.startsWith(home + "/") ? "~" + m_directory.mid(home.size()) : m_directory;
}

QString Backend::status(const Nts::Episode &episode) const
{
    const QString queued = m_queue.statusFor(episode.show, episode.alias);
    if (!queued.isEmpty() && queued != "failed")
        return queued;
    if (!Nts::savedFile(m_directory, episode).isEmpty())
        return "done";
    return queued;
}

bool Backend::open(const QString &text)
{
    const Nts::Link link = Nts::parseLink(text);
    if (!link.isValid()) {
        setMessage(tr("That isn't an NTS show or episode link."));
        return false;
    }
    clearMessage();
    if (link.show != m_archive.showAlias() || !m_archive.error().isEmpty())
        m_archive.load(link.show);
    if (link.isEpisode()) {
        Nts::Episode episode;
        episode.show = link.show;
        episode.alias = link.episode;
        if (!m_queue.add(episode))
            setMessage(tr("That episode is already in the queue."));
    }
    return true;
}

QString Backend::linkText(const QString &text) const
{
    return Nts::linkText(Nts::parseLink(text));
}

void Backend::openFolder() const
{
    QDir().mkpath(m_directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_directory));
}

void Backend::checkTools()
{
    const Tools found = Tools::find();
    if (found.missing() == m_tools.missing())
        return;
    m_tools = found;
    m_queue.setTools(found);
    emit toolsChanged();
}

void Backend::setMessage(const QString &message)
{
    if (message == m_message)
        return;
    m_message = message;
    emit messageChanged();
}

void Backend::clearMessage()
{
    setMessage({});
}
