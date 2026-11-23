#include "actionengine.h"
#include "markdown.h"
#include "templateengine.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {

QString expandHome(const QString &path) {
    if (path == "~")
        return QDir::homePath();
    if (path.startsWith("~/"))
        return QDir::homePath() + path.mid(1);
    return path;
}

// ---------------------------------------------------------------------------
// QJSEngine scripting bridge objects (Drafts-compatible JS API)
// ---------------------------------------------------------------------------

class JsContextObject : public QObject {
    Q_OBJECT
public:
    bool cancelled = false;
    bool failed = false;
    QString failMessage;

    Q_INVOKABLE void cancel(const QString &msg = {}) {
        cancelled = true;
        if (!msg.isEmpty())
            failMessage = msg;
    }
    Q_INVOKABLE void fail(const QString &msg = {}) {
        failed = true;
        failMessage = msg.isEmpty() ? QStringLiteral("Action failed") : msg;
    }
};

class JsDraftObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString uuid READ uuid CONSTANT)
    Q_PROPERTY(QString content READ content WRITE setContent)
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QString displayTitle READ displayTitle)
    Q_PROPERTY(QStringList tags READ tags WRITE setTags)
    Q_PROPERTY(bool isFlagged READ isFlagged WRITE setIsFlagged)
    Q_PROPERTY(QString flagColor READ flagColor WRITE setFlagColor)
    Q_PROPERTY(bool isArchived READ isArchived WRITE setIsArchived)
    Q_PROPERTY(bool isTrashed READ isTrashed WRITE setIsTrashed)
    Q_PROPERTY(QString syntax READ syntax WRITE setSyntax)

public:
    JsDraftObject(Store &store, DraftRecord &record, bool &modifiedFlag,
                  const QString &accent, std::function<QString()> clipFn, QObject *parent = nullptr)
        : QObject(parent), m_store(store), m_record(record), m_modified(modifiedFlag),
          m_accent(accent), m_clipFn(std::move(clipFn)) {}

    QString uuid() const { return m_record.uuid; }
    QString content() const { return m_record.content; }
    void setContent(const QString &c) {
        m_record.content = c;
        m_modified = true;
    }
    QString title() const { return m_record.title(); }
    QString displayTitle() const { return m_record.displayTitle(); }
    QStringList tags() const { return m_record.tags; }
    void setTags(const QStringList &t) {
        m_record.tags = t;
        m_modified = true;
    }
    bool isFlagged() const { return m_record.flagged; }
    void setIsFlagged(bool f) {
        m_record.flagged = f;
        m_modified = true;
    }
    QString flagColor() const { return m_record.flagColor; }
    void setFlagColor(const QString &c) {
        m_record.flagColor = c;
        m_modified = true;
    }
    bool isArchived() const { return m_record.folder == "archive"; }
    void setIsArchived(bool a) {
        m_record.folder = a ? "archive" : "inbox";
        m_modified = true;
    }
    bool isTrashed() const { return m_record.folder == "trash"; }
    void setIsTrashed(bool t) {
        m_record.folder = t ? "trash" : "inbox";
        m_modified = true;
    }
    QString syntax() const { return m_record.syntax; }
    void setSyntax(const QString &s) {
        m_record.syntax = s;
        m_modified = true;
    }

    Q_INVOKABLE void addTag(const QString &tag) {
        const QString norm = Store::normalizeTag(tag);
        if (norm.isEmpty())
            return;
        const int scopeIdx = norm.indexOf("::");
        if (scopeIdx > 0) {
            const QString prefix = norm.left(scopeIdx + 2);
            for (int i = m_record.tags.size() - 1; i >= 0; --i) {
                if (m_record.tags[i].startsWith(prefix))
                    m_record.tags.removeAt(i);
            }
        }
        if (!m_record.tags.contains(norm))
            m_record.tags.append(norm);
        m_modified = true;
    }

    Q_INVOKABLE void removeTag(const QString &tag) {
        m_record.tags.removeAll(Store::normalizeTag(tag));
        m_modified = true;
    }

    Q_INVOKABLE bool hasTag(const QString &tag) const {
        const QString norm = Store::normalizeTag(tag);
        for (const QString &t : m_record.tags) {
            if (Store::tagMatchesFilter(t, norm))
                return true;
        }
        return false;
    }

    Q_INVOKABLE void append(const QString &text, const QString &separator = "\n") {
        if (!m_record.content.isEmpty() && !separator.isEmpty())
            m_record.content += separator;
        m_record.content += text;
        m_modified = true;
    }

    Q_INVOKABLE void prepend(const QString &text, const QString &separator = "\n") {
        if (!m_record.content.isEmpty() && !separator.isEmpty())
            m_record.content = text + separator + m_record.content;
        else
            m_record.content = text + m_record.content;
        m_modified = true;
    }

    Q_INVOKABLE QString processTemplate(const QString &tmpl) const {
        TemplateContext tctx;
        tctx.draft = m_record;
        tctx.clipboard = m_clipFn ? m_clipFn() : QString();
        tctx.accentColor = m_accent;
        return TemplateEngine::process(tmpl, tctx);
    }

    Q_INVOKABLE void update() {
        m_store.updateDraft(m_record, true);
        m_modified = true;
    }

    Q_INVOKABLE void saveVersion() {
        m_store.saveVersion(m_record.uuid, m_record.content, false);
    }

private:
    Store &m_store;
    DraftRecord &m_record;
    bool &m_modified;
    QString m_accent;
    std::function<QString()> m_clipFn;
};

class JsEditorObject : public QObject {
    Q_OBJECT
public:
    JsEditorObject(DraftRecord &record, int &selStart, int &selLen, bool &modifiedFlag,
                   ActionRunResult &res, QObject *parent = nullptr)
        : QObject(parent), m_record(record), m_selStart(selStart), m_selLen(selLen),
          m_modified(modifiedFlag), m_res(res) {}

    Q_INVOKABLE QString getText() const { return m_record.content; }
    Q_INVOKABLE void setText(const QString &text) {
        m_record.content = text;
        m_selStart = qBound(0, m_selStart, text.size());
        m_selLen = 0;
        m_modified = true;
    }

    Q_INVOKABLE QString getSelectedText() const {
        const int s = qBound(0, m_selStart, m_record.content.size());
        const int l = qBound(0, m_selLen, m_record.content.size() - s);
        return m_record.content.mid(s, l);
    }

    Q_INVOKABLE void setSelectedText(const QString &text) {
        const int s = qBound(0, m_selStart, m_record.content.size());
        const int l = qBound(0, m_selLen, m_record.content.size() - s);
        m_record.content.replace(s, l, text);
        m_selStart = s + text.size();
        m_selLen = 0;
        m_modified = true;
    }

    Q_INVOKABLE QVariantList getSelectedRange() const {
        return {m_selStart, m_selLen};
    }

    Q_INVOKABLE void setSelectedRange(int start, int length) {
        m_selStart = qBound(0, start, m_record.content.size());
        m_selLen = qBound(0, length, m_record.content.size() - m_selStart);
    }

    Q_INVOKABLE QVariantList getSelectedLineRange() const {
        const int pos = qBound(0, m_selStart, m_record.content.size());
        int lineStart = m_record.content.lastIndexOf('\n', pos > 0 ? pos - 1 : 0);
        lineStart = (lineStart < 0) ? 0 : lineStart + 1;
        const int selEnd = qBound(pos, pos + m_selLen, m_record.content.size());
        int lineEnd = m_record.content.indexOf('\n', selEnd);
        if (lineEnd < 0)
            lineEnd = m_record.content.size();
        return {lineStart, lineEnd - lineStart};
    }

    Q_INVOKABLE QString getTextInRange(int start, int length) const {
        const int s = qBound(0, start, m_record.content.size());
        const int l = qBound(0, length, m_record.content.size() - s);
        return m_record.content.mid(s, l);
    }

    Q_INVOKABLE void setTextInRange(int start, int length, const QString &text) {
        const int s = qBound(0, start, m_record.content.size());
        const int l = qBound(0, length, m_record.content.size() - s);
        m_record.content.replace(s, l, text);
        m_selStart = s + text.size();
        m_selLen = 0;
        m_modified = true;
    }

    Q_INVOKABLE void newDraft() { m_res.newDraftRequested = true; }
    Q_INVOKABLE void load(const QString &uuid) { m_res.loadDraftUuid = uuid; }
    Q_INVOKABLE void activate() {}

private:
    DraftRecord &m_record;
    int &m_selStart;
    int &m_selLen;
    bool &m_modified;
    ActionRunResult &m_res;
};

class JsAppObject : public QObject {
    Q_OBJECT
public:
    JsAppObject(std::function<QString()> getClip, std::function<void(const QString &)> setClip,
                std::function<bool(const QString &)> openUrlFn, ActionRunResult &res,
                QObject *parent = nullptr)
        : QObject(parent), m_getClip(std::move(getClip)), m_setClip(std::move(setClip)),
          m_openUrl(std::move(openUrlFn)), m_res(res) {}

    Q_INVOKABLE QString getClipboard() const { return m_getClip ? m_getClip() : QString(); }
    Q_INVOKABLE void setClipboard(const QString &text) {
        if (m_setClip)
            m_setClip(text);
    }
    Q_INVOKABLE bool openURL(const QString &url) {
        return m_openUrl ? m_openUrl(url) : false;
    }
    Q_INVOKABLE void displayInfoMessage(const QString &msg) {
        m_res.message = msg;
        m_res.logs.append("[info] " + msg);
    }
    Q_INVOKABLE void displaySuccessMessage(const QString &msg) {
        m_res.message = msg;
        m_res.logs.append("[ok] " + msg);
    }
    Q_INVOKABLE void displayErrorMessage(const QString &msg) {
        m_res.message = msg;
        m_res.logs.append("[error] " + msg);
    }
    Q_INVOKABLE void displayWarningMessage(const QString &msg) {
        m_res.message = msg;
        m_res.logs.append("[warn] " + msg);
    }

private:
    std::function<QString()> m_getClip;
    std::function<void(const QString &)> m_setClip;
    std::function<bool(const QString &)> m_openUrl;
    ActionRunResult &m_res;
};

class JsDraftStaticObject : public QObject {
    Q_OBJECT
public:
    explicit JsDraftStaticObject(Store &store, QObject *parent = nullptr)
        : QObject(parent), m_store(store) {}

    Q_INVOKABLE QVariantList query(const QString &queryString = {}, const QString &filter = "all",
                                   const QStringList &tags = {}, const QStringList &omitTags = {}) const {
        DraftQuery dq;
        dq.folder = filter.isEmpty() ? QStringLiteral("all") : filter;
        dq.query = queryString;
        dq.includeTags = tags;
        dq.omitTags = omitTags;
        const auto rows = m_store.queryDrafts(dq);
        QVariantList list;
        for (const DraftRecord &d : rows)
            list.append(d.toVariantMap());
        return list;
    }

    Q_INVOKABLE QVariantMap create(const QString &content = {}, const QStringList &tags = {}) {
        DraftRecord d = m_store.createDraft(content, tags);
        return d.toVariantMap();
    }

    Q_INVOKABLE QVariantMap find(const QString &uuid) const {
        DraftRecord d;
        if (m_store.getDraft(uuid, d))
            return d.toVariantMap();
        return {};
    }

private:
    Store &m_store;
};

class JsShellHelper : public QObject {
    Q_OBJECT
public:
    explicit JsShellHelper(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantMap run(const QString &script, const QStringList &args = {},
                                const QString &stdinText = {}) const {
        QProcess proc;
        QStringList procArgs = {"-c", script, "drafts-script"};
        procArgs.append(args);
        proc.start("/bin/sh", procArgs);
        if (!proc.waitForStarted(3000)) {
            return {{"ok", false}, {"exitCode", -1}, {"stdout", ""}, {"stderr", proc.errorString()}};
        }
        if (!stdinText.isEmpty())
            proc.write(stdinText.toUtf8());
        proc.closeWriteChannel();
        if (!proc.waitForFinished(15000)) {
            proc.kill();
            proc.waitForFinished(1000);
            return {{"ok", false}, {"exitCode", -1}, {"stdout", ""}, {"stderr", "Command timed out"}};
        }
        return {
            {"ok", proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0},
            {"exitCode", proc.exitCode()},
            {"stdout", QString::fromUtf8(proc.readAllStandardOutput())},
            {"stderr", QString::fromUtf8(proc.readAllStandardError())},
        };
    }
};

class JsFileManagerHelper : public QObject {
    Q_OBJECT
public:
    explicit JsFileManagerHelper(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QString readString(const QString &path) const {
        QFile f(expandHome(path));
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(f.readAll());
    }

    Q_INVOKABLE bool writeString(const QString &path, const QString &content) const {
        const QString full = expandHome(path);
        QDir().mkpath(QFileInfo(full).absolutePath());
        QSaveFile f(full);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        f.write(content.toUtf8());
        return f.commit();
    }

    Q_INVOKABLE QStringList listContents(const QString &path) const {
        return QDir(expandHome(path)).entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
    }
};

class JsHttpHelper : public QObject {
    Q_OBJECT
public:
    explicit JsHttpHelper(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantMap request(const QVariantMap &opts) const {
        const QString urlStr = opts.value("url").toString();
        const QString method = opts.value("method", "GET").toString().toUpper();
        if (urlStr.isEmpty())
            return {{"success", false}, {"statusCode", 0}, {"error", "Missing URL"}};

        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl{urlStr});
        const QVariantMap headers = opts.value("headers").toMap();
        for (auto it = headers.begin(); it != headers.end(); ++it)
            req.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());

        const QByteArray payload = opts.value("data").toString().toUtf8();
        QNetworkReply *reply = nullptr;
        if (method == "POST")
            reply = nam.post(req, payload);
        else if (method == "PUT")
            reply = nam.put(req, payload);
        else if (method == "DELETE")
            reply = nam.deleteResource(req);
        else
            reply = nam.get(req);

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timeout.start(10000);
        loop.exec();

        if (!reply->isFinished()) {
            reply->abort();
            reply->deleteLater();
            return {{"success", false}, {"statusCode", 0}, {"error", "Request timed out"}};
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString body = QString::fromUtf8(reply->readAll());
        const bool ok = (reply->error() == QNetworkReply::NoError);
        const QString err = ok ? QString() : reply->errorString();
        reply->deleteLater();
        return {
            {"success", ok},
            {"statusCode", statusCode},
            {"responseText", body},
            {"error", err},
        };
    }
};

class JsConsoleObject : public QObject {
    Q_OBJECT
public:
    explicit JsConsoleObject(QStringList &logs, QObject *parent = nullptr)
        : QObject(parent), m_logs(logs) {}

    Q_INVOKABLE void log(const QString &msg) { m_logs.append(msg); }
    Q_INVOKABLE void warn(const QString &msg) { m_logs.append("[warn] " + msg); }
    Q_INVOKABLE void error(const QString &msg) { m_logs.append("[error] " + msg); }

private:
    QStringList &m_logs;
};

} // namespace

// ---------------------------------------------------------------------------
// ActionEngine
// ---------------------------------------------------------------------------

ActionEngine::ActionEngine(Store &store) : m_store(store) {}

QString ActionEngine::readClipboard() const {
    if (getClipboard)
        return getClipboard();
    if (auto *cb = QGuiApplication::clipboard())
        return cb->text();
    return {};
}

void ActionEngine::writeClipboard(const QString &text) const {
    if (setClipboard) {
        setClipboard(text);
        return;
    }
    if (auto *cb = QGuiApplication::clipboard())
        cb->setText(text);
}

bool ActionEngine::launchUrl(const QString &url) const {
    if (openUrl)
        return openUrl(url);
    return QDesktopServices::openUrl(QUrl(url));
}

bool ActionEngine::runScriptStep(const QString &code, DraftRecord &draft, int &selStart, int &selLen,
                                 const QString &accentColor, ActionRunResult &res) {
    QJSEngine engine;
    engine.installExtensions(QJSEngine::ConsoleExtension);

    JsContextObject ctxObj;
    bool modified = false;
    JsDraftObject draftObj(m_store, draft, modified, accentColor, [this] { return readClipboard(); });
    JsEditorObject editorObj(draft, selStart, selLen, modified, res);
    JsAppObject appObj(
        [this] { return readClipboard(); },
        [this](const QString &s) { writeClipboard(s); },
        [this](const QString &u) { return launchUrl(u); },
        res);
    JsDraftStaticObject draftStatic(m_store);
    JsShellHelper shellHelper;
    JsFileManagerHelper fileHelper;
    JsHttpHelper httpHelper;
    JsConsoleObject consoleObj(res.logs);

    auto g = engine.globalObject();
    g.setProperty("context", engine.newQObject(&ctxObj));
    g.setProperty("draft", engine.newQObject(&draftObj));
    g.setProperty("editor", engine.newQObject(&editorObj));
    g.setProperty("app", engine.newQObject(&appObj));
    g.setProperty("Draft", engine.newQObject(&draftStatic));
    g.setProperty("_shell", engine.newQObject(&shellHelper));
    g.setProperty("FileManager", engine.newQObject(&fileHelper));
    g.setProperty("_http", engine.newQObject(&httpHelper));
    g.setProperty("console", engine.newQObject(&consoleObj));

    // Provide Drafts-idiomatic ShellScript.create() & HTTP.create() wrappers
    engine.evaluate(R"(
        var ShellScript = {
            create: function(script) {
                return {
                    script: script,
                    stdout: "",
                    stderr: "",
                    exitCode: 0,
                    execute: function(args, stdinText) {
                        var r = _shell.run(this.script, args || [], stdinText || "");
                        this.stdout = r.stdout;
                        this.stderr = r.stderr;
                        this.exitCode = r.exitCode;
                        return r.ok;
                    }
                };
            }
        };
        var HTTP = {
            create: function() {
                return {
                    request: function(opts) {
                        return _http.request(opts || {});
                    }
                };
            }
        };
    )");

    const QJSValue val = engine.evaluate(code);
    if (modified)
        res.draftModified = true;

    if (val.isError()) {
        res.ok = false;
        res.status = "error";
        res.message = QStringLiteral("Script error (line %1): %2")
                          .arg(val.property("lineNumber").toInt())
                          .arg(val.toString());
        return false;
    }
    if (ctxObj.failed) {
        res.ok = false;
        res.status = "error";
        res.message = ctxObj.failMessage;
        return false;
    }
    if (ctxObj.cancelled) {
        res.ok = true;
        res.cancelled = true;
        res.status = "cancelled";
        res.message = ctxObj.failMessage.isEmpty() ? QStringLiteral("Action cancelled") : ctxObj.failMessage;
        return false;
    }
    return true;
}

ActionRunResult ActionEngine::execute(const ActionRecord &action, const ActionRunInput &input) {
    ActionRunResult res;
    DraftRecord draft = input.draft;
    int selStart = qBound(0, input.selectionStart, draft.content.size());
    int selLen = qBound(0, input.selectionLength, draft.content.size() - selStart);

    auto makeTemplateCtx = [&]() {
        TemplateContext tctx;
        tctx.draft = draft;
        tctx.selectionStart = selStart;
        tctx.selectionLength = selLen;
        tctx.clipboard = readClipboard();
        tctx.accentColor = input.accentColor;
        return tctx;
    };

    for (const ActionStep &step : action.steps) {
        const QString &type = step.type;
        const QVariantMap &cfg = step.config;

        if (type == "clipboard") {
            const QString tmpl = cfg.value("template", "[[draft]]").toString();
            const QString mode = cfg.value("mode", "replace").toString();
            const QString rendered = TemplateEngine::process(tmpl, makeTemplateCtx());
            if (mode == "append") {
                const QString existing = readClipboard();
                writeClipboard(existing.isEmpty() ? rendered : existing + "\n" + rendered);
            } else if (mode == "prepend") {
                const QString existing = readClipboard();
                writeClipboard(existing.isEmpty() ? rendered : rendered + "\n" + existing);
            } else {
                writeClipboard(rendered);
            }
            if (res.message.isEmpty())
                res.message = QStringLiteral("Copied to clipboard");

        } else if (type == "file") {
            const QString rawPath = TemplateEngine::process(
                cfg.value("path", "~/Documents/Drafts/[[safe_title]].md").toString(), makeTemplateCtx());
            const QString path = expandHome(rawPath);
            const QString mode = cfg.value("mode", "write").toString();
            const QString rendered = TemplateEngine::process(
                cfg.value("template", "[[draft]]").toString(), makeTemplateCtx());

            QDir().mkpath(QFileInfo(path).absolutePath());
            bool ok = false;
            QString err;
            if (mode == "append") {
                QFile f(path);
                ok = f.open(QIODevice::WriteOnly | QIODevice::Append);
                if (ok)
                    f.write(rendered.toUtf8());
                else
                    err = f.errorString();
            } else if (mode == "prepend") {
                QByteArray prev;
                QFile rf(path);
                if (rf.open(QIODevice::ReadOnly))
                    prev = rf.readAll();
                rf.close();
                QSaveFile wf(path);
                ok = wf.open(QIODevice::WriteOnly);
                if (ok) {
                    wf.write(rendered.toUtf8());
                    wf.write(prev);
                    ok = wf.commit();
                }
                if (!ok)
                    err = wf.errorString();
            } else {
                QSaveFile wf(path);
                ok = wf.open(QIODevice::WriteOnly);
                if (ok) {
                    wf.write(rendered.toUtf8());
                    ok = wf.commit();
                }
                if (!ok)
                    err = wf.errorString();
            }

            if (!ok) {
                res.ok = false;
                res.status = "error";
                res.message = QStringLiteral("File step failed: %1").arg(err);
                break;
            }
            res.message = QStringLiteral("Saved to %1").arg(path);

        } else if (type == "url") {
            const QString url = TemplateEngine::process(cfg.value("template").toString(), makeTemplateCtx());
            if (url.isEmpty() || (input.allowDesktopSideEffects && !launchUrl(url))) {
                res.ok = false;
                res.status = "error";
                res.message = QStringLiteral("Failed to open URL: %1").arg(url);
                break;
            }
            res.message = QStringLiteral("Opened %1").arg(url);

        } else if (type == "mail") {
            const auto tctx = makeTemplateCtx();
            const QString to = TemplateEngine::process(cfg.value("to").toString(), tctx);
            const QString subject = TemplateEngine::process(cfg.value("subject", "[[title]]").toString(), tctx);
            const QString body = TemplateEngine::process(cfg.value("body", "[[body]]").toString(), tctx);
            QUrl mailUrl;
            mailUrl.setScheme("mailto");
            mailUrl.setPath(to);
            QUrlQuery query;
            if (!subject.isEmpty())
                query.addQueryItem("subject", subject);
            if (!body.isEmpty())
                query.addQueryItem("body", body);
            mailUrl.setQuery(query);
            if (input.allowDesktopSideEffects)
                launchUrl(mailUrl.toString());
            res.message = QStringLiteral("Opened mail composer");

        } else if (type == "insert_text") {
            const QString rendered = TemplateEngine::process(cfg.value("template").toString(), makeTemplateCtx());
            draft.content.replace(selStart, selLen, rendered);
            selStart += rendered.size();
            selLen = 0;
            res.draftModified = true;

        } else if (type == "shell") {
            const QString script = TemplateEngine::process(cfg.value("script").toString(), makeTemplateCtx());
            const QString outMode = cfg.value("outputMode", "none").toString();
            QProcess proc;
            proc.start("/bin/sh", {"-c", script});
            if (!proc.waitForStarted(3000)) {
                res.ok = false;
                res.status = "error";
                res.message = QStringLiteral("Shell failed to start: %1").arg(proc.errorString());
                break;
            }
            proc.write(draft.content.toUtf8());
            proc.closeWriteChannel();
            if (!proc.waitForFinished(15000)) {
                proc.kill();
                proc.waitForFinished(1000);
                res.ok = false;
                res.status = "error";
                res.message = QStringLiteral("Shell command timed out");
                break;
            }
            const QString stdoutStr = QString::fromUtf8(proc.readAllStandardOutput());
            const QString stderrStr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
                res.ok = false;
                res.status = "error";
                res.message = stderrStr.isEmpty()
                                  ? QStringLiteral("Shell exited with code %1").arg(proc.exitCode())
                                  : stderrStr;
                break;
            }
            res.logs.append(stdoutStr);
            if (outMode == "replace") {
                draft.content = stdoutStr;
                res.draftModified = true;
            } else if (outMode == "append") {
                if (!stdoutStr.isEmpty()) {
                    if (!draft.content.endsWith('\n'))
                        draft.content += '\n';
                    draft.content += "\n---\n" + stdoutStr;
                    res.draftModified = true;
                }
            } else if (outMode == "prepend") {
                if (!stdoutStr.isEmpty()) {
                    draft.content = stdoutStr + "\n" + draft.content;
                    res.draftModified = true;
                }
            } else if (outMode == "insert") {
                draft.content.replace(selStart, selLen, stdoutStr);
                selStart += stdoutStr.size();
                selLen = 0;
                res.draftModified = true;
            } else if (outMode == "clipboard") {
                writeClipboard(stdoutStr);
            }
            if (res.message.isEmpty())
                res.message = stdoutStr.trimmed().isEmpty()
                                  ? QStringLiteral("Shell command succeeded")
                                  : stdoutStr.trimmed().left(120);

        } else if (type == "html_preview") {
            const QString tmpl = cfg.value("template", "[[markdown]]").toString();
            res.htmlPreview = TemplateEngine::process(tmpl, makeTemplateCtx());
            res.message = QStringLiteral("Rendered HTML preview");

        } else if (type == "script") {
            const QString code = cfg.value("code").toString();
            if (!runScriptStep(code, draft, selStart, selLen, input.accentColor, res))
                break;
        }
    }

    if (res.ok && !res.cancelled) {
        if (action.afterSuccess == "archive") {
            draft.folder = "archive";
            res.draftModified = true;
        } else if (action.afterSuccess == "trash") {
            draft.folder = "trash";
            res.draftModified = true;
        }
        if (res.draftModified && !draft.uuid.isEmpty()) {
            m_store.updateDraft(draft, true);
        }
        if (res.message.isEmpty())
            res.message = QStringLiteral("Ran %1").arg(action.name);
    }

    res.updatedDraft = draft;
    res.newSelectionStart = selStart;
    res.newSelectionLength = selLen;

    m_store.recordActionLog(action.id, action.name, draft.uuid, res.status,
                            res.message + (res.logs.isEmpty() ? QString() : "\n" + res.logs.join('\n')));
    return res;
}

#include "actionengine.moc"
