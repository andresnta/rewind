#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <functional>
#include <memory>

#include "nts.h"

// The command-line tools a download runs. Tests hand in fake scripts.
struct Tools {
    QString ytdlp;
    QString ffmpeg;
    static Tools find();
    // The tools that aren't installed, by package name, for the window to name.
    QStringList missing() const;
};

// One episode on its way to disk: fill in missing metadata from the NTS
// API, fetch the tracklist and artwork, have yt-dlp fetch the audio into a
// temporary directory, and have ffmpeg rewrap it, untouched, with tags and
// artwork beside the target before renaming it into place, so a half-written
// file never carries the real name.
class Download : public QObject {
    Q_OBJECT

public:
    enum State { Waiting, Fetching, Downloading, Saving, Done, Failed, Cancelled };
    Q_ENUM(State)

    Download(const Nts::Episode &episode, const QString &directory, const Tools &tools,
             QNetworkAccessManager *network, QObject *parent = nullptr);
    ~Download() override;

    void start();
    void cancel();

    const Nts::Episode &episode() const { return m_episode; }
    State state() const { return m_state; }
    bool isFinished() const { return m_state == Done || m_state == Failed || m_state == Cancelled; }
    // 0..1 within the current step, or -1 when it can't be measured.
    double progress() const { return m_progress; }
    QString error() const { return m_error; }
    QString path() const { return m_path; }
    QString tracklistPath() const { return m_tracklistPath; }
    bool skipped() const { return m_skipped; }

signals:
    void changed();
    void finished();

private:
    void fetchMetadata();
    void fetchExtras();
    void runYtDlp();
    void runFfmpeg();
    void complete();
    void fail(const QString &message);
    void setState(State state);
    void setProgress(double progress);
    void get(const QUrl &url, std::function<void(QNetworkReply *)> done);
    void readYtDlp();
    void readFfmpeg();

    Nts::Episode m_episode;
    QString m_directory;
    Tools m_tools;
    QNetworkAccessManager *m_network;
    State m_state = Waiting;
    double m_progress = 0;
    QString m_error, m_path, m_tracklistPath;
    bool m_skipped = false;

    std::unique_ptr<QTemporaryDir> m_temp;
    QList<QPointer<QNetworkReply>> m_replies;
    QProcess m_process;
    QByteArray m_stdout, m_stderr;
    QString m_stem, m_audio, m_coverFile;
    QList<Nts::Track> m_tracks;
    double m_duration = 0;
    int m_pendingExtras = 0;
};
