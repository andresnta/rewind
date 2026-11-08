#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

// Everything that knows the shape of nts.live: its links, its public JSON
// API, and how an episode is named on disk. Kept free of networking so the
// tests can feed it saved responses.
namespace Nts {

struct Link {
    QString show;
    QString episode;
    bool isValid() const { return !show.isEmpty(); }
    bool isEpisode() const { return !episode.isEmpty(); }
};

struct Episode {
    QString show;        // show alias, "floating-points"
    QString alias;       // episode alias, "floating-points-21st-september-2026"
    QString name;        // "Floating Points w/ Guest"
    QString showName;    // "Floating Points"
    QString date;        // broadcast day, ISO "2026-09-21"
    QString description;
    QString location;
    QStringList genres;
    QString cover;       // large artwork URL, embedded in the saved file
    QString thumbnail;   // small artwork URL, shown in the list
    bool hasAudio = true;

    QString pageUrl() const;
    QVariantMap toMap() const;
    static Episode fromMap(const QVariantMap &map);
};

// One track in an episode's tracklist. NTS gives exact start times for
// most tracks and estimates for the rest; -1 means it gives neither.
struct Track {
    QString artist;
    QString title;
    int start = -1;     // seconds into the episode
    int length = -1;    // seconds
    QString label() const;
    QVariantMap toMap() const;
};

struct Show {
    QString alias;
    QString name;
    QString description;
    QString location;
    QString thumbnail;
};

// Accepts any nts.live show or episode link, with or without the scheme,
// query, or trailing slash, and a bare show alias.
Link parseLink(const QString &text);
// "nts.live/shows/<show>[/episodes/<episode>]", the link as it's shown.
QString linkText(const Link &link);

// The API's base, "https://www.nts.live/api/v2". Tests point it at a local
// directory of saved responses.
QString apiBase();
void setApiBase(const QString &base);

QUrl showUrl(const QString &show);
QUrl episodesUrl(const QString &show, int offset, int limit);
QUrl episodeUrl(const QString &show, const QString &episode);
QUrl tracklistUrl(const QString &show, const QString &episode);

Episode episodeFromJson(const QJsonObject &object);
Show showFromJson(const QByteArray &json);
QList<Episode> episodesFromJson(const QByteArray &json, int *total = nullptr);
// Tracks in broadcast order.
QList<Track> tracksFromJson(const QByteArray &json);

// A name every filesystem accepts: no separators or control characters, no
// leading dot, and short enough to leave room for the directory.
QString safeFileName(const QString &name);
// <directory>/<Show>/<YYYY-MM-DD> - <Episode>, without an extension: the
// audio keeps whatever format NTS serves, so it's known only once fetched.
QString targetStem(const QString &directory, const Episode &episode);
// Audio formats rewind saves, and recognises as already saved (older
// versions converted everything to MP3).
QStringList audioExtensions();
// The episode's audio file if it's on disk in any of those formats, or "".
QString savedFile(const QString &directory, const Episode &episode);
// The text file saved beside the audio when NTS lists the tracks, with
// start times where known, and the reverse, so a saved episode shows its
// current track offline too.
QString tracklistText(const Episode &episode, const QList<Track> &tracks);
QList<Track> tracksFromText(const QString &text);
// "1:02:03" or "2:03".
QString clock(int seconds);

}
