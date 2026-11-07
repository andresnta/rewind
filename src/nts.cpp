#include "nts.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrlQuery>

namespace Nts {

namespace {
QString &base()
{
    static QString value = "https://www.nts.live/api/v2";
    return value;
}

QString text(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1String(key)).toString().simplified();
}
}

QString apiBase() { return base(); }
void setApiBase(const QString &value) { base() = value; }

QString Episode::pageUrl() const
{
    return QString("https://www.nts.live/shows/%1/episodes/%2").arg(show, alias);
}

QVariantMap Episode::toMap() const
{
    return {{"show", show}, {"alias", alias}, {"name", name}, {"showName", showName},
            {"date", date}, {"description", description}, {"location", location},
            {"genres", genres}, {"cover", cover}, {"thumbnail", thumbnail},
            {"hasAudio", hasAudio}, {"pageUrl", pageUrl()}};
}

Episode Episode::fromMap(const QVariantMap &map)
{
    Episode e;
    e.show = map.value("show").toString();
    e.alias = map.value("alias").toString();
    e.name = map.value("name").toString();
    e.showName = map.value("showName").toString();
    e.date = map.value("date").toString();
    e.description = map.value("description").toString();
    e.location = map.value("location").toString();
    e.genres = map.value("genres").toStringList();
    e.cover = map.value("cover").toString();
    e.thumbnail = map.value("thumbnail").toString();
    e.hasAudio = map.value("hasAudio", true).toBool();
    return e;
}

Link parseLink(const QString &input)
{
    const QString trimmed = input.trimmed();
    static const QRegularExpression link(
        R"((?:^|[/.])nts\.live/shows/([A-Za-z0-9_-]+)(?:/episodes/([A-Za-z0-9_-]+))?)");
    const auto match = link.match(trimmed);
    if (match.hasMatch())
        return {match.captured(1).toLower(), match.captured(2).toLower()};

    static const QRegularExpression alias("^[a-z0-9][a-z0-9_-]*$");
    if (alias.match(trimmed).hasMatch())
        return {trimmed, {}};
    return {};
}

QUrl showUrl(const QString &show)
{
    return QUrl(apiBase() + "/shows/" + show);
}

QUrl episodesUrl(const QString &show, int offset, int limit)
{
    QUrl url(apiBase() + "/shows/" + show + "/episodes");
    QUrlQuery query;
    query.addQueryItem("offset", QString::number(offset));
    query.addQueryItem("limit", QString::number(limit));
    url.setQuery(query);
    return url;
}

QUrl episodeUrl(const QString &show, const QString &episode)
{
    return QUrl(apiBase() + "/shows/" + show + "/episodes/" + episode);
}

QUrl tracklistUrl(const QString &show, const QString &episode)
{
    return QUrl(apiBase() + "/shows/" + show + "/episodes/" + episode + "/tracklist");
}

Episode episodeFromJson(const QJsonObject &object)
{
    Episode e;
    e.show = text(object, "show_alias");
    e.alias = text(object, "episode_alias");
    e.name = text(object, "name");
    e.description = object.value("description").toString().trimmed();
    e.location = text(object, "location_long");

    // Broadcast times are UTC; the day in London is the one NTS prints.
    const QString broadcast = text(object, "broadcast");
    const QDateTime when = QDateTime::fromString(broadcast, Qt::ISODate);
    e.date = when.isValid() ? when.toUTC().date().toString(Qt::ISODate) : broadcast.left(10);

    for (const auto &genre : object.value("genres").toArray()) {
        const QString value = genre.toObject().value("value").toString().trimmed();
        if (!value.isEmpty())
            e.genres << value;
    }

    const QJsonObject media = object.value("media").toObject();
    e.cover = media.value("picture_medium_large").toString();
    if (e.cover.isEmpty())
        e.cover = media.value("picture_large").toString();
    e.thumbnail = media.value("picture_small").toString();

    // NTS lists episodes whose audio never made it to Mixcloud or SoundCloud;
    // those can't be downloaded, and the list says so.
    e.hasAudio = !text(object, "mixcloud").isEmpty() || !object.value("audio_sources").toArray().isEmpty();
    return e;
}

Show showFromJson(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    Show show;
    show.alias = text(object, "show_alias");
    show.name = text(object, "name");
    show.description = object.value("description").toString().trimmed();
    show.location = text(object, "location_long");
    show.thumbnail = object.value("media").toObject().value("picture_small").toString();
    return show;
}

QList<Episode> episodesFromJson(const QByteArray &json, int *total)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    if (total)
        *total = object.value("metadata").toObject().value("resultset").toObject().value("count").toInt();
    QList<Episode> episodes;
    for (const auto &value : object.value("results").toArray())
        episodes << episodeFromJson(value.toObject());
    return episodes;
}

QString Track::label() const
{
    return artist.isEmpty() ? title : title.isEmpty() ? artist : artist + " – " + title;
}

QVariantMap Track::toMap() const
{
    return {{"artist", artist}, {"title", title}, {"start", start}, {"length", length}};
}

QList<Track> tracksFromJson(const QByteArray &json)
{
    // An exact time where NTS has one, its estimate where it doesn't.
    auto seconds = [](const QJsonObject &track, const char *exact, const char *estimate) {
        const QJsonValue value = track.value(QLatin1String(exact));
        if (value.isDouble())
            return value.toInt();
        const QJsonValue guess = track.value(QLatin1String(estimate));
        return guess.isDouble() ? guess.toInt() : -1;
    };
    QList<Track> tracks;
    const QJsonArray results = QJsonDocument::fromJson(json).object().value("results").toArray();
    for (const auto &value : results) {
        const QJsonObject object = value.toObject();
        Track track;
        track.artist = text(object, "artist");
        track.title = text(object, "title");
        if (track.artist.isEmpty() && track.title.isEmpty())
            continue;
        track.start = seconds(object, "offset", "offset_estimate");
        track.length = seconds(object, "duration", "duration_estimate");
        tracks << track;
    }
    return tracks;
}

QString clock(int seconds)
{
    const int h = seconds / 3600, m = seconds % 3600 / 60, s = seconds % 60;
    return h > 0 ? QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'))
                 : QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

QList<Track> tracksFromText(const QString &content)
{
    // "07. 1:02:03  Artist – Title", or without the time as older files have it.
    static const QRegularExpression line(R"(^\d+\.\s+(?:(?:(\d+):)?(\d+):(\d\d)\s\s)?(.+)$)");
    QList<Track> tracks;
    for (const auto &row : content.split('\n')) {
        const auto match = line.match(row.trimmed());
        if (!match.hasMatch())
            continue;
        Track track;
        if (!match.captured(3).isEmpty())
            track.start = match.captured(1).toInt() * 3600 + match.captured(2).toInt() * 60 + match.captured(3).toInt();
        const QString label = match.captured(4).trimmed();
        const int dash = label.indexOf(" – ");
        track.artist = dash < 0 ? QString() : label.left(dash);
        track.title = dash < 0 ? label : label.mid(dash + 3);
        tracks << track;
    }
    return tracks;
}

QString safeFileName(const QString &name)
{
    QString clean = name;
    static const QRegularExpression unsafe(R"([/\\:*?"<>|\x00-\x1f\x7f])");
    clean.replace(unsafe, " ");
    clean = clean.simplified();
    while (clean.startsWith('.'))
        clean.remove(0, 1);
    // 60 characters, plus the date and extension, stays under the 255-byte
    // name limit even for scripts that take four bytes a character.
    if (clean.size() > 60)
        clean = clean.left(60).trimmed();
    return clean.isEmpty() ? QString("Untitled") : clean;
}

QString targetStem(const QString &directory, const Episode &episode)
{
    const QString show = safeFileName(episode.showName.isEmpty() ? episode.show : episode.showName);
    QString file = safeFileName(episode.name.isEmpty() ? episode.alias : episode.name);
    if (!episode.date.isEmpty())
        file = episode.date + " - " + file;
    return directory + "/" + show + "/" + file;
}

QStringList audioExtensions()
{
    return {"m4a", "mp3", "opus", "ogg", "webm", "flac"};
}

QString savedFile(const QString &directory, const Episode &episode)
{
    const QString stem = targetStem(directory, episode);
    for (const auto &extension : audioExtensions())
        if (QFileInfo::exists(stem + "." + extension))
            return stem + "." + extension;
    return {};
}

QString tracklistText(const Episode &episode, const QList<Track> &tracks)
{
    QString out = episode.name + "\n";
    if (!episode.showName.isEmpty() && episode.showName != episode.name)
        out += episode.showName + "\n";
    out += episode.date + "\n" + episode.pageUrl() + "\n\n";
    const int width = QString::number(tracks.size()).size() < 2 ? 2 : QString::number(tracks.size()).size();
    for (int i = 0; i < tracks.size(); ++i)
        out += QString("%1. %2%3\n")
                   .arg(i + 1, width, 10, QChar('0'))
                   .arg(tracks.at(i).start >= 0 ? clock(tracks.at(i).start) + "  " : QString())
                   .arg(tracks.at(i).label());
    return out;
}

}
