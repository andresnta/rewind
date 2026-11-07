#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "download.h"
#include "nts.h"
#include "queue.h"
#include "theme.h"
#include "translator.h"

// A saved NTS episode, trimmed to the fields rewind reads.
// Kept as one ordinary literal: moc misreads multi-line raw strings.
static const char *EpisodeJson =
    "{\"name\": \"  Floating Points w/ A Guest \",\"description\": \"Two hours of records.\",\"location_long\": \"London\",\"genres\": [{\"id\": \"a\", \"value\": \"Funk\"}, {\"id\": \"b\", \"value\": \"Soul\"}],\"media\": {\"picture_medium_large\": \"https://example.com/800.jpeg\", \"picture_small\": \"https://example.com/200.jpeg\"},\"episode_alias\": \"floating-points-21st-september-2026\",\"show_alias\": \"floating-points\",\"broadcast\": \"2026-09-21T23:30:00+00:00\",\"mixcloud\": \"https://www.mixcloud.com/NTSRadio/x/\",\"audio_sources\": []}";

class RewindTests : public QObject {
    Q_OBJECT

private slots:
    void parsesLinks()
    {
        auto link = Nts::parseLink("https://www.nts.live/shows/floating-points/episodes/fp-21st-september-2026?x=1");
        QCOMPARE(link.show, QString("floating-points"));
        QCOMPARE(link.episode, QString("fp-21st-september-2026"));

        link = Nts::parseLink("  nts.live/shows/Floating-Points/ ");
        QCOMPARE(link.show, QString("floating-points"));
        QVERIFY(!link.isEpisode());

        QCOMPARE(Nts::parseLink("floating-points").show, QString("floating-points"));
        QVERIFY(!Nts::parseLink("https://example.com/shows/x").isValid());
        QVERIFY(!Nts::parseLink("not a link").isValid());
        QVERIFY(!Nts::parseLink("").isValid());
    }

    void readsEpisodes()
    {
        const auto e = Nts::episodeFromJson(QJsonDocument::fromJson(EpisodeJson).object());
        QCOMPARE(e.name, QString("Floating Points w/ A Guest"));
        QCOMPARE(e.show, QString("floating-points"));
        QCOMPARE(e.date, QString("2026-09-21"));
        QCOMPARE(e.genres, QStringList({"Funk", "Soul"}));
        QCOMPARE(e.cover, QString("https://example.com/800.jpeg"));
        QVERIFY(e.hasAudio);
        QCOMPARE(e.pageUrl(),
                 QString("https://www.nts.live/shows/floating-points/episodes/floating-points-21st-september-2026"));

        const QByteArray page = QByteArray(R"({"metadata":{"resultset":{"count":109}},"results":[)") + EpisodeJson
                              + R"(,{"name":"Silent","episode_alias":"s","show_alias":"floating-points","audio_sources":[]}]})";
        int total = 0;
        const auto list = Nts::episodesFromJson(page, &total);
        QCOMPARE(total, 109);
        QCOMPARE(list.size(), 2);
        QVERIFY(!list.at(1).hasAudio);

        const auto round = Nts::Episode::fromMap(e.toMap());
        QCOMPARE(round.name, e.name);
        QCOMPARE(round.genres, e.genres);
    }

    void readsTracklists()
    {
        // Exact times win over estimates, and a track with neither has none.
        const auto tracks = Nts::tracksFromJson(R"({"results":[
            {"artist":"Artist One","title":"First","offset":null,"offset_estimate":8,"duration":null,"duration_estimate":772},
            {"artist":"","title":"Untitled","offset":3725,"offset_estimate":3600,"duration":120},
            {"artist":"Artist Three","title":"Third"},
            {"artist":"","title":""}]})");
        QCOMPARE(tracks.size(), 3);
        QCOMPARE(tracks.at(0).label(), QString("Artist One – First"));
        QCOMPARE(tracks.at(0).start, 8);
        QCOMPARE(tracks.at(0).length, 772);
        QCOMPARE(tracks.at(1).label(), QString("Untitled"));
        QCOMPARE(tracks.at(1).start, 3725);
        QCOMPARE(tracks.at(2).start, -1);

        Nts::Episode e;
        e.show = "s"; e.alias = "a"; e.name = "Ep"; e.showName = "Show"; e.date = "2026-01-02";
        const QString text = Nts::tracklistText(e, tracks);
        QVERIFY(text.startsWith("Ep\nShow\n2026-01-02\nhttps://www.nts.live/shows/s/episodes/a\n\n"));
        QVERIFY(text.contains("01. 0:08  Artist One – First\n02. 1:02:05  Untitled\n03. Artist Three – Third\n"));

        // The saved file reads back with its times, and so do older ones without.
        const auto back = Nts::tracksFromText(text);
        QCOMPARE(back.size(), 3);
        QCOMPARE(back.at(0).artist, QString("Artist One"));
        QCOMPARE(back.at(0).title, QString("First"));
        QCOMPARE(back.at(0).start, 8);
        QCOMPARE(back.at(1).title, QString("Untitled"));
        QCOMPARE(back.at(1).start, 3725);
        QCOMPARE(back.at(2).start, -1);
        const auto old = Nts::tracksFromText("Ep\n\n01. Artist – Song\n02. Other\n");
        QCOMPARE(old.size(), 2);
        QCOMPARE(old.at(0).artist, QString("Artist"));
        QCOMPARE(old.at(0).start, -1);
    }

    void namesFilesSafely()
    {
        QCOMPARE(Nts::safeFileName("AC/DC: Live? <yes>"), QString("AC DC Live yes"));
        QCOMPARE(Nts::safeFileName("..hidden"), QString("hidden"));
        QCOMPARE(Nts::safeFileName("   "), QString("Untitled"));
        QVERIFY(Nts::safeFileName(QString(300, 'x')).size() <= 60);

        Nts::Episode e;
        e.show = "fp"; e.showName = "Floating Points"; e.name = "w/ Guest"; e.date = "2026-09-21";
        QCOMPARE(Nts::targetStem("/music", e), QString("/music/Floating Points/2026-09-21 - w Guest"));
    }

    void findsSavedFilesInAnyFormat()
    {
        QTemporaryDir dir;
        Nts::Episode e;
        e.show = "fp"; e.showName = "Show"; e.name = "Ep"; e.date = "2026-01-02";
        QVERIFY(Nts::savedFile(dir.path(), e).isEmpty());
        QDir().mkpath(dir.filePath("Show"));
        write(dir.filePath("Show/2026-01-02 - Ep.txt"), "tracks");
        QVERIFY(Nts::savedFile(dir.path(), e).isEmpty()); // a tracklist alone isn't the episode
        write(dir.filePath("Show/2026-01-02 - Ep.opus"), "audio");
        QCOMPARE(Nts::savedFile(dir.path(), e), dir.filePath("Show/2026-01-02 - Ep.opus"));
    }

    void keepsTheOriginalAudio()
    {
        Fixture f;
        Download job(f.episode(), f.dir.filePath("out"), f.tools(), &f.network);
        QSignalSpy finished(&job, &Download::finished);
        job.start();
        QVERIFY(finished.wait(10000));
        QCOMPARE(job.state(), Download::Done);
        QVERIFY2(job.error().isEmpty(), qPrintable(job.error()));

        const QString audio = f.dir.filePath("out/Floating Points/2026-09-21 - Floating Points w A Guest.m4a");
        QCOMPARE(job.path(), audio);
        QVERIFY(QFile::exists(audio));
        QCOMPARE(job.tracklistPath(), audio.left(audio.size() - 4) + ".txt");
        QVERIFY(read(job.tracklistPath()).contains("01. Artist One – First"));

        // The tags and cover reached ffmpeg, and the audio was copied, not
        // re-encoded.
        const QString args = read(f.dir.filePath("ffmpeg-args"));
        QVERIFY(args.contains("title=Floating Points w/ A Guest"));
        QVERIFY(args.contains("artist=Floating Points"));
        QVERIFY(args.contains("date=2026-09-21"));
        QVERIFY(args.contains("genre=Funk, Soul"));
        QVERIFY(args.contains("cover.jpg"));
        QVERIFY(args.contains("-c:a\ncopy\n"));
        QVERIFY(args.contains("-f\nipod\n"));
        QVERIFY(!args.contains("libmp3lame"));
        // Nothing half-written is left beside it.
        QCOMPARE(QDir(f.dir.filePath("out/Floating Points")).entryList(QDir::Files | QDir::Hidden).size(), 2);
    }

    void neverOverwritesASavedEpisode()
    {
        // An MP3 from before rewind kept originals counts as saved too.
        Fixture f;
        const QString mp3 = f.dir.filePath("out/Floating Points/2026-09-21 - Floating Points w A Guest.mp3");
        QDir().mkpath(QFileInfo(mp3).absolutePath());
        write(mp3, "original");

        Download job(f.episode(), f.dir.filePath("out"), f.tools(), &f.network);
        QSignalSpy finished(&job, &Download::finished);
        job.start();
        QVERIFY(finished.count() || finished.wait(5000));
        QCOMPARE(job.state(), Download::Done);
        QVERIFY(job.skipped());
        QCOMPARE(read(mp3), QString("original"));
        QVERIFY(!QFile::exists(f.dir.filePath("ytdlp-ran")));
    }

    void fillsInMetadataFromALink()
    {
        Fixture f;
        Nts::Episode bare;
        bare.show = "floating-points";
        bare.alias = "floating-points-21st-september-2026";
        Download job(bare, f.dir.filePath("out"), f.tools(), &f.network);
        QSignalSpy finished(&job, &Download::finished);
        job.start();
        QVERIFY(finished.wait(10000));
        QVERIFY2(job.state() == Download::Done, qPrintable(job.error()));
        QCOMPARE(job.episode().showName, QString("Floating Points"));
        QCOMPARE(job.episode().name, QString("Floating Points w/ A Guest"));
    }

    void reportsYtDlpErrors()
    {
        Fixture f;
        f.script("yt-dlp", "#!/bin/sh\necho 'ERROR: [soundcloud] This track is unavailable' >&2\nexit 1\n");
        Download job(f.episode(), f.dir.filePath("out"), f.tools(), &f.network);
        QSignalSpy finished(&job, &Download::finished);
        job.start();
        QVERIFY(finished.wait(10000));
        QCOMPARE(job.state(), Download::Failed);
        QVERIFY(job.error().contains("This track is unavailable"));
        QVERIFY(!QDir(f.dir.filePath("out/Floating Points")).exists()
                || QDir(f.dir.filePath("out/Floating Points")).isEmpty());
    }

    void saysWhichToolIsMissing()
    {
        Fixture f;
        Tools tools = f.tools();
        tools.ytdlp.clear();
        QCOMPARE(tools.missing(), QStringList({"yt-dlp"}));
        Download job(f.episode(), f.dir.filePath("out"), tools, &f.network);
        job.start();
        QCOMPARE(job.state(), Download::Failed);
        QVERIFY(job.error().contains("yt-dlp"));
    }

    void cancelsCleanly()
    {
        Fixture f;
        f.script("yt-dlp", "#!/bin/sh\necho '@@ 1 100 NA NA'\nsleep 30\n");
        Download job(f.episode(), f.dir.filePath("out"), f.tools(), &f.network);
        job.start();
        QTRY_COMPARE_WITH_TIMEOUT(job.state(), Download::Downloading, 5000);
        job.cancel();
        QCOMPARE(job.state(), Download::Cancelled);
        QVERIFY(Nts::savedFile(f.dir.filePath("out"), f.episode()).isEmpty());
    }

    void rejectsFormatsItCantTag()
    {
        Fixture f;
        f.script("yt-dlp", "#!/bin/sh\nwhile [ $# -gt 0 ]; do [ \"$1\" = -o ] && out=\"$2\"; shift; done\n"
                           "printf x > \"$(echo \"$out\" | sed 's/%(ext)s/xyz/')\"\n");
        Download job(f.episode(), f.dir.filePath("out"), f.tools(), &f.network);
        QSignalSpy finished(&job, &Download::finished);
        job.start();
        QVERIFY(finished.wait(10000));
        QCOMPARE(job.state(), Download::Failed);
        QVERIFY(job.error().contains("xyz"));
    }

    void remembersWherePlaybackStopped()
    {
        QTemporaryDir dir;
        {
            Backend backend(dir.path(), Tools{"/bin/true", "/bin/true"});
            // Keyed by episode, so streaming and the saved file share a place.
            const QString key = backend.episodeKey({{"show", "fp"}, {"alias", "ep-1"}});
            QCOMPARE(key, QString("fp/ep-1"));
            backend.savePosition(key, 3600000);
            backend.savePosition("fp/ep-2", 2000); // too early to bother
            QCOMPARE(backend.resumePosition(key), 3600000);
            QCOMPARE(backend.resumePosition("fp/ep-2"), 0);
        }
        Backend again(dir.path(), Tools{"/bin/true", "/bin/true"});
        QCOMPARE(again.resumePosition("fp/ep-1"), 3600000);
        again.forgetPosition("fp/ep-1");
        QCOMPARE(again.resumePosition("fp/ep-1"), 0);
        QCOMPARE(again.fileUrl("/music/a b.m4a"), QUrl("file:///music/a%20b.m4a"));
    }

    void queueRunsOneAtATime()
    {
        Fixture f;
        Queue queue(&f.network, f.tools());
        queue.setDirectory(f.dir.filePath("out"));
        Nts::Episode a = f.episode(), b = f.episode();
        b.alias = "second";
        b.name = "Second";
        QVERIFY(queue.add(a));
        QVERIFY(!queue.add(a)); // already waiting or running
        QVERIFY(queue.add(b));
        QCOMPARE(queue.count(), 2);
        QCOMPARE(queue.statusFor(b.show, b.alias), QString("waiting"));
        QTRY_COMPARE_WITH_TIMEOUT(queue.activeCount(), 0, 15000);
        QCOMPARE(queue.statusFor(a.show, a.alias), QString("done"));
        QCOMPARE(queue.statusFor(b.show, b.alias), QString("done"));
        queue.clearFinished();
        QCOMPARE(queue.count(), 0);
    }

    void backendRejectsOtherLinks()
    {
        QTemporaryDir dir;
        Backend backend(dir.path(), Tools{"/bin/true", "/bin/true"});
        QVERIFY(!backend.open("https://example.com/whatever"));
        QVERIFY(!backend.message().isEmpty());
        backend.setDirectory("relative/path");
        QCOMPARE(backend.directory(), dir.path());
        backend.setDirectory("~/Music/Elsewhere");
        QCOMPARE(backend.directory(), QDir::homePath() + "/Music/Elsewhere");
    }

    void resolvesStreams()
    {
        Fixture f;
        f.script("yt-dlp", "#!/bin/sh\necho 'https://cdn.example.com/a/playlist.m3u8'\n");
        Backend backend(f.dir.filePath("out"), f.tools());
        QSignalSpy ready(&backend, &Backend::streamReady);
        backend.resolveStream(f.episode().toMap());
        QVERIFY(ready.wait(5000));
        QCOMPARE(ready.first().at(0).toString(), QString("floating-points/floating-points-21st-september-2026"));
        QCOMPARE(ready.first().at(1).toUrl(), QUrl("https://cdn.example.com/a/playlist.m3u8"));

        f.script("yt-dlp", "#!/bin/sh\necho 'ERROR: [soundcloud] Gone' >&2\nexit 1\n");
        QSignalSpy failed(&backend, &Backend::streamFailed);
        backend.resolveStream(f.episode().toMap());
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().at(1).toString().contains("Gone"));
    }

    void loadsTracksOnlineAndOffline()
    {
        Fixture f;
        f.routes["/api/v2/shows/floating-points/episodes/floating-points-21st-september-2026/tracklist"] =
            R"({"results":[{"artist":"Artist One","title":"First","offset":0},{"artist":"Two","title":"Second","offset":600}]})";
        Backend backend(f.dir.filePath("out"), f.tools());
        QSignalSpy changed(&backend, &Backend::tracksChanged);
        backend.loadTracks(f.episode().toMap(), {});
        QTRY_COMPARE_WITH_TIMEOUT(backend.tracks().size(), 2, 5000);
        QVERIFY(backend.tracksTimed());
        QCOMPARE(backend.tracks().at(1).toMap().value("start").toInt(), 600);

        // With NTS out of reach, the text file beside a saved episode stands in.
        f.server.close();
        Nts::setApiBase("http://127.0.0.1:1/api/v2");
        QDir().mkpath(f.dir.filePath("saved"));
        write(f.dir.filePath("saved/ep.txt"), "Ep\n\n01. 0:00  Offline – One\n02. 12:00  Offline – Two\n03. Offline – Three\n");
        backend.loadTracks(f.episode().toMap(), f.dir.filePath("saved/ep.m4a"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.tracks().size(), 3, 10000);
        QCOMPARE(backend.tracks().at(1).toMap().value("start").toInt(), 720);
        QCOMPARE(backend.tracks().at(2).toMap().value("title").toString(), QString("Three"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    void bundledCatalogsAreValid()
    {
        // Every shipped catalog parses, and keeps the placeholders it translates.
        QDir catalogs(QFINDTESTDATA("../src/i18n"));
        const QStringList files = catalogs.entryList({"*.json"});
        QVERIFY(files.contains("en.json"));
        for (const auto &name : files) {
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(read(catalogs.filePath(name)).toUtf8(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(name + ": " + error.errorString()));
            const QJsonObject object = doc.object();
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (!it.value().isString())
                    continue;
                for (const auto &mark : {"%1", "%2"})
                    QVERIFY2(it.key().contains(mark) == it.value().toString().contains(mark),
                             qPrintable(name + ": " + it.key()));
            }
        }
    }

private:
    // A local stand-in for nts.live and the two tools: the API is a tiny
    // HTTP server answering from a map of paths, and yt-dlp and ffmpeg are
    // scripts.
    struct Fixture {
        QTemporaryDir dir;
        QNetworkAccessManager network;
        QTcpServer server;
        QHash<QString, QByteArray> routes;

        Fixture()
        {
            QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
                while (QTcpSocket *socket = server.nextPendingConnection()) {
                    QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                        const QByteArray request = socket->readAll();
                        const QString path = QString::fromUtf8(request.split(' ').value(1)).section('?', 0, 0);
                        const bool found = routes.contains(path);
                        const QByteArray body = found ? routes.value(path) : QByteArray("{}");
                        socket->write((found ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n")
                                      + QByteArray("Content-Length: ") + QByteArray::number(body.size())
                                      + "\r\nConnection: close\r\n\r\n" + body);
                        socket->disconnectFromHost();
                    });
                    QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                }
            });
            server.listen(QHostAddress::LocalHost);
            Nts::setApiBase(base() + "/api/v2");

            const QString episodePath = "/api/v2/shows/floating-points/episodes/floating-points-21st-september-2026";
            routes[episodePath] = EpisodeJson;
            routes[episodePath + "/tracklist"] = R"({"results":[{"artist":"Artist One","title":"First"}]})";
            routes["/api/v2/shows/floating-points"] = R"({"name":"Floating Points","show_alias":"floating-points"})";
            routes["/cover.jpeg"] = "jpeg";

            script("yt-dlp", "#!/bin/sh\n"
                             "touch '" + dir.filePath("ytdlp-ran").toUtf8() + "'\n"
                             "while [ $# -gt 0 ]; do [ \"$1\" = -o ] && out=\"$2\"; shift; done\n"
                             "echo '@@ 50 100 NA NA'\necho '@@ 100 100 NA NA'\n"
                             "printf audio > \"$(echo \"$out\" | sed 's/%(ext)s/m4a/')\"\n");
            script("ffmpeg", "#!/bin/sh\n"
                             "printf '%s\\n' \"$@\" > '" + dir.filePath("ffmpeg-args").toUtf8() + "'\n"
                             "echo '  Duration: 00:00:10.00, start: 0' >&2\n"
                             "echo 'out_time_us=5000000'\necho 'out_time_us=10000000'\n"
                             "for a; do last=\"$a\"; done\nprintf mp3 > \"$last\"\n");
        }

        ~Fixture() { Nts::setApiBase("https://www.nts.live/api/v2"); }

        QString base() const { return QString("http://127.0.0.1:%1").arg(server.serverPort()); }

        Nts::Episode episode()
        {
            Nts::Episode e = Nts::episodeFromJson(QJsonDocument::fromJson(EpisodeJson).object());
            e.showName = "Floating Points";
            e.cover = base() + "/cover.jpeg";
            return e;
        }

        Tools tools() { return {dir.filePath("yt-dlp"), dir.filePath("ffmpeg")}; }

        void script(const QString &name, const QByteArray &body)
        {
            write(dir.filePath(name), body);
            QFile::setPermissions(dir.filePath(name), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        }
    };

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static QString read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }
};

QTEST_MAIN(RewindTests)
#include "rewind_tests.moc"
