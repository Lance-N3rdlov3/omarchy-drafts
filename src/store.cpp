#include "store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <atomic>

namespace {
std::atomic<int> g_connectionCounter{0};

QString newUuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
}
} // namespace

// ---------------------------------------------------------------------------
// DraftRecord helpers
// ---------------------------------------------------------------------------

QString DraftRecord::title() const {
    const QStringList lines = content.split('\n');
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        static const QRegularExpression headingPrefix(R"(^#{1,6}\s+)");
        trimmed.remove(headingPrefix);
        return trimmed.trimmed();
    }
    return {};
}

QString DraftRecord::displayTitle() const {
    const QString t = title();
    return t.isEmpty() ? QStringLiteral("Untitled Draft") : t;
}

QString DraftRecord::safeTitle() const {
    QString t = displayTitle();
    static const QRegularExpression unsafe(R"([\\/:*?"<>|\r\n\t]+)");
    t.replace(unsafe, "-");
    t = t.trimmed();
    if (t.size() > 80)
        t = t.left(80).trimmed();
    return t.isEmpty() ? QStringLiteral("draft") : t;
}

QString DraftRecord::body() const {
    const int idx = content.indexOf('\n');
    if (idx < 0)
        return {};
    QString rest = content.mid(idx + 1);
    while (rest.startsWith('\n'))
        rest.remove(0, 1);
    return rest;
}

QString DraftRecord::excerpt() const {
    const QStringList lines = content.split('\n');
    bool skippedFirst = false;
    QStringList parts;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (!skippedFirst) {
            skippedFirst = true;
            continue;
        }
        parts.append(trimmed);
        if (parts.join(' ').size() >= 140)
            break;
    }
    QString out = parts.join(' ');
    if (out.size() > 140)
        out = out.left(137) + "...";
    return out;
}

int DraftRecord::wordCount() const {
    static const QRegularExpression wordRe(R"(\S+)");
    int count = 0;
    auto it = wordRe.globalMatch(content);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

int DraftRecord::charCount() const {
    return content.size();
}

int DraftRecord::lineCount() const {
    if (content.isEmpty())
        return 0;
    return content.count('\n') + 1;
}

QVariantMap DraftRecord::toVariantMap() const {
    return {
        {"uuid", uuid},
        {"content", content},
        {"title", displayTitle()},
        {"rawTitle", title()},
        {"excerpt", excerpt()},
        {"folder", folder},
        {"flagged", flagged},
        {"flagColor", flagColor},
        {"syntax", syntax},
        {"tags", tags},
        {"createdAt", createdAt},
        {"modifiedAt", modifiedAt},
        {"accessedAt", accessedAt},
        {"cursorPos", cursorPos},
        {"wordCount", wordCount()},
        {"charCount", charCount()},
        {"lineCount", lineCount()},
    };
}

QJsonObject DraftRecord::toJson() const {
    QJsonArray tagArr;
    for (const QString &t : tags)
        tagArr.append(t);
    return QJsonObject{
        {"uuid", uuid},
        {"content", content},
        {"title", displayTitle()},
        {"folder", folder},
        {"flagged", flagged},
        {"flag_color", flagColor},
        {"syntax", syntax},
        {"tags", tagArr},
        {"created_at", createdAt},
        {"modified_at", modifiedAt},
        {"accessed_at", accessedAt},
        {"cursor_pos", cursorPos},
    };
}

DraftRecord DraftRecord::fromJson(const QJsonObject &obj) {
    DraftRecord d;
    d.uuid = obj.value("uuid").toString();
    if (d.uuid.isEmpty())
        d.uuid = newUuid();
    d.content = obj.value("content").toString();
    d.folder = obj.value("folder").toString("inbox");
    d.flagged = obj.value("flagged").toBool(false);
    d.flagColor = obj.value("flag_color").toString("orange");
    d.syntax = obj.value("syntax").toString("markdown");
    const QJsonArray tagArr = obj.value("tags").toArray();
    for (const QJsonValue &v : tagArr) {
        const QString norm = Store::normalizeTag(v.toString());
        if (!norm.isEmpty() && !d.tags.contains(norm))
            d.tags.append(norm);
    }
    const QString now = Store::nowIso();
    d.createdAt = obj.value("created_at").toString(now);
    d.modifiedAt = obj.value("modified_at").toString(now);
    d.accessedAt = obj.value("accessed_at").toString(now);
    d.cursorPos = obj.value("cursor_pos").toInt(0);
    return d;
}

// ---------------------------------------------------------------------------
// VersionRecord, WorkspaceRecord, ActionStep, ActionRecord, ActionGroupRecord
// ---------------------------------------------------------------------------

QVariantMap VersionRecord::toVariantMap() const {
    QString preview = content.left(180).replace('\n', ' ');
    return {
        {"id", id},
        {"draftUuid", draftUuid},
        {"content", content},
        {"preview", preview},
        {"charCount", content.size()},
        {"createdAt", createdAt},
    };
}

QJsonObject VersionRecord::toJson() const {
    return QJsonObject{
        {"id", id},
        {"draft_uuid", draftUuid},
        {"content", content},
        {"created_at", createdAt},
    };
}

QVariantMap WorkspaceRecord::toVariantMap() const {
    return {
        {"id", id},
        {"name", name},
        {"tagFilter", tagFilter},
        {"tagFilterMode", tagFilterMode},
        {"query", query},
        {"sortField", sortField},
        {"sortDescending", sortDescending},
        {"showFlaggedFirst", showFlaggedFirst},
        {"defaultFolder", defaultFolder},
        {"actionGroupId", actionGroupId},
        {"actionBarGroupId", actionBarGroupId},
        {"position", position},
    };
}

QJsonObject WorkspaceRecord::toJson() const {
    return QJsonObject{
        {"id", id},
        {"name", name},
        {"tag_filter", tagFilter},
        {"tag_filter_mode", tagFilterMode},
        {"query", query},
        {"sort_field", sortField},
        {"sort_descending", sortDescending},
        {"show_flagged_first", showFlaggedFirst},
        {"default_folder", defaultFolder},
        {"action_group_id", actionGroupId},
        {"action_bar_group_id", actionBarGroupId},
        {"position", position},
    };
}

WorkspaceRecord WorkspaceRecord::fromJson(const QJsonObject &obj) {
    WorkspaceRecord ws;
    ws.id = obj.value("id").toString();
    ws.name = obj.value("name").toString();
    ws.tagFilter = obj.value("tag_filter").toString();
    ws.tagFilterMode = obj.value("tag_filter_mode").toString("all");
    ws.query = obj.value("query").toString();
    ws.sortField = obj.value("sort_field").toString("modified");
    ws.sortDescending = obj.value("sort_descending").toBool(true);
    ws.showFlaggedFirst = obj.value("show_flagged_first").toBool(true);
    ws.defaultFolder = obj.value("default_folder").toString("inbox");
    ws.actionGroupId = obj.value("action_group_id").toString();
    ws.actionBarGroupId = obj.value("action_bar_group_id").toString();
    ws.position = obj.value("position").toInt(0);
    return ws;
}

QVariantMap ActionStep::toVariantMap() const {
    return {
        {"type", type},
        {"config", config},
    };
}

QJsonObject ActionStep::toJson() const {
    return QJsonObject{
        {"type", type},
        {"config", QJsonObject::fromVariantMap(config)},
    };
}

ActionStep ActionStep::fromJson(const QJsonObject &obj) {
    ActionStep step;
    step.type = obj.value("type").toString();
    step.config = obj.value("config").toObject().toVariantMap();
    return step;
}

QVariantMap ActionRecord::toVariantMap() const {
    QVariantList stepList;
    for (const ActionStep &s : steps)
        stepList.append(s.toVariantMap());
    return {
        {"id", id},
        {"groupId", groupId},
        {"name", name},
        {"description", description},
        {"icon", icon},
        {"tint", tint},
        {"shortcut", shortcut},
        {"afterSuccess", afterSuccess},
        {"steps", stepList},
        {"position", position},
    };
}

QJsonObject ActionRecord::toJson() const {
    QJsonArray stepArr;
    for (const ActionStep &s : steps)
        stepArr.append(s.toJson());
    return QJsonObject{
        {"id", id},
        {"group_id", groupId},
        {"name", name},
        {"description", description},
        {"icon", icon},
        {"tint", tint},
        {"shortcut", shortcut},
        {"after_success", afterSuccess},
        {"steps", stepArr},
        {"position", position},
    };
}

ActionRecord ActionRecord::fromJson(const QJsonObject &obj) {
    ActionRecord a;
    a.id = obj.value("id").toString();
    a.groupId = obj.value("group_id").toString();
    a.name = obj.value("name").toString();
    a.description = obj.value("description").toString();
    a.icon = obj.value("icon").toString("⚡");
    a.tint = obj.value("tint").toString();
    a.shortcut = obj.value("shortcut").toString();
    a.afterSuccess = obj.value("after_success").toString("nothing");
    a.position = obj.value("position").toInt(0);
    const QJsonArray arr = obj.value("steps").toArray();
    for (const QJsonValue &v : arr)
        a.steps.append(ActionStep::fromJson(v.toObject()));
    return a;
}

QVariantMap ActionGroupRecord::toVariantMap() const {
    return {
        {"id", id},
        {"name", name},
        {"icon", icon},
        {"position", position},
    };
}

QJsonObject ActionGroupRecord::toJson() const {
    return QJsonObject{
        {"id", id},
        {"name", name},
        {"icon", icon},
        {"position", position},
    };
}

QVariantMap ActionLogRecord::toVariantMap() const {
    return {
        {"id", id},
        {"actionId", actionId},
        {"actionName", actionName},
        {"draftUuid", draftUuid},
        {"status", status},
        {"message", message},
        {"createdAt", createdAt},
    };
}

// ---------------------------------------------------------------------------
// Store lifecycle
// ---------------------------------------------------------------------------

QString Store::defaultDbPath() {
    const QString envPath = qEnvironmentVariable("DRAFTS_DB_PATH");
    if (!envPath.isEmpty())
        return envPath;
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base + "/drafts/drafts.db";
}

QString Store::nowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

Store::Store(const QString &dbPath)
    : m_dbPath(dbPath.isEmpty() ? defaultDbPath() : dbPath),
      m_connectionName(QStringLiteral("drafts_conn_%1").arg(++g_connectionCounter)) {
    const QFileInfo fi(m_dbPath);
    QDir().mkpath(fi.absolutePath());

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return;
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA foreign_keys=ON");
    pragma.exec("PRAGMA synchronous=NORMAL");

    m_open = initSchema();
    if (m_open)
        seedDefaultData();
}

Store::~Store() {
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Store::initSchema() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);

    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS drafts (
            uuid TEXT PRIMARY KEY,
            content TEXT NOT NULL DEFAULT '',
            folder TEXT NOT NULL DEFAULT 'inbox',
            flagged INTEGER NOT NULL DEFAULT 0,
            flag_color TEXT NOT NULL DEFAULT 'orange',
            syntax TEXT NOT NULL DEFAULT 'markdown',
            created_at TEXT NOT NULL,
            modified_at TEXT NOT NULL,
            accessed_at TEXT NOT NULL,
            cursor_pos INTEGER NOT NULL DEFAULT 0
        ))",
        R"(CREATE INDEX IF NOT EXISTS idx_drafts_folder_mod ON drafts(folder, modified_at DESC))",
        R"(CREATE TABLE IF NOT EXISTS draft_tags (
            draft_uuid TEXT NOT NULL,
            tag TEXT NOT NULL,
            PRIMARY KEY (draft_uuid, tag),
            FOREIGN KEY (draft_uuid) REFERENCES drafts(uuid) ON DELETE CASCADE
        ))",
        R"(CREATE INDEX IF NOT EXISTS idx_draft_tags_tag ON draft_tags(tag))",
        R"(CREATE TABLE IF NOT EXISTS versions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            draft_uuid TEXT NOT NULL,
            content TEXT NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (draft_uuid) REFERENCES drafts(uuid) ON DELETE CASCADE
        ))",
        R"(CREATE INDEX IF NOT EXISTS idx_versions_draft ON versions(draft_uuid, id DESC))",
        R"(CREATE TABLE IF NOT EXISTS workspaces (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            tag_filter TEXT NOT NULL DEFAULT '',
            tag_filter_mode TEXT NOT NULL DEFAULT 'all',
            query TEXT NOT NULL DEFAULT '',
            sort_field TEXT NOT NULL DEFAULT 'modified',
            sort_descending INTEGER NOT NULL DEFAULT 1,
            show_flagged_first INTEGER NOT NULL DEFAULT 1,
            default_folder TEXT NOT NULL DEFAULT 'inbox',
            action_group_id TEXT NOT NULL DEFAULT '',
            action_bar_group_id TEXT NOT NULL DEFAULT '',
            position INTEGER NOT NULL DEFAULT 0
        ))",
        R"(CREATE TABLE IF NOT EXISTS action_groups (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            icon TEXT NOT NULL DEFAULT '⚡',
            position INTEGER NOT NULL DEFAULT 0
        ))",
        R"(CREATE TABLE IF NOT EXISTS actions (
            id TEXT PRIMARY KEY,
            group_id TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT '',
            icon TEXT NOT NULL DEFAULT '⚡',
            tint TEXT NOT NULL DEFAULT '',
            shortcut TEXT NOT NULL DEFAULT '',
            after_success TEXT NOT NULL DEFAULT 'nothing',
            steps_json TEXT NOT NULL DEFAULT '[]',
            position INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY (group_id) REFERENCES action_groups(id) ON DELETE CASCADE
        ))",
        R"(CREATE TABLE IF NOT EXISTS action_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            action_id TEXT NOT NULL,
            action_name TEXT NOT NULL,
            draft_uuid TEXT NOT NULL,
            status TEXT NOT NULL,
            message TEXT NOT NULL DEFAULT '',
            created_at TEXT NOT NULL
        ))",
        R"(CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        ))",
    };

    for (const QString &sql : statements) {
        if (!q.exec(sql)) {
            m_lastError = q.lastError().text();
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Tags
// ---------------------------------------------------------------------------

QString Store::normalizeTag(const QString &raw) {
    QString t = raw.trimmed().toLower();
    while (t.startsWith('#'))
        t.remove(0, 1);
    t.replace(QRegularExpression(R"(\s+)"), "-");
    t.remove(',');
    return t.trimmed();
}

bool Store::tagMatchesFilter(const QString &draftTag, const QString &filterTag) {
    const QString dt = normalizeTag(draftTag);
    const QString ft = normalizeTag(filterTag);
    if (ft.isEmpty())
        return true;
    if (dt == ft)
        return true;
    // Nested tags: filtering by "work" matches "work/project" and "work/project/q4".
    if (dt.startsWith(ft + "/"))
        return true;
    return false;
}

QStringList Store::tagsForDraft(const QString &uuid) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT tag FROM draft_tags WHERE draft_uuid = ? ORDER BY tag ASC");
    q.addBindValue(uuid);
    QStringList result;
    if (q.exec()) {
        while (q.next())
            result.append(q.value(0).toString());
    }
    return result;
}

bool Store::addTagToDraft(const QString &uuid, const QString &rawTag) {
    const QString tag = normalizeTag(rawTag);
    if (tag.isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    // Scoped tags (e.g. "status::done") replace any existing tag sharing the same "status::" prefix.
    const int scopeIdx = tag.indexOf("::");
    if (scopeIdx > 0) {
        const QString prefix = tag.left(scopeIdx + 2);
        QSqlQuery del(db);
        del.prepare("DELETE FROM draft_tags WHERE draft_uuid = ? AND tag LIKE ?");
        del.addBindValue(uuid);
        del.addBindValue(prefix + "%");
        del.exec();
    }

    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO draft_tags (draft_uuid, tag) VALUES (?, ?)");
    q.addBindValue(uuid);
    q.addBindValue(tag);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool Store::removeTagFromDraft(const QString &uuid, const QString &tag) {
    const QString norm = normalizeTag(tag);
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM draft_tags WHERE draft_uuid = ? AND tag = ?");
    q.addBindValue(uuid);
    q.addBindValue(norm);
    return q.exec();
}

bool Store::setDraftTags(const QString &uuid, const QStringList &tags) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery del(db);
    del.prepare("DELETE FROM draft_tags WHERE draft_uuid = ?");
    del.addBindValue(uuid);
    if (!del.exec())
        return false;
    for (const QString &t : tags)
        addTagToDraft(uuid, t);
    return true;
}

QVariantList Store::allTagsWithCounts(const QString &folder) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    if (folder == "all" || folder.isEmpty()) {
        q.prepare(R"(
            SELECT dt.tag, COUNT(*) as cnt
            FROM draft_tags dt
            JOIN drafts d ON d.uuid = dt.draft_uuid
            WHERE d.folder != 'trash'
            GROUP BY dt.tag
            ORDER BY dt.tag ASC
        )");
    } else if (folder == "flagged") {
        q.prepare(R"(
            SELECT dt.tag, COUNT(*) as cnt
            FROM draft_tags dt
            JOIN drafts d ON d.uuid = dt.draft_uuid
            WHERE d.folder != 'trash' AND d.flagged = 1
            GROUP BY dt.tag
            ORDER BY dt.tag ASC
        )");
    } else {
        q.prepare(R"(
            SELECT dt.tag, COUNT(*) as cnt
            FROM draft_tags dt
            JOIN drafts d ON d.uuid = dt.draft_uuid
            WHERE d.folder = ?
            GROUP BY dt.tag
            ORDER BY dt.tag ASC
        )");
        q.addBindValue(folder);
    }

    QVariantList result;
    if (q.exec()) {
        while (q.next()) {
            const QString tag = q.value(0).toString();
            const int count = q.value(1).toInt();
            const int depth = tag.count('/');
            const int scopeIdx = tag.indexOf("::");
            result.append(QVariantMap{
                {"name", tag},
                {"count", count},
                {"depth", depth},
                {"isScoped", scopeIdx > 0},
                {"scope", scopeIdx > 0 ? tag.left(scopeIdx) : QString()},
            });
        }
    }
    return result;
}

bool Store::renameTagGlobally(const QString &oldTag, const QString &newTag) {
    const QString oldNorm = normalizeTag(oldTag);
    const QString newNorm = normalizeTag(newTag);
    if (oldNorm.isEmpty() || newNorm.isEmpty() || oldNorm == newNorm)
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery find(db);
    find.prepare("SELECT draft_uuid FROM draft_tags WHERE tag = ?");
    find.addBindValue(oldNorm);
    QStringList uuids;
    if (find.exec()) {
        while (find.next())
            uuids.append(find.value(0).toString());
    }
    for (const QString &uuid : uuids) {
        removeTagFromDraft(uuid, oldNorm);
        addTagToDraft(uuid, newNorm);
    }
    return db.commit();
}

bool Store::deleteTagGlobally(const QString &tag) {
    const QString norm = normalizeTag(tag);
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM draft_tags WHERE tag = ?");
    q.addBindValue(norm);
    return q.exec();
}

// ---------------------------------------------------------------------------
// Drafts CRUD
// ---------------------------------------------------------------------------

DraftRecord Store::createDraft(const QString &content, const QStringList &tags,
                               const QString &folder, const QString &syntax,
                               bool flagged, const QString &flagColor) {
    DraftRecord d;
    d.uuid = newUuid();
    d.content = content;
    d.folder = folder.isEmpty() ? QStringLiteral("inbox") : folder;
    d.flagged = flagged;
    d.flagColor = flagColor.isEmpty() ? QStringLiteral("orange") : flagColor;
    d.syntax = syntax.isEmpty() ? QStringLiteral("markdown") : syntax;
    const QString now = nowIso();
    d.createdAt = now;
    d.modifiedAt = now;
    d.accessedAt = now;
    d.cursorPos = content.size();

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO drafts (uuid, content, folder, flagged, flag_color, syntax, created_at, modified_at, accessed_at, cursor_pos)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(d.uuid);
    q.addBindValue(d.content);
    q.addBindValue(d.folder);
    q.addBindValue(d.flagged ? 1 : 0);
    q.addBindValue(d.flagColor);
    q.addBindValue(d.syntax);
    q.addBindValue(d.createdAt);
    q.addBindValue(d.modifiedAt);
    q.addBindValue(d.accessedAt);
    q.addBindValue(d.cursorPos);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return {};
    }

    for (const QString &tag : tags)
        addTagToDraft(d.uuid, tag);
    d.tags = tagsForDraft(d.uuid);
    return d;
}

bool Store::getDraft(const QString &uuid, DraftRecord &out) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT uuid, content, folder, flagged, flag_color, syntax, created_at, modified_at, accessed_at, cursor_pos
        FROM drafts WHERE uuid = ?
    )");
    q.addBindValue(uuid);
    if (!q.exec() || !q.next())
        return false;
    out.uuid = q.value(0).toString();
    out.content = q.value(1).toString();
    out.folder = q.value(2).toString();
    out.flagged = q.value(3).toInt() != 0;
    out.flagColor = q.value(4).toString();
    out.syntax = q.value(5).toString();
    out.createdAt = q.value(6).toString();
    out.modifiedAt = q.value(7).toString();
    out.accessedAt = q.value(8).toString();
    out.cursorPos = q.value(9).toInt();
    out.tags = tagsForDraft(out.uuid);
    return true;
}

bool Store::findDraftByTitle(const QString &title, DraftRecord &out) const {
    const QString target = title.trimmed().toLower();
    if (target.isEmpty())
        return false;

    DraftQuery dq;
    dq.folder = "all";
    const auto all = queryDrafts(dq);
    // Exact title match first, then prefix match.
    for (const DraftRecord &d : all) {
        if (d.title().toLower() == target) {
            out = d;
            return true;
        }
    }
    for (const DraftRecord &d : all) {
        if (d.title().toLower().startsWith(target)) {
            out = d;
            return true;
        }
    }
    return false;
}

bool Store::updateDraft(const DraftRecord &draft, bool touchModified) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const QString now = nowIso();
    const QString modAt = touchModified ? now : draft.modifiedAt;
    QSqlQuery q(db);
    q.prepare(R"(
        UPDATE drafts
        SET content = ?, folder = ?, flagged = ?, flag_color = ?, syntax = ?,
            modified_at = ?, accessed_at = ?, cursor_pos = ?
        WHERE uuid = ?
    )");
    q.addBindValue(draft.content);
    q.addBindValue(draft.folder);
    q.addBindValue(draft.flagged ? 1 : 0);
    q.addBindValue(draft.flagColor);
    q.addBindValue(draft.syntax);
    q.addBindValue(modAt);
    q.addBindValue(now);
    q.addBindValue(draft.cursorPos);
    q.addBindValue(draft.uuid);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return setDraftTags(draft.uuid, draft.tags);
}

bool Store::touchAccessed(const QString &uuid) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("UPDATE drafts SET accessed_at = ? WHERE uuid = ?");
    q.addBindValue(nowIso());
    q.addBindValue(uuid);
    return q.exec();
}

bool Store::setDraftFolder(const QString &uuid, const QString &folder) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("UPDATE drafts SET folder = ?, modified_at = ? WHERE uuid = ?");
    q.addBindValue(folder);
    q.addBindValue(nowIso());
    q.addBindValue(uuid);
    return q.exec();
}

bool Store::setDraftFlagged(const QString &uuid, bool flagged, const QString &flagColor) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    if (!flagColor.isEmpty()) {
        q.prepare("UPDATE drafts SET flagged = ?, flag_color = ?, modified_at = ? WHERE uuid = ?");
        q.addBindValue(flagged ? 1 : 0);
        q.addBindValue(flagColor);
        q.addBindValue(nowIso());
        q.addBindValue(uuid);
    } else {
        q.prepare("UPDATE drafts SET flagged = ?, modified_at = ? WHERE uuid = ?");
        q.addBindValue(flagged ? 1 : 0);
        q.addBindValue(nowIso());
        q.addBindValue(uuid);
    }
    return q.exec();
}

bool Store::setDraftSyntax(const QString &uuid, const QString &syntax) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("UPDATE drafts SET syntax = ?, modified_at = ? WHERE uuid = ?");
    q.addBindValue(syntax);
    q.addBindValue(nowIso());
    q.addBindValue(uuid);
    return q.exec();
}

bool Store::setDraftCursor(const QString &uuid, int cursorPos) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("UPDATE drafts SET cursor_pos = ? WHERE uuid = ?");
    q.addBindValue(cursorPos);
    q.addBindValue(uuid);
    return q.exec();
}

bool Store::deleteDraftPermanently(const QString &uuid) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM drafts WHERE uuid = ?");
    q.addBindValue(uuid);
    return q.exec();
}

int Store::emptyTrash() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec("DELETE FROM drafts WHERE folder = 'trash'");
    return q.numRowsAffected();
}

// ---------------------------------------------------------------------------
// Query, Advanced Search & Backlinks
// ---------------------------------------------------------------------------

void Store::parseWorkspaceTags(const QString &tagFilter, QStringList &includeTags, QStringList &omitTags) {
    const QStringList parts = tagFilter.split(',', Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        if (part.isEmpty())
            continue;
        if (part.startsWith('-')) {
            const QString norm = normalizeTag(part.mid(1));
            if (!norm.isEmpty())
                omitTags.append(norm);
        } else {
            const QString norm = normalizeTag(part);
            if (!norm.isEmpty())
                includeTags.append(norm);
        }
    }
}

bool Store::matchesSearchQuery(const DraftRecord &draft, const QString &queryText) const {
    const QString trimmed = queryText.trimmed();
    if (trimmed.isEmpty())
        return true;

    // Regex search: /pattern/
    if (trimmed.size() >= 2 && trimmed.startsWith('/') && trimmed.endsWith('/')) {
        const QString pattern = trimmed.mid(1, trimmed.size() - 2);
        const QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
        if (re.isValid())
            return re.match(draft.content).hasMatch();
    }

    // Tokenize supporting "quoted phrases", -negations, tag:foo, -tag:foo, is:flagged
    static const QRegularExpression tokenRe(R"((-?"[^"]+"|\S+))");
    auto it = tokenRe.globalMatch(trimmed);
    const QString lowerContent = draft.content.toLower();

    while (it.hasNext()) {
        QString tok = it.next().captured(1);
        bool negate = false;
        if (tok.startsWith('-') && tok.size() > 1) {
            negate = true;
            tok = tok.mid(1);
        }

        bool matched = false;
        if (tok.startsWith("tag:", Qt::CaseInsensitive)) {
            const QString wanted = normalizeTag(tok.mid(4));
            for (const QString &t : draft.tags) {
                if (tagMatchesFilter(t, wanted)) {
                    matched = true;
                    break;
                }
            }
        } else if (tok.compare("is:flagged", Qt::CaseInsensitive) == 0) {
            matched = draft.flagged;
        } else if (tok.compare("is:untagged", Qt::CaseInsensitive) == 0) {
            matched = draft.tags.isEmpty();
        } else if (tok.startsWith("syntax:", Qt::CaseInsensitive)) {
            matched = (draft.syntax.compare(tok.mid(7), Qt::CaseInsensitive) == 0);
        } else {
            if (tok.startsWith('"') && tok.endsWith('"') && tok.size() >= 2)
                tok = tok.mid(1, tok.size() - 2);
            const QString needle = tok.toLower();
            matched = lowerContent.contains(needle);
            if (!matched) {
                for (const QString &t : draft.tags) {
                    if (t.contains(needle)) {
                        matched = true;
                        break;
                    }
                }
            }
        }

        if (negate ? matched : !matched)
            return false;
    }
    return true;
}

bool Store::matchesWorkspaceFilter(const DraftRecord &draft, const DraftQuery &q) const {
    // Folder filtering
    if (q.folder == "flagged") {
        if (draft.folder == "trash" || !draft.flagged)
            return false;
    } else if (q.folder == "all") {
        if (draft.folder == "trash")
            return false;
    } else if (!q.folder.isEmpty()) {
        if (draft.folder != q.folder)
            return false;
    }

    // Omit tags always exclude
    for (const QString &omit : q.omitTags) {
        if (omit == "untagged") {
            if (draft.tags.isEmpty())
                return false;
            continue;
        }
        for (const QString &dt : draft.tags) {
            if (tagMatchesFilter(dt, omit))
                return false;
        }
    }

    // Include tags (all or any)
    if (!q.includeTags.isEmpty()) {
        if (q.tagMode == "any") {
            bool anyMatch = false;
            for (const QString &inc : q.includeTags) {
                if (inc == "untagged" && draft.tags.isEmpty()) {
                    anyMatch = true;
                    break;
                }
                for (const QString &dt : draft.tags) {
                    if (tagMatchesFilter(dt, inc)) {
                        anyMatch = true;
                        break;
                    }
                }
                if (anyMatch)
                    break;
            }
            if (!anyMatch)
                return false;
        } else {
            for (const QString &inc : q.includeTags) {
                bool found = (inc == "untagged" && draft.tags.isEmpty());
                if (!found) {
                    for (const QString &dt : draft.tags) {
                        if (tagMatchesFilter(dt, inc)) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found)
                    return false;
            }
        }
    }

    return matchesSearchQuery(draft, q.query);
}

QVector<DraftRecord> Store::queryDrafts(const DraftQuery &dq) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT uuid, content, folder, flagged, flag_color, syntax, created_at, modified_at, accessed_at, cursor_pos
        FROM drafts
    )");

    QVector<DraftRecord> results;
    if (!q.exec())
        return results;

    while (q.next()) {
        DraftRecord d;
        d.uuid = q.value(0).toString();
        d.content = q.value(1).toString();
        d.folder = q.value(2).toString();
        d.flagged = q.value(3).toInt() != 0;
        d.flagColor = q.value(4).toString();
        d.syntax = q.value(5).toString();
        d.createdAt = q.value(6).toString();
        d.modifiedAt = q.value(7).toString();
        d.accessedAt = q.value(8).toString();
        d.cursorPos = q.value(9).toInt();
        d.tags = tagsForDraft(d.uuid);

        if (matchesWorkspaceFilter(d, dq))
            results.append(d);
    }

    std::sort(results.begin(), results.end(), [&](const DraftRecord &a, const DraftRecord &b) {
        if (dq.showFlaggedFirst && a.flagged != b.flagged)
            return a.flagged && !b.flagged;

        int cmp = 0;
        if (dq.sortField == "created")
            cmp = a.createdAt.compare(b.createdAt);
        else if (dq.sortField == "accessed")
            cmp = a.accessedAt.compare(b.accessedAt);
        else if (dq.sortField == "text")
            cmp = a.displayTitle().compare(b.displayTitle(), Qt::CaseInsensitive);
        else
            cmp = a.modifiedAt.compare(b.modifiedAt);

        if (cmp == 0)
            cmp = a.uuid.compare(b.uuid);
        return dq.sortDescending ? (cmp > 0) : (cmp < 0);
    });

    return results;
}

QVector<DraftRecord> Store::backlinksFor(const QString &uuid) const {
    DraftRecord target;
    if (!getDraft(uuid, target))
        return {};
    const QString title = target.title().trimmed();
    if (title.isEmpty())
        return {};

    // Match [[Title]] or [[Title#Heading]] or [[d:Title]]
    const QString escaped = QRegularExpression::escape(title);
    const QRegularExpression re(QStringLiteral(R"(\[\[(?:d:)?%1(?:#[^\]]+)?\]\])").arg(escaped),
                                QRegularExpression::CaseInsensitiveOption);

    DraftQuery dq;
    dq.folder = "all";
    const auto all = queryDrafts(dq);
    QVector<DraftRecord> links;
    for (const DraftRecord &d : all) {
        if (d.uuid == uuid)
            continue;
        if (re.match(d.content).hasMatch())
            links.append(d);
    }
    return links;
}

QVariantMap Store::folderCounts(const WorkspaceRecord *workspace) const {
    DraftQuery base;
    if (workspace) {
        parseWorkspaceTags(workspace->tagFilter, base.includeTags, base.omitTags);
        base.tagMode = workspace->tagFilterMode;
        base.query = workspace->query;
    }

    auto countFolder = [&](const QString &folder) {
        DraftQuery q = base;
        q.folder = folder;
        return queryDrafts(q).size();
    };

    return {
        {"inbox", countFolder("inbox")},
        {"flagged", countFolder("flagged")},
        {"archive", countFolder("archive")},
        {"all", countFolder("all")},
        {"trash", countFolder("trash")},
    };
}

// ---------------------------------------------------------------------------
// Version History
// ---------------------------------------------------------------------------

qint64 Store::saveVersion(const QString &uuid, const QString &content, bool skipIfUnchanged) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (skipIfUnchanged) {
        QSqlQuery last(db);
        last.prepare("SELECT content FROM versions WHERE draft_uuid = ? ORDER BY id DESC LIMIT 1");
        last.addBindValue(uuid);
        if (last.exec() && last.next() && last.value(0).toString() == content)
            return -1;
    }

    QSqlQuery q(db);
    q.prepare("INSERT INTO versions (draft_uuid, content, created_at) VALUES (?, ?, ?)");
    q.addBindValue(uuid);
    q.addBindValue(content);
    q.addBindValue(nowIso());
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

QVector<VersionRecord> Store::versionsForDraft(const QString &uuid) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT id, draft_uuid, content, created_at FROM versions WHERE draft_uuid = ? ORDER BY id DESC");
    q.addBindValue(uuid);
    QVector<VersionRecord> result;
    if (q.exec()) {
        while (q.next()) {
            VersionRecord v;
            v.id = q.value(0).toLongLong();
            v.draftUuid = q.value(1).toString();
            v.content = q.value(2).toString();
            v.createdAt = q.value(3).toString();
            result.append(v);
        }
    }
    return result;
}

bool Store::restoreVersion(const QString &uuid, qint64 versionId, DraftRecord &updated) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT content FROM versions WHERE id = ? AND draft_uuid = ?");
    q.addBindValue(versionId);
    q.addBindValue(uuid);
    if (!q.exec() || !q.next())
        return false;
    const QString oldContent = q.value(0).toString();

    DraftRecord current;
    if (!getDraft(uuid, current))
        return false;

    // Preserve the current content as a version before restoring so nothing is ever lost.
    saveVersion(uuid, current.content, true);
    current.content = oldContent;
    if (!updateDraft(current, true))
        return false;
    return getDraft(uuid, updated);
}

// ---------------------------------------------------------------------------
// Workspaces
// ---------------------------------------------------------------------------

QVector<WorkspaceRecord> Store::listWorkspaces() const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT id, name, tag_filter, tag_filter_mode, query, sort_field,
               sort_descending, show_flagged_first, default_folder,
               action_group_id, action_bar_group_id, position
        FROM workspaces ORDER BY position ASC, name ASC
    )");
    QVector<WorkspaceRecord> list;
    if (q.exec()) {
        while (q.next()) {
            WorkspaceRecord ws;
            ws.id = q.value(0).toString();
            ws.name = q.value(1).toString();
            ws.tagFilter = q.value(2).toString();
            ws.tagFilterMode = q.value(3).toString();
            ws.query = q.value(4).toString();
            ws.sortField = q.value(5).toString();
            ws.sortDescending = q.value(6).toInt() != 0;
            ws.showFlaggedFirst = q.value(7).toInt() != 0;
            ws.defaultFolder = q.value(8).toString();
            ws.actionGroupId = q.value(9).toString();
            ws.actionBarGroupId = q.value(10).toString();
            ws.position = q.value(11).toInt();
            list.append(ws);
        }
    }
    return list;
}

bool Store::getWorkspace(const QString &id, WorkspaceRecord &out) const {
    for (const WorkspaceRecord &ws : listWorkspaces()) {
        if (ws.id == id) {
            out = ws;
            return true;
        }
    }
    return false;
}

bool Store::findWorkspaceByName(const QString &name, WorkspaceRecord &out) const {
    for (const WorkspaceRecord &ws : listWorkspaces()) {
        if (ws.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0) {
            out = ws;
            return true;
        }
    }
    return false;
}

WorkspaceRecord Store::saveWorkspace(const WorkspaceRecord &input) {
    WorkspaceRecord ws = input;
    if (ws.id.isEmpty())
        ws.id = newUuid();

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT OR REPLACE INTO workspaces
        (id, name, tag_filter, tag_filter_mode, query, sort_field, sort_descending,
         show_flagged_first, default_folder, action_group_id, action_bar_group_id, position)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(ws.id);
    q.addBindValue(ws.name);
    q.addBindValue(ws.tagFilter);
    q.addBindValue(ws.tagFilterMode);
    q.addBindValue(ws.query);
    q.addBindValue(ws.sortField);
    q.addBindValue(ws.sortDescending ? 1 : 0);
    q.addBindValue(ws.showFlaggedFirst ? 1 : 0);
    q.addBindValue(ws.defaultFolder);
    q.addBindValue(ws.actionGroupId);
    q.addBindValue(ws.actionBarGroupId);
    q.addBindValue(ws.position);
    if (!q.exec())
        m_lastError = q.lastError().text();
    return ws;
}

bool Store::deleteWorkspace(const QString &id) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM workspaces WHERE id = ?");
    q.addBindValue(id);
    return q.exec();
}

// ---------------------------------------------------------------------------
// Action Groups & Actions
// ---------------------------------------------------------------------------

QVector<ActionGroupRecord> Store::listActionGroups() const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT id, name, icon, position FROM action_groups ORDER BY position ASC, name ASC");
    QVector<ActionGroupRecord> list;
    if (q.exec()) {
        while (q.next()) {
            ActionGroupRecord g;
            g.id = q.value(0).toString();
            g.name = q.value(1).toString();
            g.icon = q.value(2).toString();
            g.position = q.value(3).toInt();
            list.append(g);
        }
    }
    return list;
}

ActionGroupRecord Store::saveActionGroup(const ActionGroupRecord &input) {
    ActionGroupRecord g = input;
    if (g.id.isEmpty())
        g.id = newUuid();
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO action_groups (id, name, icon, position) VALUES (?, ?, ?, ?)");
    q.addBindValue(g.id);
    q.addBindValue(g.name);
    q.addBindValue(g.icon);
    q.addBindValue(g.position);
    q.exec();
    return g;
}

bool Store::deleteActionGroup(const QString &id) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM action_groups WHERE id = ?");
    q.addBindValue(id);
    return q.exec();
}

QVector<ActionRecord> Store::listActions(const QString &groupId) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    if (groupId.isEmpty()) {
        q.prepare(R"(
            SELECT id, group_id, name, description, icon, tint, shortcut, after_success, steps_json, position
            FROM actions ORDER BY position ASC, name ASC
        )");
    } else {
        q.prepare(R"(
            SELECT id, group_id, name, description, icon, tint, shortcut, after_success, steps_json, position
            FROM actions WHERE group_id = ? ORDER BY position ASC, name ASC
        )");
        q.addBindValue(groupId);
    }

    QVector<ActionRecord> list;
    if (q.exec()) {
        while (q.next()) {
            ActionRecord a;
            a.id = q.value(0).toString();
            a.groupId = q.value(1).toString();
            a.name = q.value(2).toString();
            a.description = q.value(3).toString();
            a.icon = q.value(4).toString();
            a.tint = q.value(5).toString();
            a.shortcut = q.value(6).toString();
            a.afterSuccess = q.value(7).toString();
            const QJsonArray arr = QJsonDocument::fromJson(q.value(8).toString().toUtf8()).array();
            for (const QJsonValue &v : arr)
                a.steps.append(ActionStep::fromJson(v.toObject()));
            a.position = q.value(9).toInt();
            list.append(a);
        }
    }
    return list;
}

bool Store::getAction(const QString &id, ActionRecord &out) const {
    for (const ActionRecord &a : listActions()) {
        if (a.id == id) {
            out = a;
            return true;
        }
    }
    return false;
}

bool Store::findActionByName(const QString &name, ActionRecord &out) const {
    const QString target = name.trimmed();
    for (const ActionRecord &a : listActions()) {
        if (a.name.compare(target, Qt::CaseInsensitive) == 0) {
            out = a;
            return true;
        }
    }
    return false;
}

ActionRecord Store::saveAction(const ActionRecord &input) {
    ActionRecord a = input;
    if (a.id.isEmpty())
        a.id = newUuid();
    QJsonArray stepArr;
    for (const ActionStep &s : a.steps)
        stepArr.append(s.toJson());
    const QString stepsJson = QString::fromUtf8(QJsonDocument(stepArr).toJson(QJsonDocument::Compact));

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT OR REPLACE INTO actions
        (id, group_id, name, description, icon, tint, shortcut, after_success, steps_json, position)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(a.id);
    q.addBindValue(a.groupId);
    q.addBindValue(a.name);
    q.addBindValue(a.description);
    q.addBindValue(a.icon);
    q.addBindValue(a.tint);
    q.addBindValue(a.shortcut);
    q.addBindValue(a.afterSuccess);
    q.addBindValue(stepsJson);
    q.addBindValue(a.position);
    if (!q.exec())
        m_lastError = q.lastError().text();
    return a;
}

bool Store::deleteAction(const QString &id) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM actions WHERE id = ?");
    q.addBindValue(id);
    return q.exec();
}

// ---------------------------------------------------------------------------
// Action Log
// ---------------------------------------------------------------------------

void Store::recordActionLog(const QString &actionId, const QString &actionName,
                            const QString &draftUuid, const QString &status, const QString &message) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO action_logs (action_id, action_name, draft_uuid, status, message, created_at)
        VALUES (?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(actionId);
    q.addBindValue(actionName);
    q.addBindValue(draftUuid);
    q.addBindValue(status);
    q.addBindValue(message);
    q.addBindValue(nowIso());
    q.exec();
}

QVector<ActionLogRecord> Store::recentActionLogs(int limit) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT id, action_id, action_name, draft_uuid, status, message, created_at FROM action_logs ORDER BY id DESC LIMIT ?");
    q.addBindValue(limit);
    QVector<ActionLogRecord> logs;
    if (q.exec()) {
        while (q.next()) {
            ActionLogRecord r;
            r.id = q.value(0).toLongLong();
            r.actionId = q.value(1).toString();
            r.actionName = q.value(2).toString();
            r.draftUuid = q.value(3).toString();
            r.status = q.value(4).toString();
            r.message = q.value(5).toString();
            r.createdAt = q.value(6).toString();
            logs.append(r);
        }
    }
    return logs;
}

void Store::clearActionLogs() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec("DELETE FROM action_logs");
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

QString Store::getSetting(const QString &key, const QString &defaultValue) const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return defaultValue;
}

void Store::setSetting(const QString &key, const QString &value) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

QVariantMap Store::allSettings() const {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    QVariantMap out;
    if (q.exec("SELECT key, value FROM settings")) {
        while (q.next())
            out.insert(q.value(0).toString(), q.value(1).toString());
    }
    return out;
}

// ---------------------------------------------------------------------------
// Backup, Export & Import (atomic writes)
// ---------------------------------------------------------------------------

bool Store::exportBackupJson(const QString &filePath, QString *errorOut) const {
    QJsonObject root;
    root.insert("format", "drafts-omarchy-backup");
    root.insert("version", 1);
    root.insert("exported_at", nowIso());

    // All drafts including trash
    QJsonArray draftsArr;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    if (q.exec("SELECT uuid FROM drafts ORDER BY modified_at DESC")) {
        while (q.next()) {
            DraftRecord d;
            if (getDraft(q.value(0).toString(), d))
                draftsArr.append(d.toJson());
        }
    }
    root.insert("drafts", draftsArr);

    QJsonArray wsArr;
    for (const WorkspaceRecord &ws : listWorkspaces())
        wsArr.append(ws.toJson());
    root.insert("workspaces", wsArr);

    QJsonArray grpArr;
    for (const ActionGroupRecord &g : listActionGroups())
        grpArr.append(g.toJson());
    root.insert("action_groups", grpArr);

    QJsonArray actArr;
    for (const ActionRecord &a : listActions())
        actArr.append(a.toJson());
    root.insert("actions", actArr);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    return true;
}

bool Store::importBackupJson(const QString &filePath, int *importedDrafts,
                             int *importedActions, QString *errorOut) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut)
            *errorOut = err.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    int draftCount = 0;
    int actionCount = 0;

    // Supports both our backup format and Drafts for Mac/iOS JSON export arrays!
    const QJsonArray draftsArr = root.value("drafts").toArray();
    for (const QJsonValue &v : draftsArr) {
        if (!v.isObject())
            continue;
        DraftRecord d = DraftRecord::fromJson(v.toObject());
        DraftRecord existing;
        if (getDraft(d.uuid, existing)) {
            updateDraft(d, false);
        } else {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            QSqlQuery ins(db);
            ins.prepare(R"(
                INSERT INTO drafts (uuid, content, folder, flagged, flag_color, syntax, created_at, modified_at, accessed_at, cursor_pos)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            ins.addBindValue(d.uuid);
            ins.addBindValue(d.content);
            ins.addBindValue(d.folder);
            ins.addBindValue(d.flagged ? 1 : 0);
            ins.addBindValue(d.flagColor);
            ins.addBindValue(d.syntax);
            ins.addBindValue(d.createdAt);
            ins.addBindValue(d.modifiedAt);
            ins.addBindValue(d.accessedAt);
            ins.addBindValue(d.cursorPos);
            if (ins.exec())
                setDraftTags(d.uuid, d.tags);
        }
        ++draftCount;
    }

    for (const QJsonValue &v : root.value("workspaces").toArray()) {
        if (v.isObject())
            saveWorkspace(WorkspaceRecord::fromJson(v.toObject()));
    }

    for (const QJsonValue &v : root.value("action_groups").toArray()) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        ActionGroupRecord g;
        g.id = o.value("id").toString();
        g.name = o.value("name").toString();
        g.icon = o.value("icon").toString("⚡");
        g.position = o.value("position").toInt();
        saveActionGroup(g);
    }

    for (const QJsonValue &v : root.value("actions").toArray()) {
        if (!v.isObject())
            continue;
        saveAction(ActionRecord::fromJson(v.toObject()));
        ++actionCount;
    }

    if (importedDrafts)
        *importedDrafts = draftCount;
    if (importedActions)
        *importedActions = actionCount;
    return true;
}

QString Store::exportActionJson(const QString &actionId) const {
    ActionRecord a;
    if (!getAction(actionId, a))
        return {};
    return QString::fromUtf8(QJsonDocument(a.toJson()).toJson(QJsonDocument::Indented));
}

bool Store::importActionJson(const QString &jsonText, const QString &targetGroupId,
                             ActionRecord &out, QString *errorOut) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut)
            *errorOut = err.errorString();
        return false;
    }
    ActionRecord a = ActionRecord::fromJson(doc.object());
    if (a.name.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Action JSON is missing a name.");
        return false;
    }
    a.id = newUuid();
    if (!targetGroupId.isEmpty())
        a.groupId = targetGroupId;
    if (a.groupId.isEmpty()) {
        const auto groups = listActionGroups();
        if (!groups.isEmpty())
            a.groupId = groups.first().id;
    }
    out = saveAction(a);
    return true;
}

// ---------------------------------------------------------------------------
// Seed built-in Workspaces, Action Groups & Actions
// ---------------------------------------------------------------------------

void Store::seedDefaultData() {
    if (!listActionGroups().isEmpty())
        return;

    const QString grpBasic = "grp-basic";
    const QString grpEditing = "grp-editing";
    const QString grpMarkdown = "grp-markdown";
    const QString grpDev = "grp-dev";

    saveActionGroup({grpBasic, "Basic", "⚡", 0});
    saveActionGroup({grpEditing, "Editing", "✎", 1});
    saveActionGroup({grpMarkdown, "Markdown", "M", 2});
    saveActionGroup({grpDev, "Developer", ">_", 3});

    auto addAct = [&](const QString &id, const QString &grp, const QString &name,
                      const QString &desc, const QString &icon, const QString &tint,
                      const QString &shortcut, const QString &afterSuccess,
                      const QVector<ActionStep> &steps, int pos) {
        ActionRecord a;
        a.id = id;
        a.groupId = grp;
        a.name = name;
        a.description = desc;
        a.icon = icon;
        a.tint = tint;
        a.shortcut = shortcut;
        a.afterSuccess = afterSuccess;
        a.steps = steps;
        a.position = pos;
        saveAction(a);
    };

    // --- Basic Group ---
    addAct("act-copy", grpBasic, "Copy", "Copy entire draft to system clipboard", "⧉", "#3b82f6", "", "nothing",
           {{"clipboard", {{"template", "[[draft]]"}, {"mode", "replace"}}}}, 0);
    addAct("act-copy-html", grpBasic, "Copy as HTML", "Convert Markdown to HTML and copy to clipboard", "⟨⟩", "#f59e0b", "", "nothing",
           {{"clipboard", {{"template", "[[markdown]]"}, {"mode", "replace"}}}}, 1);
    addAct("act-append-clip", grpBasic, "Append to Clipboard", "Append draft text to the system clipboard", "+⧉", "#06b6d4", "", "nothing",
           {{"clipboard", {{"template", "[[draft]]"}, {"mode", "append"}}}}, 2);
    addAct("act-save-file", grpBasic, "Save to File", "Save draft as a Markdown file in ~/Documents/Drafts", "⤓", "#10b981", "", "nothing",
           {{"file", {{"path", "~/Documents/Drafts/[[safe_title]].md"}, {"template", "[[draft]]"}, {"mode", "write"}}}}, 3);
    addAct("act-append-journal", grpBasic, "Append to Daily Journal", "Append draft with timestamp to today's journal file", "◫", "#8b5cf6", "", "archive",
           {{"file", {{"path", "~/Documents/Drafts/Journal-[[date|%Y-%m-%d]].md"}, {"template", "\n## [[date|%H:%M]]\n[[draft]]\n"}, {"mode", "append"}}}}, 4);
    addAct("act-mail", grpBasic, "Email", "Open mail client with title as subject and body as text", "✉", "#ef4444", "", "nothing",
           {{"mail", {{"to", ""}, {"subject", "[[title]]"}, {"body", "[[body]]"}}}}, 5);
    addAct("act-preview", grpBasic, "Markdown Preview", "Render draft Markdown to HTML preview", "◉", "#ec4899", "", "nothing",
           {{"html_preview", {{"template", "[[markdown]]"}}}}, 6);
    addAct("act-notify", grpBasic, "Desktop Notification", "Send draft title and excerpt via notify-send", "◈", "#f97316", "", "nothing",
           {{"shell", {{"script", "notify-send \"[[title]]\" \"[[body]]\""}, {"outputMode", "none"}}}}, 7);

    // --- Editing Group (also default Action Bar) ---
    addAct("act-bold", grpEditing, "Bold", "Wrap selection in Markdown bold (**)", "B", "#e68e0d", "", "nothing",
           {{"insert_text", {{"template", "**[[selection]]**"}}}}, 0);
    addAct("act-italic", grpEditing, "Italic", "Wrap selection in Markdown italic (*)", "I", "#e68e0d", "", "nothing",
           {{"insert_text", {{"template", "*[[selection]]*"}}}}, 1);
    addAct("act-heading", grpEditing, "Heading", "Insert or cycle Markdown heading prefix", "H", "#3b82f6", "", "nothing",
           {{"script", {{"code",
               "var range = editor.getSelectedLineRange();\n"
               "var line = editor.getTextInRange(range[0], range[1]);\n"
               "var m = line.match(/^(#{1,6})\\s+/);\n"
               "var next = m ? (m[1].length >= 4 ? line.replace(/^#{1,6}\\s+/, '') : '#' + line) : '# ' + line;\n"
               "editor.setTextInRange(range[0], range[1], next);\n"
           }}}}, 2);
    addAct("act-task", grpEditing, "Task", "Insert or toggle a Markdown task checkbox", "☑", "#10b981", "", "nothing",
           {{"script", {{"code",
               "var range = editor.getSelectedLineRange();\n"
               "var line = editor.getTextInRange(range[0], range[1]);\n"
               "var updated;\n"
               "if (/^\\s*- \\[ \\]/.test(line)) updated = line.replace('- [ ]', '- [x]');\n"
               "else if (/^\\s*- \\[[xX]\\]/.test(line)) updated = line.replace(/- \\[[xX]\\]/, '- [ ]');\n"
               "else updated = '- [ ] ' + line;\n"
               "editor.setTextInRange(range[0], range[1], updated);\n"
           }}}}, 3);
    addAct("act-code", grpEditing, "Code", "Wrap selection in inline code or fenced code block", "</>", "#8b5cf6", "", "nothing",
           {{"script", {{"code",
               "var sel = editor.getSelectedText();\n"
               "if (sel.indexOf('\\n') >= 0) editor.setSelectedText('```\\n' + sel + '\\n```');\n"
               "else editor.setSelectedText('`' + sel + '`');\n"
           }}}}, 4);
    addAct("act-date", grpEditing, "Date", "Insert ISO date (YYYY-MM-DD)", "◷", "#06b6d4", "", "nothing",
           {{"insert_text", {{"template", "[[date|%Y-%m-%d]]"}}}}, 5);
    addAct("act-timestamp", grpEditing, "Timestamp", "Insert ISO date and time", "◴", "#06b6d4", "", "nothing",
           {{"insert_text", {{"template", "[[date|%Y-%m-%d %H:%M]]"}}}}, 6);
    addAct("act-sort-lines", grpEditing, "Sort Lines", "Sort selected lines (or entire draft) alphabetically", "⇅", "#f59e0b", "", "nothing",
           {{"script", {{"code",
               "var sel = editor.getSelectedText();\n"
               "if (sel.length > 0) {\n"
               "  var sorted = sel.split('\\n').sort(function(a,b){return a.localeCompare(b);}).join('\\n');\n"
               "  editor.setSelectedText(sorted);\n"
               "} else {\n"
               "  var lines = draft.content.split('\\n');\n"
               "  var head = lines.shift();\n"
               "  lines.sort(function(a,b){return a.localeCompare(b);});\n"
               "  draft.content = head + '\\n' + lines.join('\\n');\n"
               "  draft.update();\n"
               "}\n"
               "app.displaySuccessMessage('Sorted lines');\n"
           }}}}, 7);

    // --- Markdown Group ---
    addAct("act-md-link", grpMarkdown, "Insert Markdown Link", "Wrap selection as Markdown link using clipboard URL", "⛓", "#3b82f6", "", "nothing",
           {{"insert_text", {{"template", "[[[selection]]]([[clipboard]])"}}}}, 0);
    addAct("act-wikilink-copy", grpMarkdown, "Copy Wiki-Link", "Copy [[Title]] wiki-link to this draft", "[[ ]]", "#8b5cf6", "", "nothing",
           {{"clipboard", {{"template", "[[[[title]]]]"}, {"mode", "replace"}}}}, 1);
    addAct("act-add-backlinks", grpMarkdown, "Insert Backlinks", "Scan library and append a Backlinks section for this draft", "↩", "#10b981", "", "nothing",
           {{"script", {{"code",
               "var title = draft.title;\n"
               "if (!title) { context.fail('Draft needs a title first'); }\n"
               "else {\n"
               "  var hits = Draft.query('[[' + title + ']]', 'all');\n"
               "  var lines = [];\n"
               "  for (var i = 0; i < hits.length; i++) {\n"
               "    if (hits[i].uuid !== draft.uuid) lines.push('- [[' + hits[i].title + ']]');\n"
               "  }\n"
               "  if (lines.length === 0) {\n"
               "    app.displayInfoMessage('No backlinks found for ' + title);\n"
               "  } else {\n"
               "    draft.append('\\n\\n## Backlinks\\n' + lines.join('\\n'));\n"
               "    draft.update();\n"
               "    app.displaySuccessMessage('Inserted ' + lines.length + ' backlink(s)');\n"
               "  }\n"
               "}\n"
           }}}}, 2);
    addAct("act-strip-md", grpMarkdown, "Copy Plain Text", "Strip basic Markdown markers and copy clean text", "T", "#f59e0b", "", "nothing",
           {{"script", {{"code",
               "var txt = draft.content\n"
               "  .replace(/^#{1,6}\\s+/gm, '')\n"
               "  .replace(/\\*\\*(.*?)\\*\\*/g, '$1')\n"
               "  .replace(/\\*(.*?)\\*/g, '$1')\n"
               "  .replace(/`([^`]+)`/g, '$1')\n"
               "  .replace(/\\[([^\\]]+)\\]\\([^)]+\\)/g, '$1');\n"
               "app.setClipboard(txt);\n"
               "app.displaySuccessMessage('Copied plain text');\n"
           }}}}, 3);
    addAct("act-toc", grpMarkdown, "Insert Table of Contents", "Generate a Markdown table of contents from headings", "≡", "#06b6d4", "", "nothing",
           {{"script", {{"code",
               "var lines = draft.content.split('\\n');\n"
               "var toc = [];\n"
               "for (var i = 0; i < lines.length; i++) {\n"
               "  var m = lines[i].match(/^(#{1,6})\\s+(.+)/);\n"
               "  if (m) {\n"
               "    var indent = '';\n"
               "    for (var j = 1; j < m[1].length; j++) indent += '  ';\n"
               "    toc.push(indent + '- ' + m[2].trim());\n"
               "  }\n"
               "}\n"
               "editor.setSelectedText(toc.join('\\n') + '\\n');\n"
           }}}}, 4);

    // --- Developer Group ---
    addAct("act-run-shell", grpDev, "Run as Shell Script", "Execute draft body in /bin/sh and append output", "▶", "#10b981", "", "nothing",
           {{"shell", {{"script", "[[draft]]"}, {"outputMode", "append"}}}}, 0);
    addAct("act-format-json", grpDev, "Format JSON", "Pretty-print JSON in selection or entire draft", "{}", "#f59e0b", "", "nothing",
           {{"script", {{"code",
               "var sel = editor.getSelectedText();\n"
               "var raw = sel.length > 0 ? sel : draft.content;\n"
               "try {\n"
               "  var formatted = JSON.stringify(JSON.parse(raw), null, 2);\n"
               "  if (sel.length > 0) editor.setSelectedText(formatted);\n"
               "  else { draft.content = formatted; draft.update(); }\n"
               "  app.displaySuccessMessage('Formatted JSON');\n"
               "} catch (e) {\n"
               "  context.fail('Invalid JSON: ' + e);\n"
               "}\n"
           }}}}, 1);
    addAct("act-url-encode", grpDev, "URL Encode Selection", "Percent-encode the currently selected text", "%", "#3b82f6", "", "nothing",
           {{"insert_text", {{"template", "[[enc:selection]]"}}}}, 2);
    addAct("act-word-stats", grpDev, "Draft Statistics", "Compute word count, unique words, and reading time", "∑", "#8b5cf6", "", "nothing",
           {{"script", {{"code",
               "var words = (draft.content.match(/\\S+/g) || []);\n"
               "var uniq = {};\n"
               "for (var i = 0; i < words.length; i++) uniq[words[i].toLowerCase()] = true;\n"
               "var mins = Math.max(1, Math.ceil(words.length / 200));\n"
               "app.displayInfoMessage(words.length + ' words • ' + Object.keys(uniq).length + ' unique • ~' + mins + ' min read');\n"
           }}}}, 3);
    addAct("act-web-search", grpDev, "Web Search Selection", "Search the web for the selected text or draft title", "⌕", "#06b6d4", "", "nothing",
           {{"url", {{"template", "https://duckduckgo.com/?q=[[enc:selection_only]]"}}}}, 4);

    // Seed default Workspaces
    if (listWorkspaces().isEmpty()) {
        WorkspaceRecord def;
        def.id = "ws-default";
        def.name = "Default";
        def.defaultFolder = "inbox";
        def.actionGroupId = grpBasic;
        def.actionBarGroupId = grpEditing;
        def.position = 0;
        saveWorkspace(def);

        WorkspaceRecord writing;
        writing.id = "ws-writing";
        writing.name = "Writing";
        writing.tagFilter = "writing";
        writing.defaultFolder = "all";
        writing.actionGroupId = grpMarkdown;
        writing.actionBarGroupId = grpEditing;
        writing.position = 1;
        saveWorkspace(writing);

        WorkspaceRecord tasks;
        tasks.id = "ws-tasks";
        tasks.name = "Tasks";
        tasks.tagFilter = "tasks";
        tasks.defaultFolder = "inbox";
        tasks.actionGroupId = grpEditing;
        tasks.actionBarGroupId = grpEditing;
        tasks.position = 2;
        saveWorkspace(tasks);

        WorkspaceRecord dev;
        dev.id = "ws-dev";
        dev.name = "Code & Scripts";
        dev.tagFilter = "dev";
        dev.defaultFolder = "all";
        dev.actionGroupId = grpDev;
        dev.actionBarGroupId = grpDev;
        dev.position = 3;
        saveWorkspace(dev);
    }
}

void Store::seedWelcomeDraftIfEmpty() {
    DraftQuery dq;
    dq.folder = "all";
    if (!queryDrafts(dq).isEmpty())
        return;

    const QString welcome =
        "# Welcome to Drafts\n"
        "\n"
        "Where text starts — capture immediately, organize with tags and workspaces, and send text anywhere with Actions.\n"
        "\n"
        "## Quick Highlights\n"
        "- [ ] Press **Ctrl+N** anytime for a fresh blank draft\n"
        "- [ ] Press **Ctrl+K** for Quick Search across drafts, actions, and workspaces\n"
        "- [ ] Press **?** to view all keyboard shortcuts\n"
        "- [ ] Use `[[Title]]` wiki-links to connect drafts (try Link Mode with **Ctrl+L**)\n"
        "- [ ] Open **Arrange Mode** (**Ctrl+Shift+A**) to reorder lines or paragraphs\n"
        "- [ ] Preview Markdown (**Ctrl+Shift+P**) or run JavaScript & Shell Actions from the right panel\n"
        "\n"
        "## Nested & Scoped Tags\n"
        "Assign nested tags like `projects/omarchy` or exclusive scoped tags like `status::active` in the tag bar above.\n";

    DraftRecord d = createDraft(welcome, {"welcome", "status::active"}, "inbox", "markdown", true, "orange");
    saveVersion(d.uuid, d.content, false);
}
