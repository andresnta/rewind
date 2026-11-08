// rewind — saves past NTS Radio episodes, with their artwork and tags, for offline listening.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Commands run headless, from scripts and agents, with no display at all.
    const bool command = Cli::handles(arguments);
    if (command)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("rewind");
    app.setOrganizationName("rewind");
    app.setApplicationVersion("0.4.2");
    if (command)
        return Cli::run(arguments);

    // `rewind`, `rewind open`, and `rewind open <link>` all open the window.
    QString link;
    if (arguments.value(1) == "open")
        link = arguments.value(2);
    else if (arguments.size() > 1)
        link = arguments.value(1);

    // Matches the window to rewind.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("rewind");
    app.setWindowIcon(QIcon::fromTheme("rewind"));

    QQuickStyle::setStyle("Material");
    // A see-through window needs an alpha channel, for Omarchy's glassy mode.
    QQuickWindow::setDefaultAlphaBuffer(true);

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("initialLink", link);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
