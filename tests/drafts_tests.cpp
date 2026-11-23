#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include "actionengine.h"
#include "backend.h"
#include "cli.h"
#include "markdown.h"
#include "store.h"
#include "syntaxhighlighter.h"
#include "templateengine.h"
#include "theme.h"
#include "translator.h"

class DraftsTests : public QObject {
    Q_OBJECT

private slots:
    void storeDraftCrudAndMetadata() {
        QTemporaryDir dir;
        Store store(dir.filePath("drafts.db"));
        QVERIFY(store.isOpen());

        DraftRecord d = store.createDraft("# Project Alpha\nSecond line body", {"work/alpha", "status::active"});
        QVERIFY(!d.uuid.isEmpty());
        QCOMPARE(d.title(), QString("Project Alpha"));
        QCOMPARE(d.folder, QString("inbox"));
        QCOMPARE(d.tags.size(), 2);

        // Scoped tag exclusivity: adding status::done replaces status::active.
        QVERIFY(store.addTagToDraft(d.uuid, "status::done"));
        DraftRecord loaded;
        QVERIFY(store.getDraft(d.uuid, loaded));
        QVERIFY(loaded.tags.contains("status::done"));
        QVERIFY(!loaded.tags.contains("status::active"));
        QVERIFY(loaded.tags.contains("work/alpha"));

        // Nested tag query: filtering by "work" matches "work/alpha".
        DraftQuery dq;
        dq.folder = "inbox";
        dq.includeTags = {"work"};
        auto matches = store.queryDrafts(dq);
        QCOMPARE(matches.size(), 1);
        QCOMPARE(matches.first().uuid, d.uuid);

        // Flagging + color + archive + trash
        QVERIFY(store.setDraftFlagged(d.uuid, true, "purple"));
        QVERIFY(store.setDraftFolder(d.uuid, "archive"));
        QVERIFY(store.getDraft(d.uuid, loaded));
        QVERIFY(loaded.flagged);
        QCOMPARE(loaded.flagColor, QString("purple"));
        QCOMPARE(loaded.folder, QString("archive"));
    }

    void advancedSearchAndWorkspaces() {
        QTemporaryDir dir;
        Store store(dir.filePath("drafts.db"));

        store.createDraft("# Sprint Plan\nShip release v2.0 soon", {"work/sprint", "active"});
        store.createDraft("# Grocery List\nMilk and eggs", {"personal"});
        store.createDraft("# Waiting Task\nShip release blocked", {"work/sprint", "waiting"}, "inbox", "markdown", true);

        // Exact phrase + negation
        DraftQuery dq;
        dq.folder = "all";
        dq.query = "\"Ship release\" -blocked";
        auto hits = store.queryDrafts(dq);
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits.first().title(), QString("Sprint Plan"));

        // Regex search
        dq.query = "/v[0-9]+\\.[0-9]+/";
        hits = store.queryDrafts(dq);
        QCOMPARE(hits.size(), 1);

        // Workspace filtering with tag omit (-waiting)
        WorkspaceRecord ws;
        ws.name = "Focus Work";
        ws.tagFilter = "work, -waiting";
        ws.defaultFolder = "inbox";
        ws = store.saveWorkspace(ws);

        DraftQuery wq;
        wq.folder = ws.defaultFolder;
        wq.includeTags = {"work"};
        wq.omitTags = {"waiting"};
        hits = store.queryDrafts(wq);
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits.first().title(), QString("Sprint Plan"));
    }

    void versionHistoryAndRestore() {
        QTemporaryDir dir;
        Store store(dir.filePath("drafts.db"));
        DraftRecord d = store.createDraft("# Note\nVersion 1");
        qint64 v1 = store.saveVersion(d.uuid, d.content, false);
        QVERIFY(v1 > 0);

        d.content = "# Note\nVersion 2";
        store.updateDraft(d, true);
        store.saveVersion(d.uuid, d.content, true);

        auto versions = store.versionsForDraft(d.uuid);
        QCOMPARE(versions.size(), 2);

        DraftRecord restored;
        QVERIFY(store.restoreVersion(d.uuid, v1, restored));
        QCOMPARE(restored.content, QString("# Note\nVersion 1"));
    }

    void templateEngineTokensAndDateMath() {
        TemplateContext ctx;
        ctx.draft.uuid = "ABC-123";
        ctx.draft.content = "# Launch Checklist\n- [ ] Build\n- [x] Test";
        ctx.draft.tags = {"ops", "release"};
        ctx.selectionStart = 19;
        ctx.selectionLength = 11; // "- [ ] Build"
        ctx.clipboard = "https://omarchy.org";
        ctx.now = QDateTime(QDate(2026, 11, 23), QTime(14, 30, 0));

        QCOMPARE(TemplateEngine::process("[[title]]", ctx), QString("Launch Checklist"));
        QCOMPARE(TemplateEngine::process("[[safe_title]]", ctx), QString("Launch Checklist"));
        QCOMPARE(TemplateEngine::process("[[selection]]", ctx), QString("- [ ] Build"));
        QCOMPARE(TemplateEngine::process("[[line|3]]", ctx), QString("- [x] Test"));
        QCOMPARE(TemplateEngine::process("[[hashtags]]", ctx), QString("#ops #release"));
        QCOMPARE(TemplateEngine::process("[[clipboard]]", ctx), QString("https://omarchy.org"));
        QCOMPARE(TemplateEngine::process("[[[[title]]]]", ctx), QString("[[Launch Checklist]]"));
        QCOMPARE(TemplateEngine::process("[[date|%Y-%m-%d]]", ctx), QString("2026-11-23"));
        QCOMPARE(TemplateEngine::process("[[date|+1 day|%Y-%m-%d]]", ctx), QString("2026-11-24"));
        QCOMPARE(TemplateEngine::process("[[date|+1 year|%Y]]", ctx), QString("2027"));
        QCOMPARE(TemplateEngine::process("[[enc:title]]", ctx), QString("Launch%20Checklist"));
        QVERIFY(TemplateEngine::process("[[markdown]]", ctx).contains("<h1"));
    }

    void wikiLinksBacklinksAndMarkdown() {
        QTemporaryDir dir;
        Store store(dir.filePath("drafts.db"));
        DraftRecord target = store.createDraft("# Architecture\n## Storage\nDetails here");
        DraftRecord referrer = store.createDraft("# Index\nSee [[Architecture#Storage]] for DB info.");

        auto bl = store.backlinksFor(target.uuid);
        QCOMPARE(bl.size(), 1);
        QCOMPARE(bl.first().uuid, referrer.uuid);

        auto outline = MarkdownProcessor::extractOutline(target.content, "markdown");
        QCOMPARE(outline.size(), 2);
        QCOMPARE(outline[0].title, QString("Architecture"));
        QCOMPARE(outline[1].title, QString("Storage"));

        const QString linkHtml = MarkdownProcessor::toLinkModeHtml("- [ ] Check https://example.com and [[Architecture]]");
        QVERIFY(linkHtml.contains("drafts-task:0"));
        QVERIFY(linkHtml.contains("drafts-wiki:Architecture"));
        QVERIFY(linkHtml.contains("https://example.com"));
    }

    void actionEngineStepsAndJavaScriptSandbox() {
        QTemporaryDir dir;
        Store store(dir.filePath("drafts.db"));
        ActionEngine engine(store);
        QString fakeClip = "initial";
        engine.getClipboard = [&] { return fakeClip; };
        engine.setClipboard = [&](const QString &s) { fakeClip = s; };
        QString openedUrl;
        engine.openUrl = [&](const QString &u) { openedUrl = u; return true; };

        DraftRecord d = store.createDraft("# Hello\nworld", {"raw"});

        // Multi-step: JS script modifies draft + adds tag + clipboard step + archive afterSuccess
        ActionRecord act;
        act.id = "custom-test";
        act.name = "Transform & Copy";
        act.afterSuccess = "archive";
        act.steps = {
            {"script", {{"code",
                "draft.append('Appended via JS');\n"
                "draft.addTag('processed');\n"
                "var sh = ShellScript.create('printf \"SHELL_OK\"');\n"
                "sh.execute();\n"
                "draft.append(sh.stdout);\n"
                "draft.update();\n"
            }}},
            {"clipboard", {{"template", "[[title]]: [[tags]]"}, {"mode", "replace"}}},
        };

        ActionRunInput in;
        in.draft = d;
        const ActionRunResult res = engine.execute(act, in);
        QVERIFY(res.ok);
        QVERIFY(res.draftModified);
        QCOMPARE(res.updatedDraft.folder, QString("archive"));
        QVERIFY(res.updatedDraft.content.contains("Appended via JS"));
        QVERIFY(res.updatedDraft.content.contains("SHELL_OK"));
        QVERIFY(res.updatedDraft.tags.contains("processed"));
        QVERIFY(fakeClip.contains("Hello:"));
        QVERIFY(fakeClip.contains("processed"));

        // File step test
        ActionRecord fileAct;
        fileAct.id = "file-test";
        fileAct.name = "Write File";
        fileAct.steps = {{"file", {{"path", dir.filePath("[[safe_title]].md")}, {"template", "[[draft]]"}, {"mode", "write"}}}};
        const ActionRunResult fRes = engine.execute(fileAct, in);
        QVERIFY(fRes.ok);
        QVERIFY(QFile::exists(dir.filePath("Hello.md")));
    }

    void backendArrangeTasksUrlSchemeAndBackup() {
        QTemporaryDir dir;
        Backend backend(dir.filePath("drafts.db"), nullptr, false);

        backend.newDraft("# Plan\n- [ ] Task A\n- [ ] Task B");
        backend.toggleTaskAtLine(1);
        QVERIFY(backend.content().contains("- [x] Task A"));

        QStringList blocks = backend.splitForArrange("lines");
        QCOMPARE(blocks.size(), 3);
        backend.commitArrange({blocks[0], blocks[2], blocks[1]}, "lines");
        QCOMPARE(backend.content(), QString("# Plan\n- [ ] Task B\n- [x] Task A"));

        // URL scheme drafts://create
        QVERIFY(backend.handleDraftsUrl("drafts://create?text=Captured%20URL&tag=web,quick"));
        QCOMPARE(backend.content(), QString("Captured URL"));
        QVERIFY(backend.tags().contains("web"));
        QVERIFY(backend.tags().contains("quick"));

        // Export & import backup
        const QString backupPath = dir.filePath("backup.json");
        QVERIFY(backend.store().exportBackupJson(backupPath));

        Store store2(dir.filePath("restored.db"));
        int dCnt = 0, aCnt = 0;
        QVERIFY(store2.importBackupJson(backupPath, &dCnt, &aCnt));
        QVERIFY(dCnt >= 2);
        QVERIFY(aCnt >= 10);
    }

    void cliHeadlessWorkflow() {
        QTemporaryDir dir;
        const QString db = dir.filePath("cli.db");
        QCOMPARE(Cli::run({"drafts", "new", "--text", "# CLI Note\nBody", "--tag", "cli,work", "--json"}, db), 0);
        QCOMPARE(Cli::run({"drafts", "list", "--folder", "inbox", "--json"}, db), 0);
        QCOMPARE(Cli::run({"drafts", "append", "CLI Note", "More text", "--json"}, db), 0);
        QCOMPARE(Cli::run({"drafts", "search", "More text", "--json"}, db), 0);
        QCOMPARE(Cli::run({"drafts", "workspaces", "--json"}, db), 0);
        QCOMPARE(Cli::run({"drafts", "actions", "--json"}, db), 0);

        Store verify(db);
        DraftRecord found;
        QVERIFY(verify.findDraftByTitle("CLI Note", found));
        QVERIFY(found.content.contains("More text"));
    }

    void themeFallsBackAndReadsOmarchy() {
        QTemporaryDir dir;
        Theme fallbackTheme(dir.path());
        QCOMPARE(fallbackTheme.accent(), QString("#FFD60A"));
        QCOMPARE(fallbackTheme.accentForeground(), QString("black"));

        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\nbackground = \"#101012\"\n");
        Theme loaded(dir.path());
        QCOMPARE(loaded.accent(), QString("#112233"));
        QCOMPARE(loaded.accentForeground(), QString("white"));
        QCOMPARE(loaded.surface(), QString("#101012"));
    }

    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json");
        const QString command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":8}");
        QTRY_COMPARE(theme.radius(), 8);
    }

    void translatorLanguageAndPlurals() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());

        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    void qmlLoadsCleanlyOffscreen() {
        QTemporaryDir dir;
        QQuickStyle::setStyle("Material");
        qmlRegisterType<SyntaxHighlighterAdapter>("Drafts", 1, 0, "SyntaxHighlighter");

        Translator translator(dir.path() + "/language", ":/i18n");
        Theme theme(dir.path());
        Backend backend(dir.filePath("ui.db"), nullptr, true);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(win != nullptr);
        QTest::qWait(100);
        QImage img = win->grabWindow();
        QVERIFY(!img.isNull());
        QVERIFY(img.width() >= 760);
    }

private:
    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(DraftsTests)
#include "drafts_tests.moc"
