#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTextStream>

#include "backend.h"
#include "download.h"
#include "nts.h"

namespace Cli {

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

int fail(const QString &message)
{
    err() << "rewind: " << message << Qt::endl;
    return 1;
}

// Pulls "--name value" out of the arguments, so what's left are positionals.
QString take(QStringList &args, const QString &name, const QString &fallback = {})
{
    const int i = args.indexOf(name);
    if (i < 0 || i + 1 >= args.size())
        return fallback;
    const QString value = args.at(i + 1);
    args.remove(i, 2);
    return value;
}

bool flag(QStringList &args, const QString &name)
{
    return args.removeAll(name) > 0;
}

QByteArray fetch(QNetworkAccessManager &network, const QUrl &url, QString *error)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "rewind/0.1");
    request.setTransferTimeout(30000);
    QNetworkReply *reply = network.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        *error = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404
                     ? QString("not found on NTS: %1").arg(url.toString())
                     : reply->errorString();
        return {};
    }
    return reply->readAll();
}

int episodes(QStringList args)
{
    const bool json = flag(args, "--json");
    const int offset = take(args, "--offset", "0").toInt();
    const int limit = qBound(1, take(args, "--limit", "24").toInt(), 100);
    if (args.size() != 1)
        return fail("usage: rewind episodes <show> [--offset N] [--limit N] [--json]");
    const Nts::Link link = Nts::parseLink(args.first());
    if (!link.isValid())
        return fail("not an NTS show or episode link: " + args.first());

    QNetworkAccessManager network;
    QString error;
    const Nts::Show show = Nts::showFromJson(fetch(network, Nts::showUrl(link.show), &error));
    if (!error.isEmpty())
        return fail(error);
    int total = 0;
    const QList<Nts::Episode> list =
        Nts::episodesFromJson(fetch(network, Nts::episodesUrl(link.show, offset, limit), &error), &total);
    if (!error.isEmpty())
        return fail(error);

    if (json) {
        QJsonArray items;
        for (auto e : list) {
            e.showName = show.name;
            items << QJsonObject{{"alias", e.alias}, {"name", e.name}, {"date", e.date}, {"url", e.pageUrl()},
                                 {"genres", QJsonArray::fromStringList(e.genres)}, {"hasAudio", e.hasAudio}};
        }
        out() << QJsonDocument(QJsonObject{{"show", link.show}, {"name", show.name}, {"total", total},
                                           {"offset", offset}, {"episodes", items}})
                     .toJson(QJsonDocument::Indented);
        return 0;
    }
    out() << show.name << " — " << total << " episodes" << Qt::endl;
    for (const auto &e : list)
        out() << e.date << "  " << e.name << (e.hasAudio ? "" : "  (no audio)") << "\n    " << e.pageUrl() << "\n";
    out().flush();
    return 0;
}

int download(QStringList args)
{
    const bool json = flag(args, "--json");
    const QString output = take(args, "--output", Backend::defaultDirectory());
    if (args.isEmpty())
        return fail("usage: rewind download <episode link>... [--output DIR] [--json]");

    const Tools tools = Tools::find();
    if (!tools.missing().isEmpty())
        return fail(tools.missing().join(", ") + " isn't installed; install it with: omarchy-pkg-add "
                    + tools.missing().join(" "));

    QNetworkAccessManager network;
    QJsonArray results;
    bool allOk = true;
    for (const auto &arg : args) {
        const Nts::Link link = Nts::parseLink(arg);
        if (!link.isEpisode()) {
            allOk = false;
            err() << "rewind: not an NTS episode link: " << arg << Qt::endl;
            results << QJsonObject{{"url", arg}, {"ok", false}, {"error", "not an NTS episode link"}};
            continue;
        }
        Nts::Episode episode;
        episode.show = link.show;
        episode.alias = link.episode;
        Download job(episode, QDir(output).absolutePath(), tools, &network);
        QEventLoop loop;
        int lastStep = -1, lastPercent = -1;
        QObject::connect(&job, &Download::changed, &loop, [&] {
            if (json)
                return;
            const int step = int(job.state()), percent = int(job.progress() * 100);
            if (step == lastStep && percent / 5 == lastPercent / 5)
                return;
            lastStep = step;
            lastPercent = percent;
            const char *names[] = {"waiting", "fetching", "downloading", "converting", "done", "failed",
                                   "cancelled"};
            err() << "\r" << names[step] << " " << percent << "%   " << Qt::flush;
        });
        QObject::connect(&job, &Download::finished, &loop, &QEventLoop::quit);
        job.start();
        if (!job.isFinished())
            loop.exec();
        if (!json)
            err() << "\r" << QString(30, ' ') << "\r" << Qt::flush;

        const bool ok = job.state() == Download::Done;
        allOk &= ok;
        QJsonObject result{{"url", episode.pageUrl()}, {"ok", ok}, {"name", job.episode().name},
                           {"show", job.episode().showName}, {"date", job.episode().date}};
        if (ok) {
            result["path"] = job.path();
            result["skipped"] = job.skipped();
            if (!job.tracklistPath().isEmpty())
                result["tracklist"] = job.tracklistPath();
        } else {
            result["error"] = job.error();
        }
        results << result;
        if (!ok)
            err() << "rewind: " << episode.pageUrl() << ": " << job.error() << Qt::endl;
        else if (!json)
            out() << (job.skipped() ? "already saved: " : "saved: ") << job.path() << Qt::endl;
    }
    if (json)
        out() << QJsonDocument(results).toJson(QJsonDocument::Indented) << Qt::flush;
    return allOk ? 0 : 1;
}

int skill(QStringList args)
{
    if (args.value(0) != "install")
        return fail("usage: rewind skill install");
    QFile source(":/skill/SKILL.md");
    if (!source.open(QIODevice::ReadOnly))
        return fail("the bundled skill is missing");
    const QString directory = QDir::homePath() + "/.claude/skills/rewind";
    if (!QDir().mkpath(directory))
        return fail("couldn't create " + directory);
    QSaveFile file(directory + "/SKILL.md");
    if (!file.open(QIODevice::WriteOnly) || file.write(source.readAll()) < 0 || !file.commit())
        return fail("couldn't write " + file.fileName());
    out() << "installed " << file.fileName() << Qt::endl;
    return 0;
}

}

QString usage()
{
    return "Usage:\n"
           "  rewind [open] [link]                      open the window, optionally on a show or episode\n"
           "  rewind episodes <show> [--offset N] [--limit N] [--json]\n"
           "                                            list a show's past episodes\n"
           "  rewind download <episode link>... [--output DIR] [--json]\n"
           "                                            save episodes as tagged audio files (default ~/Music/NTS)\n"
           "  rewind skill install                      install the agent skill\n";
}

bool handles(const QStringList &arguments)
{
    const QString command = arguments.value(1);
    return command == "episodes" || command == "download" || command == "skill" || command == "help"
        || command == "--help" || command == "-h" || command == "--version";
}

int run(const QStringList &arguments)
{
    QStringList args = arguments.mid(2);
    const QString command = arguments.value(1);
    if (command == "episodes")
        return episodes(args);
    if (command == "download")
        return download(args);
    if (command == "skill")
        return skill(args);
    if (command == "--version") {
        out() << "rewind " << QCoreApplication::applicationVersion() << Qt::endl;
        return 0;
    }
    out() << usage() << Qt::flush;
    return 0;
}

}
