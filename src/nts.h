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
    QString cover;       // large artwork URL, embedded in the MP3
    QString thumbnail;   // small artwork URL, shown in the list
    bool hasAudio = true;

    QString pageUrl() const;
    QVariantMap toMap() const;
    static Episode fromMap(const QVariantMap &map);
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
// "Artist – Title" per track, in broadcast order.
QStringList tracksFromJson(const QByteArray &json);

// A name every filesystem accepts: no separators or control characters, no
// leading dot, and short enough to leave room for the directory.
QString safeFileName(const QString &name);
// <directory>/<Show>/<YYYY-MM-DD> - <Episode>.mp3
QString targetPath(const QString &directory, const Episode &episode);
// The text file saved beside the MP3 when NTS lists the tracks.
QString tracklistText(const Episode &episode, const QStringList &tracks);

}
