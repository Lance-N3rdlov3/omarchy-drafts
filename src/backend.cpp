#include "backend.h"
#include "markdown.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

Backend::Backend(const QString &dbPath, FilePicker *filePicker, bool seedWelcome, QObject *parent)
    : QObject(parent),
      m_store(dbPath),
      m_actionEngine(m_store),
      m_filePicker(filePicker) {
    loadSettings();

    if (seedWelcome)
        m_store.seedWelcomeDraftIfEmpty();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer, &QTimer::timeout, this, &Backend::flushPendingSave);

    // Automatic background version snapshots after 12 seconds of idle typing.
    m_versionTimer.setSingleShot(true);
    m_versionTimer.setInterval(12000);
    connect(&m_versionTimer, &QTimer::timeout, this, [this] {
        if (!m_current.uuid.isEmpty() && !m_current.content.trimmed().isEmpty()) {
            m_store.saveVersion(m_current.uuid, m_current.content, true);
            refreshCurrentMeta();
        }
    });

    refreshWorkspaces();
    refreshActions();
    refreshActionLogs();

    // Load default workspace settings
    WorkspaceRecord ws;
    if (m_store.getWorkspace(m_activeWorkspaceId, ws)) {
        m_activeWorkspaceName = ws.name;
        m_activeFolder = ws.defaultFolder;
        if (!ws.actionGroupId.isEmpty())
            m_activeActionGroupId = ws.actionGroupId;
        if (!ws.actionBarGroupId.isEmpty())
            m_activeActionBarGroupId = ws.actionBarGroupId;
    }

    refreshDrafts();
    refreshActions();

    if (m_newDraftOnLaunch || m_drafts.isEmpty()) {
        newDraft();
    } else {
        const QString firstUuid = m_drafts.first().toMap().value("uuid").toString();
        selectDraft(firstUuid);
    }
}

void Backend::setAccentColor(const QString &accent) {
    if (m_accentColor == accent)
        return;
    m_accentColor = accent;
    if (m_linkMode)
        emit currentDraftChanged();
    if (m_previewVisible) {
        m_previewHtml = MarkdownProcessor::toHtml(m_current.content, m_accentColor, false);
        emit previewChanged();
    }
}

void Backend::loadSettings() {
    m_fontFamily = m_store.getSetting("fontFamily", "monospace");
    m_fontSize = qBound(10, m_store.getSetting("fontSize", "15").toInt(), 32);
    m_lineHeight = qBound(1.1, m_store.getSetting("lineHeight", "1.55").toDouble(), 2.4);
    m_editorMaxWidth = qBound(480, m_store.getSetting("editorMaxWidth", "760").toInt(), 1600);
    m_newDraftTimeoutSecs = qMax(0, m_store.getSetting("newDraftTimeoutSecs", "0").toInt());
    m_newDraftOnLaunch = (m_store.getSetting("newDraftOnLaunch", "0") == "1");
    m_typewriterMode = (m_store.getSetting("typewriterMode", "0") == "1");
}

void Backend::flushPendingSave() {
    if (!m_dirty || m_current.uuid.isEmpty())
        return;
    m_dirty = false;
    m_store.updateDraft(m_current, true);
    DraftRecord fresh;
    if (m_store.getDraft(m_current.uuid, fresh))
        m_current = fresh;
    refreshDrafts();
    refreshCurrentMeta();
    emit currentDraftChanged();
}

void Backend::setCurrentDraftRecord(const DraftRecord &record, bool bumpEditorRevision) {
    m_current = record;
    if (bumpEditorRevision) {
        ++m_contentRevision;
        emit editorReloaded();
    }
    refreshCurrentMeta();
    emit currentDraftChanged();
    if (m_previewVisible) {
        m_previewHtml = MarkdownProcessor::toHtml(m_current.content, m_accentColor, false);
        emit previewChanged();
    }
}

void Backend::refreshAll() {
    refreshWorkspaces();
    refreshDrafts();
    refreshActions();
    refreshCurrentMeta();
    refreshActionLogs();
}

void Backend::refreshDrafts() {
    DraftQuery dq;
    dq.folder = m_activeFolder;

    WorkspaceRecord ws;
    const bool hasWs = m_store.getWorkspace(m_activeWorkspaceId, ws);
    if (hasWs) {
        dq.sortField = ws.sortField;
        dq.sortDescending = ws.sortDescending;
        dq.showFlaggedFirst = ws.showFlaggedFirst;
        dq.tagMode = ws.tagFilterMode;
        const QStringList wsParts = ws.tagFilter.split(',', Qt::SkipEmptyParts);
        for (QString p : wsParts) {
            p = p.trimmed();
            if (p.startsWith('-'))
                dq.omitTags.append(Store::normalizeTag(p.mid(1)));
            else if (!p.isEmpty())
                dq.includeTags.append(Store::normalizeTag(p));
        }
        dq.query = ws.query;
    }

    for (const QString &ft : m_filterTags) {
        if (ft.startsWith('-'))
            dq.omitTags.append(Store::normalizeTag(ft.mid(1)));
        else
            dq.includeTags.append(Store::normalizeTag(ft));
    }

    if (!m_searchQuery.trimmed().isEmpty()) {
        if (dq.query.isEmpty())
            dq.query = m_searchQuery.trimmed();
        else
            dq.query = dq.query + " " + m_searchQuery.trimmed();
    }

    const auto rows = m_store.queryDrafts(dq);
    QVariantList list;
    list.reserve(rows.size());
    for (const DraftRecord &d : rows)
        list.append(d.toVariantMap());
    m_drafts = list;

    m_allTags = m_store.allTagsWithCounts(m_activeFolder);
    m_folderCounts = m_store.folderCounts(hasWs ? &ws : nullptr);
    emit draftsChanged();
}

void Backend::refreshCurrentMeta() {
    if (m_current.uuid.isEmpty()) {
        m_versions.clear();
        m_outline.clear();
        m_backlinks.clear();
        emit currentDraftMetaChanged();
        return;
    }

    QVariantList vList;
    for (const VersionRecord &v : m_store.versionsForDraft(m_current.uuid))
        vList.append(v.toVariantMap());
    m_versions = vList;

    QVariantList oList;
    for (const OutlineItem &item : MarkdownProcessor::extractOutline(m_current.content, m_current.syntax)) {
        oList.append(QVariantMap{
            {"title", item.title},
            {"kind", item.kind},
            {"level", item.level},
            {"line", item.line},
            {"position", item.position},
        });
    }
    m_outline = oList;

    QVariantList bList;
    for (const DraftRecord &d : m_store.backlinksFor(m_current.uuid))
        bList.append(d.toVariantMap());
    m_backlinks = bList;

    emit currentDraftMetaChanged();
}

void Backend::refreshWorkspaces() {
    QVariantList list;
    for (const WorkspaceRecord &ws : m_store.listWorkspaces())
        list.append(ws.toVariantMap());
    m_workspaces = list;
    emit workspacesChanged();
}

void Backend::refreshActions() {
    QVariantList groups;
    const auto allGroups = m_store.listActionGroups();
    for (const ActionGroupRecord &g : allGroups)
        groups.append(g.toVariantMap());
    m_actionGroups = groups;

    if (!allGroups.isEmpty()) {
        auto hasGroup = [&](const QString &id) {
            for (const auto &g : allGroups)
                if (g.id == id)
                    return true;
            return false;
        };
        if (!hasGroup(m_activeActionGroupId))
            m_activeActionGroupId = allGroups.first().id;
        if (!hasGroup(m_activeActionBarGroupId))
            m_activeActionBarGroupId = allGroups.first().id;
    }

    QVariantList acts;
    const QString needle = m_actionSearchQuery.trimmed().toLower();
    const auto rawActions = needle.isEmpty() ? m_store.listActions(m_activeActionGroupId) : m_store.listActions();
    for (const ActionRecord &a : rawActions) {
        if (!needle.isEmpty() && !a.name.toLower().contains(needle) && !a.description.toLower().contains(needle))
            continue;
        acts.append(a.toVariantMap());
    }
    m_actions = acts;

    QVariantList barActs;
    for (const ActionRecord &a : m_store.listActions(m_activeActionBarGroupId))
        barActs.append(a.toVariantMap());
    m_actionBarActions = barActs;

    emit actionsChanged();
}

void Backend::refreshActionLogs() {
    QVariantList logs;
    for (const ActionLogRecord &r : m_store.recentActionLogs(100))
        logs.append(r.toVariantMap());
    m_actionLogs = logs;
    emit actionLogsChanged();
}

// ---------------------------------------------------------------------------
// Draft Operations
// ---------------------------------------------------------------------------

QString Backend::newDraft(const QString &initialContent, const QStringList &initialTags) {
    flushPendingSave();
    if (!m_current.uuid.isEmpty() && !m_current.content.trimmed().isEmpty())
        m_store.saveVersion(m_current.uuid, m_current.content, true);

    QStringList tags = initialTags;
    // Inherit positive filter tags when creating inside a tagged workspace/filter!
    for (const QString &ft : m_filterTags) {
        if (!ft.startsWith('-') && ft != "untagged" && !tags.contains(ft))
            tags.append(ft);
    }
    WorkspaceRecord ws;
    if (m_store.getWorkspace(m_activeWorkspaceId, ws) && !ws.tagFilter.isEmpty()) {
        for (QString p : ws.tagFilter.split(',', Qt::SkipEmptyParts)) {
            p = p.trimmed();
            if (!p.startsWith('-')) {
                const QString norm = Store::normalizeTag(p);
                if (!norm.isEmpty() && norm != "untagged" && !tags.contains(norm))
                    tags.append(norm);
            }
        }
    }

    const QString folder = (m_activeFolder == "archive") ? "archive" : "inbox";
    DraftRecord created = m_store.createDraft(initialContent, tags, folder, "markdown");
    if (m_activeFolder == "trash" || m_activeFolder == "flagged") {
        m_activeFolder = "inbox";
        emit filterChanged();
    }
    setCurrentDraftRecord(created, true);
    refreshDrafts();
    return created.uuid;
}

QString Backend::quickCaptureSave(const QString &text, const QString &tagsCsv, bool flagged, const QString &syntax) {
    QStringList tags;
    for (const QString &part : tagsCsv.split(',', Qt::SkipEmptyParts)) {
        const QString norm = Store::normalizeTag(part);
        if (!norm.isEmpty())
            tags.append(norm);
    }
    DraftRecord d = m_store.createDraft(text, tags, "inbox", syntax.isEmpty() ? "markdown" : syntax, flagged, "orange");
    if (!text.trimmed().isEmpty())
        m_store.saveVersion(d.uuid, d.content, false);
    refreshDrafts();
    return d.uuid;
}

void Backend::selectDraft(const QString &uuid) {
    if (uuid.isEmpty())
        return;
    flushPendingSave();
    if (!m_current.uuid.isEmpty() && m_current.uuid != uuid && !m_current.content.trimmed().isEmpty())
        m_store.saveVersion(m_current.uuid, m_current.content, true);

    DraftRecord d;
    if (!m_store.getDraft(uuid, d))
        return;
    m_store.touchAccessed(uuid);
    m_store.getDraft(uuid, d);
    setCurrentDraftRecord(d, true);
}

void Backend::selectNextDraft(int delta) {
    if (m_drafts.isEmpty())
        return;
    int idx = -1;
    for (int i = 0; i < m_drafts.size(); ++i) {
        if (m_drafts[i].toMap().value("uuid").toString() == m_current.uuid) {
            idx = i;
            break;
        }
    }
    const int next = (idx < 0) ? 0 : qBound(0, idx + delta, m_drafts.size() - 1);
    selectDraft(m_drafts[next].toMap().value("uuid").toString());
}

void Backend::updateContent(const QString &text) {
    if (m_current.uuid.isEmpty()) {
        DraftRecord d = m_store.createDraft(text);
        setCurrentDraftRecord(d, false);
        refreshDrafts();
        return;
    }
    if (m_current.content == text)
        return;
    m_current.content = text;
    m_dirty = true;
    m_saveTimer.start();
    m_versionTimer.start();
    emit currentDraftChanged();
    if (m_previewVisible) {
        m_previewHtml = MarkdownProcessor::toHtml(m_current.content, m_accentColor, false);
        emit previewChanged();
    }
}

void Backend::setCursorPos(int pos) {
    if (m_current.uuid.isEmpty() || m_current.cursorPos == pos)
        return;
    m_current.cursorPos = pos;
    m_store.setDraftCursor(m_current.uuid, pos);
}

void Backend::toggleFlag(const QString &uuid) {
    const QString target = uuid.isEmpty() ? m_current.uuid : uuid;
    if (target.isEmpty())
        return;
    flushPendingSave();
    DraftRecord d;
    if (!m_store.getDraft(target, d))
        return;
    m_store.setDraftFlagged(target, !d.flagged);
    if (target == m_current.uuid) {
        m_store.getDraft(target, m_current);
        emit currentDraftChanged();
    }
    refreshDrafts();
    showToast(d.flagged ? tr("Unflagged draft") : tr("Flagged draft"), "info");
}

void Backend::setFlagColor(const QString &color, const QString &uuid) {
    const QString target = uuid.isEmpty() ? m_current.uuid : uuid;
    if (target.isEmpty())
        return;
    flushPendingSave();
    m_store.setDraftFlagged(target, true, color);
    if (target == m_current.uuid) {
        m_store.getDraft(target, m_current);
        emit currentDraftChanged();
    }
    refreshDrafts();
}

void Backend::setSyntax(const QString &syntax) {
    if (m_current.uuid.isEmpty() || m_current.syntax == syntax)
        return;
    flushPendingSave();
    m_current.syntax = syntax;
    m_store.setDraftSyntax(m_current.uuid, syntax);
    refreshDrafts();
    refreshCurrentMeta();
    emit currentDraftChanged();
}

void Backend::archiveDraft(const QString &uuid) {
    const QString target = uuid.isEmpty() ? m_current.uuid : uuid;
    if (target.isEmpty())
        return;
    flushPendingSave();
    m_store.setDraftFolder(target, "archive");
    if (target == m_current.uuid) {
        m_store.getDraft(target, m_current);
        emit currentDraftChanged();
    }
    refreshDrafts();
    showToast(tr("Moved to Archive"), "success");
}

void Backend::moveDraftToInbox(const QString &uuid) {
    const QString target = uuid.isEmpty() ? m_current.uuid : uuid;
    if (target.isEmpty())
        return;
    flushPendingSave();
    m_store.setDraftFolder(target, "inbox");
    if (target == m_current.uuid) {
        m_store.getDraft(target, m_current);
        emit currentDraftChanged();
    }
    refreshDrafts();
    showToast(tr("Moved to Inbox"), "info");
}

void Backend::trashDraft(const QString &uuid) {
    const QString target = uuid.isEmpty() ? m_current.uuid : uuid;
    if (target.isEmpty())
        return;
    flushPendingSave();
    m_store.setDraftFolder(target, "trash");
    refreshDrafts();
    if (target == m_current.uuid) {
        if (!m_drafts.isEmpty())
            selectDraft(m_drafts.first().toMap().value("uuid").toString());
        else
            newDraft();
    }
    showToast(tr("Moved to Trash"), "info");
}

void Backend::deleteDraftPermanently(const QString &uuid) {
    const QString target = uuid.isEmpty() ? m_current.uuid : uuid;
    if (target.isEmpty())
        return;
    m_store.deleteDraftPermanently(target);
    refreshDrafts();
    if (target == m_current.uuid) {
        if (!m_drafts.isEmpty())
            selectDraft(m_drafts.first().toMap().value("uuid").toString());
        else
            newDraft();
    }
    showToast(tr("Deleted permanently"), "info");
}

void Backend::emptyTrash() {
    const int removed = m_store.emptyTrash();
    refreshDrafts();
    if (m_current.folder == "trash") {
        if (!m_drafts.isEmpty())
            selectDraft(m_drafts.first().toMap().value("uuid").toString());
        else
            newDraft();
    }
    showToast(tr("Emptied trash (%1 removed)").arg(removed), "info");
}

void Backend::duplicateCurrentDraft() {
    if (m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    DraftRecord dup = m_store.createDraft(m_current.content, m_current.tags, m_current.folder,
                                          m_current.syntax, m_current.flagged, m_current.flagColor);
    setCurrentDraftRecord(dup, true);
    refreshDrafts();
    showToast(tr("Duplicated draft"), "success");
}

// ---------------------------------------------------------------------------
// Tags
// ---------------------------------------------------------------------------

void Backend::addTag(const QString &tag) {
    if (m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    if (m_store.addTagToDraft(m_current.uuid, tag)) {
        m_store.getDraft(m_current.uuid, m_current);
        refreshDrafts();
        emit currentDraftChanged();
    }
}

void Backend::removeTag(const QString &tag) {
    if (m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    if (m_store.removeTagFromDraft(m_current.uuid, tag)) {
        m_store.getDraft(m_current.uuid, m_current);
        refreshDrafts();
        emit currentDraftChanged();
    }
}

void Backend::toggleFilterTag(const QString &tag) {
    const QString norm = tag.startsWith('-') ? ("-" + Store::normalizeTag(tag.mid(1))) : Store::normalizeTag(tag);
    if (norm.isEmpty())
        return;
    if (m_filterTags.contains(norm))
        m_filterTags.removeAll(norm);
    else
        m_filterTags.append(norm);
    emit filterChanged();
    refreshDrafts();
}

void Backend::clearFilterTags() {
    if (m_filterTags.isEmpty())
        return;
    m_filterTags.clear();
    emit filterChanged();
    refreshDrafts();
}

void Backend::renameTagGlobally(const QString &oldTag, const QString &newTag) {
    if (m_store.renameTagGlobally(oldTag, newTag)) {
        if (!m_current.uuid.isEmpty())
            m_store.getDraft(m_current.uuid, m_current);
        refreshDrafts();
        emit currentDraftChanged();
        showToast(tr("Renamed tag %1 to %2").arg(oldTag, newTag), "success");
    }
}

void Backend::deleteTagGlobally(const QString &tag) {
    if (m_store.deleteTagGlobally(tag)) {
        m_filterTags.removeAll(tag);
        if (!m_current.uuid.isEmpty())
            m_store.getDraft(m_current.uuid, m_current);
        refreshDrafts();
        emit filterChanged();
        emit currentDraftChanged();
        showToast(tr("Deleted tag %1").arg(tag), "info");
    }
}

// ---------------------------------------------------------------------------
// Versions
// ---------------------------------------------------------------------------

void Backend::saveManualVersion() {
    if (m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    m_store.saveVersion(m_current.uuid, m_current.content, false);
    refreshCurrentMeta();
    showToast(tr("Saved version snapshot"), "success");
}

void Backend::restoreVersion(qint64 versionId) {
    if (m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    DraftRecord updated;
    if (m_store.restoreVersion(m_current.uuid, versionId, updated)) {
        setCurrentDraftRecord(updated, true);
        refreshDrafts();
        showToast(tr("Restored version"), "success");
    }
}

// ---------------------------------------------------------------------------
// Filters & Workspaces
// ---------------------------------------------------------------------------

void Backend::setActiveFolder(const QString &folder) {
    if (m_activeFolder == folder)
        return;
    m_activeFolder = folder;
    emit filterChanged();
    refreshDrafts();
}

void Backend::setSearchQuery(const QString &query) {
    if (m_searchQuery == query)
        return;
    m_searchQuery = query;
    emit filterChanged();
    refreshDrafts();
}

void Backend::setActiveActionGroupId(const QString &id) {
    if (m_activeActionGroupId == id)
        return;
    m_activeActionGroupId = id;
    refreshActions();
}

void Backend::setActiveActionBarGroupId(const QString &id) {
    if (m_activeActionBarGroupId == id)
        return;
    m_activeActionBarGroupId = id;
    refreshActions();
}

void Backend::setActionSearchQuery(const QString &q) {
    if (m_actionSearchQuery == q)
        return;
    m_actionSearchQuery = q;
    refreshActions();
}

void Backend::selectWorkspace(const QString &id) {
    WorkspaceRecord ws;
    if (!m_store.getWorkspace(id, ws))
        return;
    m_activeWorkspaceId = ws.id;
    m_activeWorkspaceName = ws.name;
    m_activeFolder = ws.defaultFolder;
    m_filterTags.clear();
    if (!ws.actionGroupId.isEmpty())
        m_activeActionGroupId = ws.actionGroupId;
    if (!ws.actionBarGroupId.isEmpty())
        m_activeActionBarGroupId = ws.actionBarGroupId;
    emit filterChanged();
    refreshDrafts();
    refreshActions();
    showToast(tr("Workspace: %1").arg(ws.name), "info");
}

QString Backend::saveWorkspace(const QVariantMap &data) {
    WorkspaceRecord ws;
    ws.id = data.value("id").toString();
    ws.name = data.value("name").toString().trimmed();
    if (ws.name.isEmpty())
        return {};
    ws.tagFilter = data.value("tagFilter").toString().trimmed();
    ws.tagFilterMode = data.value("tagFilterMode", "all").toString();
    ws.query = data.value("query").toString().trimmed();
    ws.sortField = data.value("sortField", "modified").toString();
    ws.sortDescending = data.value("sortDescending", true).toBool();
    ws.showFlaggedFirst = data.value("showFlaggedFirst", true).toBool();
    ws.defaultFolder = data.value("defaultFolder", "inbox").toString();
    ws.actionGroupId = data.value("actionGroupId").toString();
    ws.actionBarGroupId = data.value("actionBarGroupId").toString();
    ws.position = data.value("position", m_workspaces.size()).toInt();

    ws = m_store.saveWorkspace(ws);
    refreshWorkspaces();
    if (ws.id == m_activeWorkspaceId) {
        m_activeWorkspaceName = ws.name;
        emit filterChanged();
        refreshDrafts();
    }
    showToast(tr("Saved workspace %1").arg(ws.name), "success");
    return ws.id;
}

void Backend::deleteWorkspace(const QString &id) {
    if (id == "ws-default")
        return;
    m_store.deleteWorkspace(id);
    if (m_activeWorkspaceId == id)
        selectWorkspace("ws-default");
    refreshWorkspaces();
    showToast(tr("Deleted workspace"), "info");
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void Backend::runAction(const QString &actionId, int selStart, int selLen) {
    ActionRecord action;
    if (!m_store.getAction(actionId, action)) {
        if (!m_store.findActionByName(actionId, action)) {
            showToast(tr("Action not found"), "error");
            return;
        }
    }

    flushPendingSave();
    ActionRunInput in;
    in.draft = m_current;
    in.selectionStart = selStart;
    in.selectionLength = selLen;
    in.accentColor = m_accentColor;

    const ActionRunResult res = m_actionEngine.execute(action, in);
    refreshActionLogs();

    if (!res.ok) {
        showToast(res.message, "error");
        return;
    }

    if (res.draftModified) {
        setCurrentDraftRecord(res.updatedDraft, true);
        refreshDrafts();
        if (res.newSelectionStart >= 0)
            emit editorSelectionRequested(res.newSelectionStart, res.newSelectionLength);
    }

    if (!res.htmlPreview.isEmpty()) {
        m_previewHtml = res.htmlPreview;
        m_previewVisible = true;
        emit previewChanged();
    }

    if (res.newDraftRequested) {
        newDraft();
    } else if (!res.loadDraftUuid.isEmpty()) {
        selectDraft(res.loadDraftUuid);
    }

    showToast(res.message, res.cancelled ? "info" : "success");
}

QString Backend::saveAction(const QVariantMap &data) {
    ActionRecord a;
    a.id = data.value("id").toString();
    a.groupId = data.value("groupId", m_activeActionGroupId).toString();
    a.name = data.value("name").toString().trimmed();
    if (a.name.isEmpty())
        return {};
    a.description = data.value("description").toString().trimmed();
    a.icon = data.value("icon", "⚡").toString();
    a.tint = data.value("tint", m_accentColor).toString();
    a.shortcut = data.value("shortcut").toString().trimmed();
    a.afterSuccess = data.value("afterSuccess", "nothing").toString();
    a.position = data.value("position", m_actions.size()).toInt();

    const QVariantList stepList = data.value("steps").toList();
    for (const QVariant &sv : stepList) {
        const QVariantMap sm = sv.toMap();
        ActionStep step;
        step.type = sm.value("type").toString();
        step.config = sm.value("config").toMap();
        if (!step.type.isEmpty())
            a.steps.append(step);
    }

    a = m_store.saveAction(a);
    refreshActions();
    showToast(tr("Saved action %1").arg(a.name), "success");
    return a.id;
}

void Backend::deleteAction(const QString &id) {
    m_store.deleteAction(id);
    refreshActions();
    showToast(tr("Deleted action"), "info");
}

QString Backend::saveActionGroup(const QString &id, const QString &name, const QString &icon) {
    if (name.trimmed().isEmpty())
        return {};
    ActionGroupRecord g;
    g.id = id;
    g.name = name.trimmed();
    g.icon = icon.isEmpty() ? QStringLiteral("⚡") : icon;
    g.position = m_actionGroups.size();
    g = m_store.saveActionGroup(g);
    refreshActions();
    return g.id;
}

void Backend::deleteActionGroup(const QString &id) {
    if (m_actionGroups.size() <= 1)
        return;
    m_store.deleteActionGroup(id);
    refreshActions();
}

QString Backend::exportActionJson(const QString &id) const {
    return m_store.exportActionJson(id);
}

bool Backend::importActionJson(const QString &jsonText, const QString &groupId) {
    ActionRecord out;
    QString err;
    if (!m_store.importActionJson(jsonText, groupId.isEmpty() ? m_activeActionGroupId : groupId, out, &err)) {
        showToast(tr("Import failed: %1").arg(err), "error");
        return false;
    }
    refreshActions();
    showToast(tr("Imported action %1").arg(out.name), "success");
    return true;
}

void Backend::clearActionLogs() {
    m_store.clearActionLogs();
    refreshActionLogs();
}

// ---------------------------------------------------------------------------
// Modes, Tasks, Wiki-Links, Arrange, Preview, Quick Search, Find & Replace
// ---------------------------------------------------------------------------

void Backend::setFocusMode(bool v) {
    if (m_focusMode == v)
        return;
    m_focusMode = v;
    emit modesChanged();
}

void Backend::setLinkMode(bool v) {
    if (m_linkMode == v)
        return;
    m_linkMode = v;
    emit modesChanged();
    emit currentDraftChanged();
}

void Backend::setTypewriterMode(bool v) {
    if (m_typewriterMode == v)
        return;
    m_typewriterMode = v;
    m_store.setSetting("typewriterMode", v ? "1" : "0");
    emit modesChanged();
}

void Backend::setShowDraftList(bool v) {
    if (m_showDraftList == v)
        return;
    m_showDraftList = v;
    emit modesChanged();
}

void Backend::setShowActionPanel(bool v) {
    if (m_showActionPanel == v)
        return;
    m_showActionPanel = v;
    emit modesChanged();
}

QString Backend::linkModeHtml() const {
    return MarkdownProcessor::toLinkModeHtml(m_current.content, m_accentColor);
}

void Backend::setPreviewVisible(bool v) {
    if (m_previewVisible == v)
        return;
    m_previewVisible = v;
    if (v)
        m_previewHtml = MarkdownProcessor::toHtml(m_current.content, m_accentColor, false);
    emit previewChanged();
}

void Backend::toggleTaskAtLine(int lineIndex) {
    QStringList lines = m_current.content.split('\n');
    if (lineIndex < 0 || lineIndex >= lines.size())
        return;
    QString &line = lines[lineIndex];
    static const QRegularExpression openRe(R"(^(\s*[-*+]\s+)\[ \])");
    static const QRegularExpression doneRe(R"(^(\s*[-*+]\s+)\[[xX]\])");
    if (openRe.match(line).hasMatch())
        line.replace(openRe, "\\1[x]");
    else if (doneRe.match(line).hasMatch())
        line.replace(doneRe, "\\1[ ]");
    else
        line = "- [ ] " + line;

    m_current.content = lines.join('\n');
    m_store.updateDraft(m_current, true);
    setCurrentDraftRecord(m_current, true);
    refreshDrafts();
}

void Backend::toggleTaskAtCursor(int cursorPosition) {
    const int pos = qBound(0, cursorPosition, m_current.content.size());
    const int lineIndex = m_current.content.left(pos).count('\n');
    toggleTaskAtLine(lineIndex);
}

void Backend::openLink(const QString &href) {
    if (href.startsWith("drafts-task:")) {
        bool ok = false;
        const int lineIdx = href.mid(12).toInt(&ok);
        if (ok)
            toggleTaskAtLine(lineIdx);
        return;
    }
    if (href.startsWith("drafts-wiki:")) {
        const QString raw = QUrl::fromPercentEncoding(href.mid(12).toUtf8());
        openWikiLink(raw);
        return;
    }
    if (href.startsWith("drafts://")) {
        handleDraftsUrl(href);
        return;
    }
    QDesktopServices::openUrl(QUrl(href));
}

void Backend::openWikiLink(const QString &raw) {
    const auto refs = MarkdownProcessor::extractWikiLinks("[[" + raw + "]]");
    if (refs.isEmpty())
        return;
    const WikiLinkRef &ref = refs.first();

    if (ref.kind == "search") {
        setSearchQuery(ref.target);
        setShowDraftList(true);
        showToast(tr("Searching for %1").arg(ref.target), "info");
        return;
    }
    if (ref.kind == "workspace") {
        WorkspaceRecord ws;
        if (m_store.findWorkspaceByName(ref.target, ws))
            selectWorkspace(ws.id);
        else
            showToast(tr("Workspace not found: %1").arg(ref.target), "error");
        return;
    }
    if (ref.kind == "url") {
        QDesktopServices::openUrl(QUrl(ref.target));
        return;
    }

    // Draft cross-link: open existing draft or create a new one with that heading!
    DraftRecord target;
    if (m_store.findDraftByTitle(ref.target, target)) {
        selectDraft(target.uuid);
        if (!ref.heading.isEmpty()) {
            const auto outlineItems = MarkdownProcessor::extractOutline(target.content, target.syntax);
            for (const OutlineItem &item : outlineItems) {
                if (item.title.compare(ref.heading, Qt::CaseInsensitive) == 0) {
                    emit editorCursorJumpRequested(item.position);
                    break;
                }
            }
        }
        showToast(tr("Opened [[%1]]").arg(target.displayTitle()), "info");
    } else {
        newDraft("# " + ref.target + "\n\n");
        showToast(tr("Created [[%1]]").arg(ref.target), "success");
    }
}

QStringList Backend::splitForArrange(const QString &mode) const {
    if (mode == "paragraphs") {
        static const QRegularExpression paraSplit(R"(\n\s*\n)");
        QStringList blocks = m_current.content.split(paraSplit);
        return blocks;
    }
    return m_current.content.split('\n');
}

void Backend::commitArrange(const QStringList &blocks, const QString &mode) {
    if (m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    m_store.saveVersion(m_current.uuid, m_current.content, true);
    m_current.content = (mode == "paragraphs") ? blocks.join("\n\n") : blocks.join('\n');
    m_store.updateDraft(m_current, true);
    setCurrentDraftRecord(m_current, true);
    refreshDrafts();
    showToast(tr("Arranged draft"), "success");
}

void Backend::openMarkdownPreview() {
    m_previewHtml = MarkdownProcessor::toHtml(m_current.content, m_accentColor, false);
    m_previewVisible = true;
    emit previewChanged();
}

void Backend::copyPreviewHtml() {
    const QString html = MarkdownProcessor::toHtml(m_current.content, m_accentColor, false);
    copyToClipboard(html);
    showToast(tr("Copied HTML to clipboard"), "success");
}

void Backend::openPreviewInBrowser() {
    const QString fullHtml = MarkdownProcessor::toHtml(m_current.content, m_accentColor, true);
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/drafts-preview.html";
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(fullHtml.toUtf8());
        if (f.commit())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

QVariantList Backend::quickSearch(const QString &query) const {
    const QString q = query.trimmed();
    const QString lower = q.toLower();
    QVariantList results;

    // 1. Matching Workspaces
    for (const WorkspaceRecord &ws : m_store.listWorkspaces()) {
        if (lower.isEmpty() || ws.name.toLower().contains(lower)) {
            results.append(QVariantMap{
                {"kind", "workspace"},
                {"id", ws.id},
                {"title", ws.name},
                {"subtitle", ws.tagFilter.isEmpty() ? tr("Workspace") : tr("Workspace • %1").arg(ws.tagFilter)},
                {"badge", "WS"},
            });
        }
    }

    // 2. Matching Actions
    int actionHits = 0;
    for (const ActionRecord &a : m_store.listActions()) {
        if (!lower.isEmpty() && (a.name.toLower().contains(lower) || a.description.toLower().contains(lower))) {
            results.append(QVariantMap{
                {"kind", "action"},
                {"id", a.id},
                {"title", a.name},
                {"subtitle", a.description},
                {"badge", a.icon},
            });
            if (++actionHits >= 8)
                break;
        }
    }

    // 3. Matching Drafts (uses full Drafts query syntax!)
    DraftQuery dq;
    dq.folder = "all";
    dq.query = q;
    const auto drafts = m_store.queryDrafts(dq);
    const int limit = qMin(drafts.size(), 25);
    for (int i = 0; i < limit; ++i) {
        const DraftRecord &d = drafts[i];
        results.append(QVariantMap{
            {"kind", "draft"},
            {"id", d.uuid},
            {"title", d.displayTitle()},
            {"subtitle", d.excerpt().isEmpty() ? d.folder : d.excerpt()},
            {"badge", d.flagged ? "⚑" : d.folder.left(1).toUpper()},
            {"tags", d.tags},
        });
    }
    return results;
}

int Backend::replaceAllInCurrent(const QString &findText, const QString &replaceText,
                                 bool caseSensitive, bool useRegex) {
    if (findText.isEmpty() || m_current.uuid.isEmpty())
        return 0;
    flushPendingSave();
    QString text = m_current.content;
    int count = 0;

    if (useRegex) {
        QRegularExpression::PatternOptions opts =
            caseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(findText, opts);
        if (!re.isValid()) {
            showToast(tr("Invalid regular expression"), "error");
            return 0;
        }
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            it.next();
            ++count;
        }
        if (count > 0)
            text.replace(re, replaceText);
    } else {
        const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        count = text.count(findText, cs);
        if (count > 0)
            text.replace(findText, replaceText, cs);
    }

    if (count > 0) {
        m_store.saveVersion(m_current.uuid, m_current.content, true);
        m_current.content = text;
        m_store.updateDraft(m_current, true);
        setCurrentDraftRecord(m_current, true);
        refreshDrafts();
        showToast(tr("Replaced %1 occurrence(s)").arg(count), "success");
    }
    return count;
}

// ---------------------------------------------------------------------------
// Typography & Settings
// ---------------------------------------------------------------------------

void Backend::setFontFamily(const QString &f) {
    if (m_fontFamily == f)
        return;
    m_fontFamily = f;
    m_store.setSetting("fontFamily", f);
    emit settingsChanged();
}

void Backend::setFontSize(int sz) {
    sz = qBound(10, sz, 32);
    if (m_fontSize == sz)
        return;
    m_fontSize = sz;
    m_store.setSetting("fontSize", QString::number(sz));
    emit settingsChanged();
}

void Backend::setLineHeight(double lh) {
    lh = qBound(1.1, lh, 2.4);
    if (qFuzzyCompare(m_lineHeight, lh))
        return;
    m_lineHeight = lh;
    m_store.setSetting("lineHeight", QString::number(lh, 'f', 2));
    emit settingsChanged();
}

void Backend::setEditorMaxWidth(int w) {
    w = qBound(480, w, 1600);
    if (m_editorMaxWidth == w)
        return;
    m_editorMaxWidth = w;
    m_store.setSetting("editorMaxWidth", QString::number(w));
    emit settingsChanged();
}

void Backend::setNewDraftTimeoutSecs(int secs) {
    secs = qMax(0, secs);
    if (m_newDraftTimeoutSecs == secs)
        return;
    m_newDraftTimeoutSecs = secs;
    m_store.setSetting("newDraftTimeoutSecs", QString::number(secs));
    emit settingsChanged();
}

void Backend::setNewDraftOnLaunch(bool v) {
    if (m_newDraftOnLaunch == v)
        return;
    m_newDraftOnLaunch = v;
    m_store.setSetting("newDraftOnLaunch", v ? "1" : "0");
    emit settingsChanged();
}

// ---------------------------------------------------------------------------
// URL Scheme (drafts://...)
// ---------------------------------------------------------------------------

bool Backend::handleDraftsUrl(const QString &urlString) {
    const QUrl url(urlString);
    if (url.scheme().compare("drafts", Qt::CaseInsensitive) != 0)
        return false;

    const QString host = url.host().toLower();
    const QUrlQuery q(url);

    if (host == "create") {
        const QString text = q.queryItemValue("text", QUrl::FullyDecoded);
        const QString tagParam = q.queryItemValue("tag", QUrl::FullyDecoded);
        QStringList tags;
        for (const QString &t : tagParam.split(',', Qt::SkipEmptyParts))
            tags.append(t.trimmed());
        const QString uuid = newDraft(text, tags);
        const QString actionName = q.queryItemValue("action", QUrl::FullyDecoded);
        if (!actionName.isEmpty())
            runAction(actionName);
        return !uuid.isEmpty();
    }

    if (host == "open") {
        const QString uuid = q.queryItemValue("uuid", QUrl::FullyDecoded);
        const QString title = q.queryItemValue("title", QUrl::FullyDecoded);
        if (!uuid.isEmpty()) {
            selectDraft(uuid);
            return m_current.uuid == uuid;
        }
        if (!title.isEmpty()) {
            DraftRecord d;
            if (m_store.findDraftByTitle(title, d)) {
                selectDraft(d.uuid);
                return true;
            }
        }
        return false;
    }

    if (host == "append" || host == "prepend") {
        const QString uuid = q.queryItemValue("uuid", QUrl::FullyDecoded);
        const QString text = q.queryItemValue("text", QUrl::FullyDecoded);
        DraftRecord d;
        if (!m_store.getDraft(uuid, d))
            return false;
        d.content = (host == "append") ? (d.content + "\n" + text) : (text + "\n" + d.content);
        m_store.updateDraft(d, true);
        selectDraft(d.uuid);
        refreshDrafts();
        return true;
    }

    if (host == "search") {
        setSearchQuery(q.queryItemValue("query", QUrl::FullyDecoded));
        return true;
    }

    if (host == "workspace") {
        WorkspaceRecord ws;
        if (m_store.findWorkspaceByName(q.queryItemValue("name", QUrl::FullyDecoded), ws)) {
            selectWorkspace(ws.id);
            return true;
        }
        return false;
    }

    if (host == "runaction") {
        const QString text = q.queryItemValue("text", QUrl::FullyDecoded);
        const QString actionName = q.queryItemValue("action", QUrl::FullyDecoded);
        if (!text.isEmpty())
            newDraft(text);
        if (!actionName.isEmpty()) {
            runAction(actionName);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Portal Import / Export / Auto Backup
// ---------------------------------------------------------------------------

void Backend::exportBackupDialog() {
    if (!m_filePicker) {
        createAutoBackupNow();
        return;
    }
    const QString suggested = QStringLiteral("drafts-backup-%1.json")
                                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"));
    m_filePicker->saveFile(tr("Export Drafts Backup"), suggested, tr("JSON Backup"), "*.json",
                           [this](const QString &path) {
                               if (path.isEmpty())
                                   return;
                               QString err;
                               if (m_store.exportBackupJson(path, &err))
                                   showToast(tr("Exported backup to %1").arg(QFileInfo(path).fileName()), "success");
                               else
                                   showToast(tr("Export failed: %1").arg(err), "error");
                           });
}

void Backend::importBackupDialog() {
    if (!m_filePicker)
        return;
    m_filePicker->openFile(tr("Import Drafts Backup"), tr("JSON Backup"), "*.json",
                           [this](const QString &path) {
                               if (path.isEmpty())
                                   return;
                               int dCnt = 0, aCnt = 0;
                               QString err;
                               if (m_store.importBackupJson(path, &dCnt, &aCnt, &err)) {
                                   refreshAll();
                                   showToast(tr("Imported %1 draft(s) and %2 action(s)").arg(dCnt).arg(aCnt), "success");
                               } else {
                                   showToast(tr("Import failed: %1").arg(err), "error");
                               }
                           });
}

void Backend::exportCurrentMarkdownDialog() {
    if (!m_filePicker || m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    m_filePicker->saveFile(tr("Export Markdown"), m_current.safeTitle() + ".md", tr("Markdown"), "*.md",
                           [this](const QString &path) {
                               if (path.isEmpty())
                                   return;
                               QSaveFile f(path);
                               if (f.open(QIODevice::WriteOnly) && f.write(m_current.content.toUtf8()) >= 0 && f.commit())
                                   showToast(tr("Exported %1").arg(QFileInfo(path).fileName()), "success");
                               else
                                   showToast(tr("Export failed: %1").arg(f.errorString()), "error");
                           });
}

void Backend::exportCurrentHtmlDialog() {
    if (!m_filePicker || m_current.uuid.isEmpty())
        return;
    flushPendingSave();
    m_filePicker->saveFile(tr("Export HTML"), m_current.safeTitle() + ".html", tr("HTML"), "*.html",
                           [this](const QString &path) {
                               if (path.isEmpty())
                                   return;
                               const QString html = MarkdownProcessor::toHtml(m_current.content, m_accentColor, true);
                               QSaveFile f(path);
                               if (f.open(QIODevice::WriteOnly) && f.write(html.toUtf8()) >= 0 && f.commit())
                                   showToast(tr("Exported %1").arg(QFileInfo(path).fileName()), "success");
                               else
                                   showToast(tr("Export failed: %1").arg(f.errorString()), "error");
                           });
}

void Backend::createAutoBackupNow() {
    const QString dir = QFileInfo(m_store.dbPath()).absolutePath() + "/backups";
    QDir().mkpath(dir);
    const QString path = QStringLiteral("%1/drafts-backup-%2.json")
                             .arg(dir, QDateTime::currentDateTime().toString("yyyy-MM-dd-HHmmss"));
    QString err;
    if (m_store.exportBackupJson(path, &err))
        showToast(tr("Backup saved to %1").arg(path), "success");
    else
        showToast(tr("Backup failed: %1").arg(err), "error");
}

void Backend::copyToClipboard(const QString &text) {
    if (auto *cb = QGuiApplication::clipboard())
        cb->setText(text);
}

void Backend::showToast(const QString &message, const QString &kind) {
    m_statusMessage = message;
    m_statusKind = kind;
    ++m_statusRevision;
    emit statusChanged();
}
