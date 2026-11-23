// drafts — Where text starts, built natively for Omarchy Linux.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "portalfilepicker.h"
#include "syntaxhighlighter.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList rawArgs;
    rawArgs.reserve(argc);
    for (int i = 0; i < argc; ++i)
        rawArgs.append(QString::fromLocal8Bit(argv[i]));

    // Headless CLI commands run without a display so scripts and agents can drive Drafts.
    if (Cli::isHeadlessCommand(rawArgs)) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qunsetenv("QT_QPA_PLATFORMTHEME");
        QGuiApplication app(argc, argv);
        app.setApplicationName("drafts");
        app.setApplicationVersion("0.1.0");
        return Cli::run(rawArgs);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("drafts");
    app.setApplicationVersion("0.1.0");

    // Matches the window to drafts.desktop so Hyprland and the launcher pick up the icon.
    app.setDesktopFileName("drafts");
    app.setWindowIcon(QIcon::fromTheme("drafts"));

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);

    qmlRegisterType<SyntaxHighlighterAdapter>("Drafts", 1, 0, "SyntaxHighlighter");

    Theme theme;
    PortalFilePicker filePicker;
    Backend backend({}, &filePicker, true);
    backend.setAccentColor(theme.accent());
    QObject::connect(&theme, &Theme::changed, &backend, [&] {
        backend.setAccentColor(theme.accent());
    });

    // Handle optional URL or draft UUID passed on launch
    const bool captureMode = (rawArgs.size() >= 2 && (rawArgs[1] == "capture" || rawArgs[1] == "--capture"));
    if (!captureMode && rawArgs.size() >= 2) {
        QString target = (rawArgs[1] == "open" && rawArgs.size() >= 3) ? rawArgs[2] : rawArgs[1];
        if (target.startsWith("drafts://", Qt::CaseInsensitive))
            backend.handleDraftsUrl(target);
        else if (rawArgs[1] != "open")
            backend.selectDraft(target);
        else if (rawArgs.size() >= 3)
            backend.selectDraft(target);
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);

    engine.load(QUrl(captureMode ? "qrc:/CaptureWindow.qml" : "qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
