#pragma once

#include "actionengine.h"
#include "filepicker.h"
#include "store.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class Backend : public QObject {
    Q_OBJECT

    // Current draft properties
    Q_PROPERTY(QString currentUuid READ currentUuid NOTIFY currentDraftChanged)
    Q_PROPERTY(QString content READ content NOTIFY currentDraftChanged)
    Q_PROPERTY(QString title READ title NOTIFY currentDraftChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY currentDraftChanged)
    Q_PROPERTY(bool flagged READ flagged NOTIFY currentDraftChanged)
    Q_PROPERTY(QString flagColor READ flagColor NOTIFY currentDraftChanged)
    Q_PROPERTY(QString syntax READ syntax NOTIFY currentDraftChanged)
    Q_PROPERTY(QStringList tags READ tags NOTIFY currentDraftChanged)
    Q_PROPERTY(int wordCount READ wordCount NOTIFY currentDraftChanged)
    Q_PROPERTY(int charCount READ charCount NOTIFY currentDraftChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY currentDraftChanged)
    Q_PROPERTY(QString createdAt READ createdAt NOTIFY currentDraftChanged)
    Q_PROPERTY(QString modifiedAt READ modifiedAt NOTIFY currentDraftChanged)
    Q_PROPERTY(QString accessedAt READ accessedAt NOTIFY currentDraftChanged)
    Q_PROPERTY(int cursorPos READ cursorPos NOTIFY currentDraftChanged)
    Q_PROPERTY(int contentRevision READ contentRevision NOTIFY editorReloaded)

    // Lists & counts
    Q_PROPERTY(QVariantList drafts READ drafts NOTIFY draftsChanged)
    Q_PROPERTY(QVariantList allTags READ allTags NOTIFY draftsChanged)
    Q_PROPERTY(QVariantMap folderCounts READ folderCounts NOTIFY draftsChanged)
    Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY workspacesChanged)
    Q_PROPERTY(QVariantList actionGroups READ actionGroups NOTIFY actionsChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY actionsChanged)
    Q_PROPERTY(QVariantList actionBarActions READ actionBarActions NOTIFY actionsChanged)
    Q_PROPERTY(QVariantList versions READ versions NOTIFY currentDraftMetaChanged)
    Q_PROPERTY(QVariantList outline READ outline NOTIFY currentDraftMetaChanged)
    Q_PROPERTY(QVariantList backlinks READ backlinks NOTIFY currentDraftMetaChanged)
    Q_PROPERTY(QVariantList actionLogs READ actionLogs NOTIFY actionLogsChanged)

    // Navigation & filter state
    Q_PROPERTY(QString activeFolder READ activeFolder WRITE setActiveFolder NOTIFY filterChanged)
    Q_PROPERTY(QString activeWorkspaceId READ activeWorkspaceId NOTIFY filterChanged)
    Q_PROPERTY(QString activeWorkspaceName READ activeWorkspaceName NOTIFY filterChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY filterChanged)
    Q_PROPERTY(QStringList filterTags READ filterTags NOTIFY filterChanged)
    Q_PROPERTY(QString activeActionGroupId READ activeActionGroupId WRITE setActiveActionGroupId NOTIFY actionsChanged)
    Q_PROPERTY(QString activeActionBarGroupId READ activeActionBarGroupId WRITE setActiveActionBarGroupId NOTIFY actionsChanged)
    Q_PROPERTY(QString actionSearchQuery READ actionSearchQuery WRITE setActionSearchQuery NOTIFY actionsChanged)

    // Modes & Preview
    Q_PROPERTY(bool focusMode READ focusMode WRITE setFocusMode NOTIFY modesChanged)
    Q_PROPERTY(bool linkMode READ linkMode WRITE setLinkMode NOTIFY modesChanged)
    Q_PROPERTY(bool typewriterMode READ typewriterMode WRITE setTypewriterMode NOTIFY modesChanged)
    Q_PROPERTY(bool showDraftList READ showDraftList WRITE setShowDraftList NOTIFY modesChanged)
    Q_PROPERTY(bool showActionPanel READ showActionPanel WRITE setShowActionPanel NOTIFY modesChanged)
    Q_PROPERTY(QString linkModeHtml READ linkModeHtml NOTIFY currentDraftChanged)
    Q_PROPERTY(QString previewHtml READ previewHtml NOTIFY previewChanged)
    Q_PROPERTY(bool previewVisible READ previewVisible WRITE setPreviewVisible NOTIFY previewChanged)

    // Typography & preferences
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY settingsChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY settingsChanged)
    Q_PROPERTY(double lineHeight READ lineHeight WRITE setLineHeight NOTIFY settingsChanged)
    Q_PROPERTY(int editorMaxWidth READ editorMaxWidth WRITE setEditorMaxWidth NOTIFY settingsChanged)
    Q_PROPERTY(int newDraftTimeoutSecs READ newDraftTimeoutSecs WRITE setNewDraftTimeoutSecs NOTIFY settingsChanged)
    Q_PROPERTY(bool newDraftOnLaunch READ newDraftOnLaunch WRITE setNewDraftOnLaunch NOTIFY settingsChanged)

    // Status toast
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString statusKind READ statusKind NOTIFY statusChanged)
    Q_PROPERTY(int statusRevision READ statusRevision NOTIFY statusChanged)

public:
    explicit Backend(const QString &dbPath = {}, FilePicker *filePicker = nullptr,
                     bool seedWelcome = true, QObject *parent = nullptr);

    Store &store() { return m_store; }
    ActionEngine &actionEngine() { return m_actionEngine; }
    void setAccentColor(const QString &accent);

    // Current draft getters
    QString currentUuid() const { return m_current.uuid; }
    QString content() const { return m_current.content; }
    QString title() const { return m_current.displayTitle(); }
    QString folder() const { return m_current.folder; }
    bool flagged() const { return m_current.flagged; }
    QString flagColor() const { return m_current.flagColor; }
    QString syntax() const { return m_current.syntax; }
    QStringList tags() const { return m_current.tags; }
    int wordCount() const { return m_current.wordCount(); }
    int charCount() const { return m_current.charCount(); }
    int lineCount() const { return m_current.lineCount(); }
    QString createdAt() const { return m_current.createdAt; }
    QString modifiedAt() const { return m_current.modifiedAt; }
    QString accessedAt() const { return m_current.accessedAt; }
    int cursorPos() const { return m_current.cursorPos; }
    int contentRevision() const { return m_contentRevision; }

    // Lists
    QVariantList drafts() const { return m_drafts; }
    QVariantList allTags() const { return m_allTags; }
    QVariantMap folderCounts() const { return m_folderCounts; }
    QVariantList workspaces() const { return m_workspaces; }
    QVariantList actionGroups() const { return m_actionGroups; }
    QVariantList actions() const { return m_actions; }
    QVariantList actionBarActions() const { return m_actionBarActions; }
    QVariantList versions() const { return m_versions; }
    QVariantList outline() const { return m_outline; }
    QVariantList backlinks() const { return m_backlinks; }
    QVariantList actionLogs() const { return m_actionLogs; }

    // Filter getters/setters
    QString activeFolder() const { return m_activeFolder; }
    void setActiveFolder(const QString &folder);
    QString activeWorkspaceId() const { return m_activeWorkspaceId; }
    QString activeWorkspaceName() const { return m_activeWorkspaceName; }
    QString searchQuery() const { return m_searchQuery; }
    void setSearchQuery(const QString &query);
    QStringList filterTags() const { return m_filterTags; }
    QString activeActionGroupId() const { return m_activeActionGroupId; }
    void setActiveActionGroupId(const QString &id);
    QString activeActionBarGroupId() const { return m_activeActionBarGroupId; }
    void setActiveActionBarGroupId(const QString &id);
    QString actionSearchQuery() const { return m_actionSearchQuery; }
    void setActionSearchQuery(const QString &q);

    // Modes
    bool focusMode() const { return m_focusMode; }
    void setFocusMode(bool v);
    bool linkMode() const { return m_linkMode; }
    void setLinkMode(bool v);
    bool typewriterMode() const { return m_typewriterMode; }
    void setTypewriterMode(bool v);
    bool showDraftList() const { return m_showDraftList; }
    void setShowDraftList(bool v);
    bool showActionPanel() const { return m_showActionPanel; }
    void setShowActionPanel(bool v);
    QString linkModeHtml() const;
    QString previewHtml() const { return m_previewHtml; }
    bool previewVisible() const { return m_previewVisible; }
    void setPreviewVisible(bool v);

    // Typography & settings
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &f);
    int fontSize() const { return m_fontSize; }
    void setFontSize(int sz);
    double lineHeight() const { return m_lineHeight; }
    void setLineHeight(double lh);
    int editorMaxWidth() const { return m_editorMaxWidth; }
    void setEditorMaxWidth(int w);
    int newDraftTimeoutSecs() const { return m_newDraftTimeoutSecs; }
    void setNewDraftTimeoutSecs(int secs);
    bool newDraftOnLaunch() const { return m_newDraftOnLaunch; }
    void setNewDraftOnLaunch(bool v);

    // Status toast
    QString statusMessage() const { return m_statusMessage; }
    QString statusKind() const { return m_statusKind; }
    int statusRevision() const { return m_statusRevision; }

    // Draft operations
    Q_INVOKABLE QString newDraft(const QString &initialContent = {}, const QStringList &initialTags = {});
    Q_INVOKABLE QString quickCaptureSave(const QString &text, const QString &tagsCsv, bool flagged, const QString &syntax);
    Q_INVOKABLE void selectDraft(const QString &uuid);
    Q_INVOKABLE void selectNextDraft(int delta);
    Q_INVOKABLE void updateContent(const QString &text);
    Q_INVOKABLE void setCursorPos(int pos);
    Q_INVOKABLE void toggleFlag(const QString &uuid = {});
    Q_INVOKABLE void setFlagColor(const QString &color, const QString &uuid = {});
    Q_INVOKABLE void setSyntax(const QString &syntax);
    Q_INVOKABLE void archiveDraft(const QString &uuid = {});
    Q_INVOKABLE void moveDraftToInbox(const QString &uuid = {});
    Q_INVOKABLE void trashDraft(const QString &uuid = {});
    Q_INVOKABLE void deleteDraftPermanently(const QString &uuid = {});
    Q_INVOKABLE void emptyTrash();
    Q_INVOKABLE void duplicateCurrentDraft();

    // Tags
    Q_INVOKABLE void addTag(const QString &tag);
    Q_INVOKABLE void removeTag(const QString &tag);
    Q_INVOKABLE void toggleFilterTag(const QString &tag);
    Q_INVOKABLE void clearFilterTags();
    Q_INVOKABLE void renameTagGlobally(const QString &oldTag, const QString &newTag);
    Q_INVOKABLE void deleteTagGlobally(const QString &tag);

    // Versions
    Q_INVOKABLE void saveManualVersion();
    Q_INVOKABLE void restoreVersion(qint64 versionId);

    // Workspaces
    Q_INVOKABLE void selectWorkspace(const QString &id);
    Q_INVOKABLE QString saveWorkspace(const QVariantMap &data);
    Q_INVOKABLE void deleteWorkspace(const QString &id);

    // Actions & Action Groups
    Q_INVOKABLE void runAction(const QString &actionId, int selStart = 0, int selLen = 0);
    Q_INVOKABLE QString saveAction(const QVariantMap &data);
    Q_INVOKABLE void deleteAction(const QString &id);
    Q_INVOKABLE QString saveActionGroup(const QString &id, const QString &name, const QString &icon);
    Q_INVOKABLE void deleteActionGroup(const QString &id);
    Q_INVOKABLE QString exportActionJson(const QString &id) const;
    Q_INVOKABLE bool importActionJson(const QString &jsonText, const QString &groupId);
    Q_INVOKABLE void clearActionLogs();

    // Editor intelligence: tasks, wiki-links, arrange, preview, search, find/replace
    Q_INVOKABLE void toggleTaskAtLine(int lineIndex);
    Q_INVOKABLE void toggleTaskAtCursor(int cursorPosition);
    Q_INVOKABLE void openLink(const QString &href);
    Q_INVOKABLE void openWikiLink(const QString &raw);
    Q_INVOKABLE QStringList splitForArrange(const QString &mode) const;
    Q_INVOKABLE void commitArrange(const QStringList &blocks, const QString &mode);
    Q_INVOKABLE void openMarkdownPreview();
    Q_INVOKABLE void copyPreviewHtml();
    Q_INVOKABLE void openPreviewInBrowser();
    Q_INVOKABLE QVariantList quickSearch(const QString &query) const;
    Q_INVOKABLE int replaceAllInCurrent(const QString &findText, const QString &replaceText,
                                        bool caseSensitive, bool useRegex);

    // URL scheme & Import / Export / Backup
    Q_INVOKABLE bool handleDraftsUrl(const QString &urlString);
    Q_INVOKABLE void exportBackupDialog();
    Q_INVOKABLE void importBackupDialog();
    Q_INVOKABLE void exportCurrentMarkdownDialog();
    Q_INVOKABLE void exportCurrentHtmlDialog();
    Q_INVOKABLE void createAutoBackupNow();
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void showToast(const QString &message, const QString &kind = "info");

signals:
    void currentDraftChanged();
    void editorReloaded();
    void editorSelectionRequested(int start, int length);
    void editorCursorJumpRequested(int position);
    void draftsChanged();
    void workspacesChanged();
    void actionsChanged();
    void currentDraftMetaChanged();
    void actionLogsChanged();
    void filterChanged();
    void modesChanged();
    void previewChanged();
    void settingsChanged();
    void statusChanged();

private:
    void loadSettings();
    void refreshAll();
    void refreshDrafts();
    void refreshCurrentMeta();
    void refreshWorkspaces();
    void refreshActions();
    void refreshActionLogs();
    void flushPendingSave();
    void setCurrentDraftRecord(const DraftRecord &record, bool bumpEditorRevision);

    Store m_store;
    ActionEngine m_actionEngine;
    FilePicker *m_filePicker = nullptr;
    QString m_accentColor = "#e68e0d";

    DraftRecord m_current;
    bool m_dirty = false;
    int m_contentRevision = 0;
    QTimer m_saveTimer;
    QTimer m_versionTimer;

    QVariantList m_drafts;
    QVariantList m_allTags;
    QVariantMap m_folderCounts;
    QVariantList m_workspaces;
    QVariantList m_actionGroups;
    QVariantList m_actions;
    QVariantList m_actionBarActions;
    QVariantList m_versions;
    QVariantList m_outline;
    QVariantList m_backlinks;
    QVariantList m_actionLogs;

    QString m_activeFolder = "inbox";
    QString m_activeWorkspaceId = "ws-default";
    QString m_activeWorkspaceName = "Default";
    QString m_searchQuery;
    QStringList m_filterTags;
    QString m_activeActionGroupId = "grp-basic";
    QString m_activeActionBarGroupId = "grp-editing";
    QString m_actionSearchQuery;

    bool m_focusMode = false;
    bool m_linkMode = false;
    bool m_typewriterMode = false;
    bool m_showDraftList = true;
    bool m_showActionPanel = true;
    QString m_previewHtml;
    bool m_previewVisible = false;

    QString m_fontFamily = "monospace";
    int m_fontSize = 15;
    double m_lineHeight = 1.55;
    int m_editorMaxWidth = 760;
    int m_newDraftTimeoutSecs = 0;
    bool m_newDraftOnLaunch = false;

    QString m_statusMessage;
    QString m_statusKind = "info";
    int m_statusRevision = 0;
};
