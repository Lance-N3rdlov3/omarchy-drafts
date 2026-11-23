#include "cli.h"
#include "actionengine.h"
#include "store.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <cstdio>
#include <unistd.h>

namespace {

void printJson(const QJsonValue &val) {
    QJsonDocument doc = val.isArray() ? QJsonDocument(val.toArray()) : QJsonDocument(val.toObject());
    std::fputs(doc.toJson(QJsonDocument::Indented).constData(), stdout);
}

void printError(const QString &msg, bool json) {
    if (json) {
        QJsonObject err{{"ok", false}, {"error", msg}};
        std::fputs(QJsonDocument(err).toJson(QJsonDocument::Compact).constData(), stderr);
        std::fputc('\n', stderr);
    } else {
        std::fprintf(stderr, "error: %s\n", msg.toUtf8().constData());
    }
}

bool resolveDraft(Store &store, const QString &idOrTitle, DraftRecord &out) {
    if (store.getDraft(idOrTitle, out))
        return true;
    return store.findDraftByTitle(idOrTitle, out);
}

int printHelp() {
    const char *help =
        "drafts — Where text starts, for Omarchy Linux.\n\n"
        "Usage:\n"
        "  drafts [open [UUID|drafts://...]]      Open the Drafts GUI window\n"
        "  drafts capture                         Open floating Quick Capture window\n"
        "  drafts new [--text TEXT] [--tag T1,T2] [--folder F] [--flag] [--syntax S] [--json]\n"
        "  drafts list [--folder F] [--workspace W] [--tag T] [--query Q] [--json]\n"
        "  drafts get <UUID|TITLE> [--json]\n"
        "  drafts append <UUID|TITLE> <TEXT> [--json]\n"
        "  drafts prepend <UUID|TITLE> <TEXT> [--json]\n"
        "  drafts search <QUERY> [--json]\n"
        "  drafts run <ACTION> <UUID|TITLE> [--json]\n"
        "  drafts workspaces [--json]\n"
        "  drafts actions [--json]\n"
        "  drafts export <FILE.json> [--json]\n"
        "  drafts import <FILE.json> [--json]\n"
        "  drafts open-url <drafts://...> [--json]\n"
        "  drafts skill install [--json]\n";
    std::fputs(help, stdout);
    return 0;
}

} // namespace

namespace Cli {

bool isHeadlessCommand(const QStringList &args) {
    if (args.size() < 2)
        return false;
    const QString cmd = args[1];
    static const QStringList headless = {
        "new", "list", "get", "append", "prepend", "search", "run",
        "workspaces", "actions", "export", "import", "open-url", "skill",
        "--help", "-h", "help", "--version", "-v"
    };
    return headless.contains(cmd);
}

int run(const QStringList &args, const QString &dbPath) {
    if (args.size() < 2)
        return printHelp();

    const QString cmd = args[1];
    const bool json = args.contains("--json");

    if (cmd == "--help" || cmd == "-h" || cmd == "help")
        return printHelp();

    if (cmd == "--version" || cmd == "-v") {
        if (json)
            printJson(QJsonObject{{"name", "drafts"}, {"version", "0.1.0"}});
        else
            std::puts("drafts 0.1.0");
        return 0;
    }

    if (cmd == "skill") {
        if (args.size() >= 3 && args[2] == "install") {
            QFile res(":/skill/SKILL.md");
            if (!res.open(QIODevice::ReadOnly)) {
                printError("Bundled skill missing from binary", json);
                return 1;
            }
            const QString targetDir = QDir::homePath() + "/.agents/skills/drafts";
            QDir().mkpath(targetDir);
            const QString targetPath = targetDir + "/SKILL.md";
            QSaveFile out(targetPath);
            if (!out.open(QIODevice::WriteOnly) || out.write(res.readAll()) < 0 || !out.commit()) {
                printError("Failed to write skill file: " + out.errorString(), json);
                return 1;
            }
            if (json)
                printJson(QJsonObject{{"ok", true}, {"path", targetPath}});
            else
                std::printf("Installed skill to %s\n", targetPath.toUtf8().constData());
            return 0;
        }
        printError("Usage: drafts skill install", json);
        return 1;
    }

    Store store(dbPath);
    if (!store.isOpen()) {
        printError("Failed to open database: " + store.lastError(), json);
        return 1;
    }

    auto optionValue = [&](const QString &flag) -> QString {
        const int idx = args.indexOf(flag);
        if (idx >= 0 && idx + 1 < args.size())
            return args[idx + 1];
        return {};
    };

    if (cmd == "new") {
        QString text = optionValue("--text");
        if (text.isEmpty() && !isatty(fileno(stdin))) {
            QTextStream ts(stdin);
            text = ts.readAll();
        }
        QStringList tags;
        const QString tagOpt = optionValue("--tag");
        if (!tagOpt.isEmpty())
            tags = tagOpt.split(',', Qt::SkipEmptyParts);
        const QString folder = optionValue("--folder").isEmpty() ? QStringLiteral("inbox") : optionValue("--folder");
        const QString syntax = optionValue("--syntax").isEmpty() ? QStringLiteral("markdown") : optionValue("--syntax");
        const bool flagged = args.contains("--flag");

        DraftRecord d = store.createDraft(text, tags, folder, syntax, flagged);
        if (json)
            printJson(d.toJson());
        else
            std::printf("%s\n", d.uuid.toUtf8().constData());
        return 0;
    }

    if (cmd == "list" || cmd == "search") {
        DraftQuery dq;
        dq.folder = optionValue("--folder");
        if (dq.folder.isEmpty())
            dq.folder = (cmd == "search") ? QStringLiteral("all") : QStringLiteral("inbox");

        const QString wsName = optionValue("--workspace");
        if (!wsName.isEmpty()) {
            WorkspaceRecord ws;
            if (!store.findWorkspaceByName(wsName, ws)) {
                printError("Workspace not found: " + wsName, json);
                return 1;
            }
            dq.folder = optionValue("--folder").isEmpty() ? ws.defaultFolder : optionValue("--folder");
            dq.sortField = ws.sortField;
            dq.sortDescending = ws.sortDescending;
            dq.showFlaggedFirst = ws.showFlaggedFirst;
            dq.tagMode = ws.tagFilterMode;
            dq.query = ws.query;
            for (QString p : ws.tagFilter.split(',', Qt::SkipEmptyParts)) {
                p = p.trimmed();
                if (p.startsWith('-'))
                    dq.omitTags.append(Store::normalizeTag(p.mid(1)));
                else if (!p.isEmpty())
                    dq.includeTags.append(Store::normalizeTag(p));
            }
        }

        const QString tagOpt = optionValue("--tag");
        if (!tagOpt.isEmpty()) {
            for (const QString &t : tagOpt.split(',', Qt::SkipEmptyParts))
                dq.includeTags.append(Store::normalizeTag(t));
        }

        if (cmd == "search") {
            if (args.size() < 3) {
                printError("Usage: drafts search <QUERY>", json);
                return 1;
            }
            dq.query = args[2];
        } else if (!optionValue("--query").isEmpty()) {
            dq.query = optionValue("--query");
        }

        const auto drafts = store.queryDrafts(dq);
        if (json) {
            QJsonArray arr;
            for (const DraftRecord &d : drafts)
                arr.append(d.toJson());
            printJson(arr);
        } else {
            for (const DraftRecord &d : drafts) {
                std::printf("%s\t[%s]%s\t%s\n",
                            d.uuid.toUtf8().constData(),
                            d.folder.toUtf8().constData(),
                            d.flagged ? " *" : "",
                            d.displayTitle().toUtf8().constData());
            }
        }
        return 0;
    }

    if (cmd == "get") {
        if (args.size() < 3) {
            printError("Usage: drafts get <UUID|TITLE>", json);
            return 1;
        }
        DraftRecord d;
        if (!resolveDraft(store, args[2], d)) {
            printError("Draft not found: " + args[2], json);
            return 1;
        }
        if (json)
            printJson(d.toJson());
        else
            std::fputs((d.content + "\n").toUtf8().constData(), stdout);
        return 0;
    }

    if (cmd == "append" || cmd == "prepend") {
        if (args.size() < 4) {
            printError(QStringLiteral("Usage: drafts %1 <UUID|TITLE> <TEXT>").arg(cmd), json);
            return 1;
        }
        DraftRecord d;
        if (!resolveDraft(store, args[2], d)) {
            printError("Draft not found: " + args[2], json);
            return 1;
        }
        d.content = (cmd == "append") ? (d.content + "\n" + args[3]) : (args[3] + "\n" + d.content);
        store.updateDraft(d, true);
        store.getDraft(d.uuid, d);
        if (json)
            printJson(d.toJson());
        else
            std::printf("Updated %s\n", d.uuid.toUtf8().constData());
        return 0;
    }

    if (cmd == "run") {
        if (args.size() < 4) {
            printError("Usage: drafts run <ACTION> <UUID|TITLE>", json);
            return 1;
        }
        ActionRecord action;
        if (!store.getAction(args[2], action) && !store.findActionByName(args[2], action)) {
            printError("Action not found: " + args[2], json);
            return 1;
        }
        DraftRecord d;
        if (!resolveDraft(store, args[3], d)) {
            printError("Draft not found: " + args[3], json);
            return 1;
        }
        ActionEngine engine(store);
        ActionRunInput in;
        in.draft = d;
        const ActionRunResult res = engine.execute(action, in);
        if (json) {
            printJson(QJsonObject{
                {"ok", res.ok},
                {"status", res.status},
                {"message", res.message},
                {"draft", res.updatedDraft.toJson()},
                {"html_preview", res.htmlPreview},
            });
        } else {
            std::printf("%s: %s\n", res.status.toUtf8().constData(), res.message.toUtf8().constData());
        }
        return res.ok ? 0 : 1;
    }

    if (cmd == "workspaces") {
        const auto list = store.listWorkspaces();
        if (json) {
            QJsonArray arr;
            for (const WorkspaceRecord &ws : list)
                arr.append(ws.toJson());
            printJson(arr);
        } else {
            for (const WorkspaceRecord &ws : list)
                std::printf("%s\t%s\t%s\n", ws.id.toUtf8().constData(), ws.name.toUtf8().constData(),
                            ws.tagFilter.toUtf8().constData());
        }
        return 0;
    }

    if (cmd == "actions") {
        const auto list = store.listActions();
        if (json) {
            QJsonArray arr;
            for (const ActionRecord &a : list)
                arr.append(a.toJson());
            printJson(arr);
        } else {
            for (const ActionRecord &a : list)
                std::printf("%s\t%s\t%s\n", a.id.toUtf8().constData(), a.name.toUtf8().constData(),
                            a.description.toUtf8().constData());
        }
        return 0;
    }

    if (cmd == "export") {
        if (args.size() < 3) {
            printError("Usage: drafts export <FILE.json>", json);
            return 1;
        }
        QString err;
        if (!store.exportBackupJson(args[2], &err)) {
            printError("Export failed: " + err, json);
            return 1;
        }
        if (json)
            printJson(QJsonObject{{"ok", true}, {"path", args[2]}});
        else
            std::printf("Exported backup to %s\n", args[2].toUtf8().constData());
        return 0;
    }

    if (cmd == "import") {
        if (args.size() < 3) {
            printError("Usage: drafts import <FILE.json>", json);
            return 1;
        }
        int dCnt = 0, aCnt = 0;
        QString err;
        if (!store.importBackupJson(args[2], &dCnt, &aCnt, &err)) {
            printError("Import failed: " + err, json);
            return 1;
        }
        if (json)
            printJson(QJsonObject{{"ok", true}, {"drafts", dCnt}, {"actions", aCnt}});
        else
            std::printf("Imported %d draft(s) and %d action(s)\n", dCnt, aCnt);
        return 0;
    }

    if (cmd == "open-url") {
        if (args.size() < 3) {
            printError("Usage: drafts open-url <drafts://...>", json);
            return 1;
        }
        const QUrl url(args[2]);
        if (url.scheme().compare("drafts", Qt::CaseInsensitive) != 0) {
            printError("Not a drafts:// URL", json);
            return 1;
        }
        const QString host = url.host().toLower();
        const QUrlQuery q(url);
        if (host == "create") {
            QStringList tags = q.queryItemValue("tag", QUrl::FullyDecoded).split(',', Qt::SkipEmptyParts);
            DraftRecord d = store.createDraft(q.queryItemValue("text", QUrl::FullyDecoded), tags);
            if (json)
                printJson(d.toJson());
            else
                std::printf("%s\n", d.uuid.toUtf8().constData());
            return 0;
        }
        if (host == "append" || host == "prepend") {
            DraftRecord d;
            if (!store.getDraft(q.queryItemValue("uuid", QUrl::FullyDecoded), d)) {
                printError("Draft not found", json);
                return 1;
            }
            const QString txt = q.queryItemValue("text", QUrl::FullyDecoded);
            d.content = (host == "append") ? (d.content + "\n" + txt) : (txt + "\n" + d.content);
            store.updateDraft(d, true);
            if (json)
                printJson(d.toJson());
            return 0;
        }
        printError("Unsupported headless URL action: " + host, json);
        return 1;
    }

    printError("Unknown command: " + cmd, json);
    return 1;
}

} // namespace Cli
