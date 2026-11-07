#include "download.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

Tools Tools::find()
{
    return {QStandardPaths::findExecutable("yt-dlp"), QStandardPaths::findExecutable("ffmpeg")};
}

QStringList Tools::missing() const
{
    QStringList names;
    if (ytdlp.isEmpty())
        names << "yt-dlp";
    if (ffmpeg.isEmpty())
        names << "ffmpeg";
    return names;
}

Download::Download(const Nts::Episode &episode, const QString &directory, const Tools &tools,
                   QNetworkAccessManager *network, QObject *parent)
    : QObject(parent), m_episode(episode), m_directory(directory), m_tools(tools), m_network(network)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_stdout += m_process.readAllStandardOutput();
        m_state == Downloading ? readYtDlp() : readFfmpeg();
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_stderr += m_process.readAllStandardError();
        // Only the tail matters for an error message and ffmpeg's duration
        // line comes first, so keep the buffer from growing for hours.
        if (m_state == Saving)
            readFfmpeg();
        if (m_stderr.size() > 64 * 1024)
            m_stderr = m_stderr.right(32 * 1024);
    });
}

Download::~Download()
{
    // A download dropped mid-way, by a quit or a cleared queue, leaves nothing
    // behind but what already finished.
    if (!isFinished()) {
        blockSignals(true);
        cancel();
    }
}

void Download::setState(State state)
{
    m_state = state;
    m_progress = 0;
    emit changed();
}

void Download::setProgress(double progress)
{
    if (qAbs(progress - m_progress) < 0.002)
        return;
    m_progress = progress;
    emit changed();
}

void Download::get(const QUrl &url, std::function<void(QNetworkReply *)> done)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "rewind/0.1");
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network->get(request);
    m_replies << reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
        reply->deleteLater();
        m_replies.removeAll(reply);
        if (m_state == Cancelled)
            return;
        done(reply);
    });
}

void Download::start()
{
    if (m_state != Waiting)
        return;
    const QStringList missing = m_tools.missing();
    if (!missing.isEmpty())
        return fail(tr("%1 isn't installed.").arg(missing.join(", ")));
    if (!m_episode.hasAudio)
        return fail(tr("NTS has no audio for this episode."));

    m_temp = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/rewind-XXXXXX");
    if (!m_temp->isValid())
        return fail(tr("Couldn't create a temporary folder."));

    setState(Fetching);
    // An episode queued from a link knows only its aliases; one picked from
    // a show's archive already carries its names.
    if (m_episode.name.isEmpty() || m_episode.showName.isEmpty())
        fetchMetadata();
    else
        fetchExtras();
}

void Download::fetchMetadata()
{
    get(Nts::episodeUrl(m_episode.show, m_episode.alias), [this](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError)
            return fail(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404
                            ? tr("NTS has no episode at that link.")
                            : tr("Couldn't reach NTS: %1").arg(reply->errorString()));
        const QString show = m_episode.show, alias = m_episode.alias;
        m_episode = Nts::episodeFromJson(QJsonDocument::fromJson(reply->readAll()).object());
        if (m_episode.show.isEmpty())
            m_episode.show = show;
        if (m_episode.alias.isEmpty())
            m_episode.alias = alias;
        emit changed();
        if (!m_episode.hasAudio)
            return fail(tr("NTS has no audio for this episode."));

        get(Nts::showUrl(m_episode.show), [this](QNetworkReply *reply) {
            // The show's name is only for tags and the folder; the alias will do.
            if (reply->error() == QNetworkReply::NoError)
                m_episode.showName = Nts::showFromJson(reply->readAll()).name;
            if (m_episode.showName.isEmpty())
                m_episode.showName = m_episode.show;
            emit changed();
            fetchExtras();
        });
    });
}

void Download::fetchExtras()
{
    // Saved before, in any format, counts: never fetch an episode twice.
    const QString saved = Nts::savedFile(m_directory, m_episode);
    if (!saved.isEmpty()) {
        m_path = saved;
        m_skipped = true;
        return complete();
    }
    m_stem = Nts::targetStem(m_directory, m_episode);

    // Neither the tracklist nor the artwork is worth failing a download over.
    m_pendingExtras = m_episode.cover.isEmpty() ? 1 : 2;
    auto settle = [this] {
        if (--m_pendingExtras == 0)
            runYtDlp();
    };
    get(Nts::tracklistUrl(m_episode.show, m_episode.alias), [this, settle](QNetworkReply *reply) {
        if (reply->error() == QNetworkReply::NoError)
            m_tracks = Nts::tracksFromJson(reply->readAll());
        settle();
    });
    if (!m_episode.cover.isEmpty()) {
        get(QUrl(m_episode.cover), [this, settle](QNetworkReply *reply) {
            const QByteArray image = reply->readAll();
            QFile file(m_temp->filePath("cover.jpg"));
            if (reply->error() == QNetworkReply::NoError && !image.isEmpty() && file.open(QIODevice::WriteOnly)
                && file.write(image) == image.size())
                m_coverFile = file.fileName();
            settle();
        });
    }
}

void Download::runYtDlp()
{
    setState(Downloading);
    m_stdout.clear();
    m_stderr.clear();
    // The user's yt-dlp config could rename or re-encode the file under us,
    // so it's ignored; the progress template gives one parseable line each.
    const QStringList args{
        "--ignore-config", "--no-playlist", "--no-warnings", "--newline", "--progress",
        "-f", "bestaudio[ext=m4a]/bestaudio/best",
        "--progress-template",
        "download:@@ %(progress.downloaded_bytes)s %(progress.total_bytes,progress.total_bytes_estimate)s "
        "%(progress.fragment_index)s %(progress.fragment_count)s",
        "-o", m_temp->filePath("audio.%(ext)s"),
        m_episode.pageUrl()};

    disconnect(&m_process, &QProcess::finished, nullptr, nullptr);
    disconnect(&m_process, &QProcess::errorOccurred, nullptr, nullptr);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(tr("Couldn't start yt-dlp."));
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (m_state != Downloading)
            return;
        if (status != QProcess::NormalExit || code != 0) {
            // yt-dlp's last ERROR line says what went wrong in its own words.
            const QStringList lines = QString::fromUtf8(m_stderr + m_stdout).split('\n', Qt::SkipEmptyParts);
            QString reason;
            for (const auto &line : lines)
                if (line.startsWith("ERROR:"))
                    reason = line.mid(6).trimmed();
            return fail(reason.isEmpty() ? tr("yt-dlp couldn't download the audio.")
                                         : tr("yt-dlp: %1").arg(reason));
        }
        const QStringList found = QDir(m_temp->path()).entryList({"audio.*"}, QDir::Files);
        QStringList audio;
        for (const auto &name : found)
            if (!name.endsWith(".part") && !name.endsWith(".ytdl"))
                audio << name;
        if (audio.isEmpty())
            return fail(tr("yt-dlp finished without saving any audio."));
        m_audio = m_temp->filePath(audio.first());
        runFfmpeg();
    });
    m_process.start(m_tools.ytdlp, args);
}

void Download::readYtDlp()
{
    while (true) {
        const int end = m_stdout.indexOf('\n');
        if (end < 0)
            break;
        const QString line = QString::fromUtf8(m_stdout.left(end)).trimmed();
        m_stdout.remove(0, end + 1);
        if (!line.startsWith("@@ "))
            continue;
        const QStringList parts = line.mid(3).split(' ');
        if (parts.size() < 4)
            continue;
        bool ok1 = false, ok2 = false;
        const double done = parts[0].toDouble(&ok1), total = parts[1].toDouble(&ok2);
        const double fragment = parts[2].toDouble(), fragments = parts[3].toDouble();
        // HLS streams only estimate their size, which wobbles; fragments
        // counted against the total move steadily.
        if (fragments > 0)
            setProgress(qBound(0.0, fragment / fragments, 1.0));
        else if (ok1 && ok2 && total > 0)
            setProgress(qBound(0.0, done / total, 1.0));
    }
}

void Download::runFfmpeg()
{
    setState(Saving);
    m_stdout.clear();
    m_stderr.clear();
    m_duration = 0;

    // The audio is kept exactly as NTS serves it, only rewrapped with tags
    // and artwork, so there's no second lossy encode. Each format names the
    // muxer ffmpeg needs, since the .part name hides the extension.
    QString extension = QFileInfo(m_audio).suffix().toLower();
    if (extension == "mp4" || extension == "aac")
        extension = "m4a";
    static const QHash<QString, QString> muxers{{"m4a", "ipod"}, {"mp3", "mp3"}, {"opus", "opus"},
                                                {"ogg", "ogg"}, {"webm", "webm"}, {"flac", "flac"}};
    if (!muxers.contains(extension))
        return fail(tr("yt-dlp saved a format rewind doesn't know (%1).").arg(extension));
    m_path = m_stem + "." + extension;

    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return fail(tr("Couldn't create the folder %1.").arg(QFileInfo(m_path).absolutePath()));

    const QString part = QFileInfo(m_path).absolutePath() + "/." + QFileInfo(m_path).fileName() + ".part";
    // Only MP4, MP3, and FLAC carry cover art as a picture stream.
    const bool cover = !m_coverFile.isEmpty() && (extension == "m4a" || extension == "mp3" || extension == "flac");
    QStringList args{"-hide_banner", "-nostdin", "-y", "-i", m_audio};
    if (cover)
        args << "-i" << m_coverFile;
    args << "-map" << "0:a:0";
    if (cover)
        args << "-map" << "1:v:0" << "-c:v" << "mjpeg" << "-disposition:v:0" << "attached_pic"
             << "-metadata:s:v" << "title=Album cover" << "-metadata:s:v" << "comment=Cover (front)";
    args << "-c:a" << "copy";
    if (extension == "mp3")
        args << "-id3v2_version" << "3";

    QString comment = m_episode.description;
    comment += (comment.isEmpty() ? "" : "\n\n") + m_episode.pageUrl();
    args << "-map_metadata" << "-1"
         << "-metadata" << "title=" + m_episode.name
         << "-metadata" << "artist=" + m_episode.showName
         << "-metadata" << "album=" + m_episode.showName
         << "-metadata" << "album_artist=NTS Radio"
         << "-metadata" << "publisher=NTS Radio"
         << "-metadata" << "date=" + m_episode.date
         << "-metadata" << "genre=" + m_episode.genres.join(", ")
         << "-metadata" << "comment=" + comment
         << "-progress" << "pipe:1" << "-nostats"
         << "-f" << muxers.value(extension) << part;

    disconnect(&m_process, &QProcess::finished, nullptr, nullptr);
    disconnect(&m_process, &QProcess::errorOccurred, nullptr, nullptr);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(tr("Couldn't start ffmpeg."));
    });
    connect(&m_process, &QProcess::finished, this, [this, part](int code, QProcess::ExitStatus status) {
        if (m_state != Saving)
            return;
        if (status != QProcess::NormalExit || code != 0) {
            QFile::remove(part);
            const QStringList lines = QString::fromUtf8(m_stderr).split('\n', Qt::SkipEmptyParts);
            return fail(lines.isEmpty() ? tr("ffmpeg couldn't save the audio.")
                                        : tr("ffmpeg: %1").arg(lines.last().trimmed()));
        }
        // Another download of the same episode may have landed meanwhile;
        // never replace a file that's already there.
        const QString saved = Nts::savedFile(m_directory, m_episode);
        if (!saved.isEmpty()) {
            m_path = saved;
            QFile::remove(part);
            m_skipped = true;
            return complete();
        }
        if (!QFile::rename(part, m_path)) {
            QFile::remove(part);
            return fail(tr("Couldn't save %1.").arg(m_path));
        }
        complete();
    });
    m_process.start(m_tools.ffmpeg, args);
}

void Download::readFfmpeg()
{
    if (m_duration <= 0) {
        static const QRegularExpression duration(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))");
        const auto match = duration.match(QString::fromUtf8(m_stderr));
        if (match.hasMatch())
            m_duration = match.captured(1).toDouble() * 3600 + match.captured(2).toDouble() * 60
                       + match.captured(3).toDouble();
    }
    while (true) {
        const int end = m_stdout.indexOf('\n');
        if (end < 0)
            break;
        const QByteArray line = m_stdout.left(end).trimmed();
        m_stdout.remove(0, end + 1);
        if (line.startsWith("out_time_us=") && m_duration > 0)
            setProgress(qBound(0.0, line.mid(12).toDouble() / 1e6 / m_duration, 1.0));
    }
}

void Download::complete()
{
    if (!m_skipped && !m_tracks.isEmpty()) {
        const QString path = m_stem + ".txt";
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(Nts::tracklistText(m_episode, m_tracks).toUtf8());
            if (file.commit())
                m_tracklistPath = path;
        }
    }
    m_temp.reset();
    m_state = Done;
    m_progress = 1;
    emit changed();
    emit finished();
}

void Download::fail(const QString &message)
{
    if (isFinished())
        return;
    m_error = message;
    m_temp.reset();
    m_state = Failed;
    emit changed();
    emit finished();
}

void Download::cancel()
{
    if (isFinished())
        return;
    const bool converting = m_state == Saving;
    m_state = Cancelled;
    for (const auto &reply : m_replies)
        if (reply)
            reply->abort();
    m_replies.clear();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(2000);
    }
    if (converting && !m_path.isEmpty())
        QFile::remove(QFileInfo(m_path).absolutePath() + "/." + QFileInfo(m_path).fileName() + ".part");
    m_temp.reset();
    emit changed();
    emit finished();
}
