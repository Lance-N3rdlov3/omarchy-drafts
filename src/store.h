#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct DraftRecord {
    QString uuid;
    QString content;
    QString folder = "inbox"; // inbox | archive | trash
    bool flagged = false;
    QString flagColor = "orange"; // accent | orange | red | yellow | green | blue | purple
    QString syntax = "markdown";  // markdown | gfm | plain | taskpaper | javascript | json | html
    QStringList tags;
    QString createdAt;
    QString modifiedAt;
    QString accessedAt;
    int cursorPos = 0;

    QString title() const;
    QString displayTitle() const;
    QString safeTitle() const;
    QString body() const;
    QString excerpt() const;
    int wordCount() const;
    int charCount() const;
    int lineCount() const;
    QVariantMap toVariantMap() const;
    QJsonObject toJson() const;
    static DraftRecord fromJson(const QJsonObject &obj);
};

struct VersionRecord {
    qint64 id = 0;
    QString draftUuid;
    QString content;
    QString createdAt;

    QVariantMap toVariantMap() const;
    QJsonObject toJson() const;
};

struct WorkspaceRecord {
    QString id;
    QString name;
    QString tagFilter;          // Comma-separated tags; prefix '-' omits
    QString tagFilterMode = "all"; // all | any
    QString query;
    QString sortField = "modified"; // modified | created | accessed | text
    bool sortDescending = true;
    bool showFlaggedFirst = true;
    QString defaultFolder = "inbox"; // inbox | flagged | archive | all
    QString actionGroupId;
    QString actionBarGroupId;
    int position = 0;

    QVariantMap toVariantMap() const;
    QJsonObject toJson() const;
    static WorkspaceRecord fromJson(const QJsonObject &obj);
};

struct ActionStep {
    QString type; // clipboard | file | url | mail | insert_text | shell | html_preview | script
    QVariantMap config;

    QVariantMap toVariantMap() const;
    QJsonObject toJson() const;
    static ActionStep fromJson(const QJsonObject &obj);
};

struct ActionRecord {
    QString id;
    QString groupId;
    QString name;
    QString description;
    QString icon;
    QString tint;
    QString shortcut;
    QString afterSuccess = "nothing"; // nothing | archive | trash
    QVector<ActionStep> steps;
    int position = 0;

    QVariantMap toVariantMap() const;
    QJsonObject toJson() const;
    static ActionRecord fromJson(const QJsonObject &obj);
};

struct ActionGroupRecord {
    QString id;
    QString name;
    QString icon;
    int position = 0;

    QVariantMap toVariantMap() const;
    QJsonObject toJson() const;
};

struct ActionLogRecord {
    qint64 id = 0;
    QString actionId;
    QString actionName;
    QString draftUuid;
    QString status; // success | cancelled | error
    QString message;
    QString createdAt;

    QVariantMap toVariantMap() const;
};

struct DraftQuery {
    QString folder = "inbox"; // inbox | flagged | archive | trash | all
    QString query;
    QStringList includeTags;
    QStringList omitTags;
    QString tagMode = "all"; // all | any
    QString sortField = "modified"; // modified | created | accessed | text
    bool sortDescending = true;
    bool showFlaggedFirst = true;
};

class Store {
public:
    explicit Store(const QString &dbPath = {});
    ~Store();

    bool isOpen() const { return m_open; }
    QString dbPath() const { return m_dbPath; }
    QString lastError() const { return m_lastError; }

    // Drafts CRUD
    DraftRecord createDraft(const QString &content = {}, const QStringList &tags = {},
                            const QString &folder = "inbox", const QString &syntax = "markdown",
                            bool flagged = false, const QString &flagColor = "orange");
    bool getDraft(const QString &uuid, DraftRecord &out) const;
    bool findDraftByTitle(const QString &title, DraftRecord &out) const;
    bool updateDraft(const DraftRecord &draft, bool touchModified = true);
    bool touchAccessed(const QString &uuid);
    bool setDraftFolder(const QString &uuid, const QString &folder);
    bool setDraftFlagged(const QString &uuid, bool flagged, const QString &flagColor = {});
    bool setDraftSyntax(const QString &uuid, const QString &syntax);
    bool setDraftCursor(const QString &uuid, int cursorPos);
    bool deleteDraftPermanently(const QString &uuid);
    int emptyTrash();

    // Query & Search
    QVector<DraftRecord> queryDrafts(const DraftQuery &q) const;
    QVector<DraftRecord> backlinksFor(const QString &uuid) const;
    QVariantMap folderCounts(const WorkspaceRecord *workspace = nullptr) const;

    // Tags (with nested and scoped tag semantics)
    bool addTagToDraft(const QString &uuid, const QString &rawTag);
    bool removeTagFromDraft(const QString &uuid, const QString &tag);
    bool setDraftTags(const QString &uuid, const QStringList &tags);
    QVariantList allTagsWithCounts(const QString &folder = "all") const;
    bool renameTagGlobally(const QString &oldTag, const QString &newTag);
    bool deleteTagGlobally(const QString &tag);
    static QString normalizeTag(const QString &raw);
    static bool tagMatchesFilter(const QString &draftTag, const QString &filterTag);

    // Version history
    qint64 saveVersion(const QString &uuid, const QString &content, bool skipIfUnchanged = true);
    QVector<VersionRecord> versionsForDraft(const QString &uuid) const;
    bool restoreVersion(const QString &uuid, qint64 versionId, DraftRecord &updated);

    // Workspaces
    QVector<WorkspaceRecord> listWorkspaces() const;
    bool getWorkspace(const QString &id, WorkspaceRecord &out) const;
    bool findWorkspaceByName(const QString &name, WorkspaceRecord &out) const;
    WorkspaceRecord saveWorkspace(const WorkspaceRecord &ws);
    bool deleteWorkspace(const QString &id);

    // Action Groups & Actions
    QVector<ActionGroupRecord> listActionGroups() const;
    ActionGroupRecord saveActionGroup(const ActionGroupRecord &group);
    bool deleteActionGroup(const QString &id);

    QVector<ActionRecord> listActions(const QString &groupId = {}) const;
    bool getAction(const QString &id, ActionRecord &out) const;
    bool findActionByName(const QString &name, ActionRecord &out) const;
    ActionRecord saveAction(const ActionRecord &action);
    bool deleteAction(const QString &id);

    // Action Log
    void recordActionLog(const QString &actionId, const QString &actionName,
                         const QString &draftUuid, const QString &status, const QString &message);
    QVector<ActionLogRecord> recentActionLogs(int limit = 100) const;
    void clearActionLogs();

    // Settings
    QString getSetting(const QString &key, const QString &defaultValue = {}) const;
    void setSetting(const QString &key, const QString &value);
    QVariantMap allSettings() const;

    // Backup & Export / Import
    bool exportBackupJson(const QString &filePath, QString *errorOut = nullptr) const;
    bool importBackupJson(const QString &filePath, int *importedDrafts = nullptr,
                          int *importedActions = nullptr, QString *errorOut = nullptr);
    QString exportActionJson(const QString &actionId) const;
    bool importActionJson(const QString &jsonText, const QString &targetGroupId, ActionRecord &out,
                          QString *errorOut = nullptr);

    void seedDefaultData();
    void seedWelcomeDraftIfEmpty();

    static QString defaultDbPath();
    static QString nowIso();

private:
    bool initSchema();
    QStringList tagsForDraft(const QString &uuid) const;
    bool matchesSearchQuery(const DraftRecord &draft, const QString &queryText) const;
    bool matchesWorkspaceFilter(const DraftRecord &draft, const DraftQuery &q) const;
    static void parseWorkspaceTags(const QString &tagFilter, QStringList &includeTags, QStringList &omitTags);

    QString m_dbPath;
    QString m_connectionName;
    bool m_open = false;
    mutable QString m_lastError;
};
