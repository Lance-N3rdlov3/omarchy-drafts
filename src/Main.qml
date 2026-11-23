import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Drafts 1.0

ApplicationWindow {
    id: win
    width: 1320
    height: 820
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    title: qsTr("Drafts")
    color: theme.background

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool quickSearchVisible: false
    property bool arrangeVisible: false
    property bool infoVisible: false
    property bool findVisible: false
    property bool settingsVisible: false
    property bool workspaceEditorVisible: false
    property bool actionEditorVisible: false
    property bool showActionLogs: false
    property var editingWorkspace: ({})
    property var editingAction: ({})
    property bool syncingEditor: true

    readonly property int smallRadius: Math.min(4, theme.radius)
    readonly property var flagColors: [
        { name: "orange", hex: "#f97316" },
        { name: "red",    hex: "#ef4444" },
        { name: "yellow", hex: "#eab308" },
        { name: "green",  hex: "#10b981" },
        { name: "blue",   hex: "#3b82f6" },
        { name: "purple", hex: "#a855f7" }
    ]

    function flagHex(name) {
        for (var i = 0; i < flagColors.length; i++) {
            if (flagColors[i].name === name) return flagColors[i].hex
        }
        return theme.accent
    }

    function textInputFocused() {
        var item = win.activeFocusItem
        if (!item) return false
        return (item instanceof TextEdit) || (item instanceof TextInput)
    }

    function openWorkspaceEditor(wsObj) {
        win.editingWorkspace = wsObj || ({
            id: "", name: "", tagFilter: "", tagFilterMode: "all", query: "",
            sortField: "modified", sortDescending: true, showFlaggedFirst: true,
            defaultFolder: "inbox", actionGroupId: backend.activeActionGroupId,
            actionBarGroupId: backend.activeActionBarGroupId
        })
        win.workspaceEditorVisible = true
    }

    function openActionEditor(actObj) {
        win.editingAction = actObj || ({
            id: "", groupId: backend.activeActionGroupId, name: "", description: "",
            icon: "⚡", tint: theme.accent, shortcut: "", afterSuccess: "nothing",
            steps: [{ type: "script", config: { code: "// Drafts JS API: draft, editor, app, Draft, ShellScript, HTTP\napp.displaySuccessMessage(draft.title);\n" } }]
        })
        win.actionEditorVisible = true
    }

    function runActionWithSelection(actionId) {
        var selStart = Math.min(editorArea.selectionStart, editorArea.selectionEnd)
        var selLen = Math.abs(editorArea.selectionEnd - editorArea.selectionStart)
        backend.runAction(actionId, selStart, selLen)
    }

    // --- Global Keyboard Shortcuts ---
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: { backend.newDraft(); editorArea.forceActiveFocus() } }
    Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; onActivated: win.quickSearchVisible = !win.quickSearchVisible }
    Shortcut { sequence: "Ctrl+Shift+Space"; context: Qt.ApplicationShortcut; onActivated: win.quickSearchVisible = !win.quickSearchVisible }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; onActivated: backend.showDraftList = !backend.showDraftList }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; onActivated: backend.showActionPanel = !backend.showActionPanel }
    Shortcut { sequence: "F11"; context: Qt.ApplicationShortcut; onActivated: backend.focusMode = !backend.focusMode }
    Shortcut { sequence: "Ctrl+Shift+F"; context: Qt.ApplicationShortcut; onActivated: backend.focusMode = !backend.focusMode }
    Shortcut { sequence: "Ctrl+L"; context: Qt.ApplicationShortcut; onActivated: backend.linkMode = !backend.linkMode }
    Shortcut { sequence: "Ctrl+Shift+A"; context: Qt.ApplicationShortcut; onActivated: win.arrangeVisible = !win.arrangeVisible }
    Shortcut { sequence: "Ctrl+Shift+P"; context: Qt.ApplicationShortcut; onActivated: backend.previewVisible ? (backend.previewVisible = false) : backend.openMarkdownPreview() }
    Shortcut { sequence: "Ctrl+I"; context: Qt.ApplicationShortcut; onActivated: win.infoVisible = !win.infoVisible }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: { win.findVisible = true; findField.forceActiveFocus() } }
    Shortcut { sequence: "Ctrl+H"; context: Qt.ApplicationShortcut; onActivated: { win.findVisible = true; replaceField.forceActiveFocus() } }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveManualVersion() }
    Shortcut { sequence: "Ctrl+Shift+L"; context: Qt.ApplicationShortcut; onActivated: backend.toggleFlag() }
    Shortcut { sequence: "Ctrl+Shift+T"; context: Qt.ApplicationShortcut; onActivated: backend.toggleTaskAtCursor(editorArea.cursorPosition) }
    Shortcut { sequence: "Ctrl+Shift+E"; context: Qt.ApplicationShortcut; onActivated: backend.archiveDraft() }
    Shortcut { sequence: "Ctrl+,"; context: Qt.ApplicationShortcut; onActivated: win.settingsVisible = !win.settingsVisible }
    Shortcut { sequence: "Ctrl+Down"; context: Qt.ApplicationShortcut; onActivated: backend.selectNextDraft(1) }
    Shortcut { sequence: "Ctrl+Up"; context: Qt.ApplicationShortcut; onActivated: backend.selectNextDraft(-1) }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        enabled: !win.textInputFocused()
        onActivated: win.helpVisible = !win.helpVisible
    }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: !win.textInputFocused() && !win.quickSearchVisible && !win.arrangeVisible && !win.actionEditorVisible && !win.workspaceEditorVisible
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible) win.helpVisible = false
            else if (win.quickSearchVisible) win.quickSearchVisible = false
            else if (win.arrangeVisible) win.arrangeVisible = false
            else if (backend.previewVisible) backend.previewVisible = false
            else if (win.actionEditorVisible) win.actionEditorVisible = false
            else if (win.workspaceEditorVisible) win.workspaceEditorVisible = false
            else if (win.settingsVisible) win.settingsVisible = false
            else if (win.infoVisible) win.infoVisible = false
            else if (win.findVisible) win.findVisible = false
            else if (backend.focusMode) backend.focusMode = false
            else rootContent.forceActiveFocus()
        }
    }

    Connections {
        target: backend
        function onEditorReloaded() {
            win.syncingEditor = true
            editorArea.text = backend.content
            var pos = Math.max(0, Math.min(backend.cursorPos, editorArea.length))
            editorArea.cursorPosition = pos
            win.syncingEditor = false
        }
        function onEditorSelectionRequested(start, length) {
            editorArea.select(start, start + length)
            editorArea.forceActiveFocus()
        }
        function onEditorCursorJumpRequested(position) {
            var pos = Math.max(0, Math.min(position, editorArea.length))
            editorArea.cursorPosition = pos
            editorArea.forceActiveFocus()
        }
        function onStatusChanged() {
            toastBanner.opacity = 1
            toastTimer.restart()
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            win.syncingEditor = true
            editorArea.text = backend.content
            editorArea.cursorPosition = Math.min(backend.cursorPos, editorArea.length)
            win.syncingEditor = false
            editorArea.forceActiveFocus()
        })
    }

    // Mirrored root so Arabic, Hebrew, Persian, and Urdu read right-to-left.
    Item {
        id: rootContent
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // =================================================================
            // Top Toolbar
            // =================================================================
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                color: theme.surface
                border.width: 0

                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 1; color: theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 6

                    // Toggle Draft List
                    ToolButton {
                        text: "◧"
                        font.pixelSize: 16
                        checked: backend.showDraftList && !backend.focusMode
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Toggle Draft List (Ctrl+1)")
                        onClicked: { backend.focusMode = false; backend.showDraftList = !backend.showDraftList }
                    }

                    // Workspace Switcher
                    ComboBox {
                        id: workspaceCombo
                        Layout.preferredWidth: 155
                        Layout.preferredHeight: 32
                        font.pixelSize: 13
                        Material.roundedScale: theme.radius
                        model: backend.workspaces
                        textRole: "name"
                        currentIndex: {
                            for (var i = 0; i < backend.workspaces.length; i++) {
                                if (backend.workspaces[i].id === backend.activeWorkspaceId) return i
                            }
                            return 0
                        }
                        onActivated: function(idx) {
                            if (idx >= 0 && idx < backend.workspaces.length)
                                backend.selectWorkspace(backend.workspaces[idx].id)
                        }
                    }

                    ToolButton {
                        text: "⚙"
                        font.pixelSize: 14
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Manage Workspaces")
                        onClicked: {
                            var current = null
                            for (var i = 0; i < backend.workspaces.length; i++) {
                                if (backend.workspaces[i].id === backend.activeWorkspaceId) {
                                    current = backend.workspaces[i]; break
                                }
                            }
                            win.openWorkspaceEditor(current)
                        }
                    }

                    // New Draft primary button
                    Rectangle {
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: newDraftRow.implicitWidth + 18
                        radius: win.smallRadius
                        color: newDraftMa.containsMouse ? Qt.lighter(theme.accent, 1.1) : theme.accent
                        Row {
                            id: newDraftRow
                            anchors.centerIn: parent
                            spacing: 5
                            Text { text: "+"; font.pixelSize: 15; font.bold: true; color: theme.accentForeground }
                            Text { text: qsTr("New"); font.pixelSize: 12; font.bold: true; color: theme.accentForeground; anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea {
                            id: newDraftMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { backend.newDraft(); editorArea.forceActiveFocus() }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Quick Search Pill
                    Rectangle {
                        Layout.preferredWidth: Math.min(340, win.width * 0.28)
                        Layout.preferredHeight: 30
                        radius: win.smallRadius
                        color: theme.surfaceRaised
                        border.color: qsMa.containsMouse ? theme.accent : theme.border
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10; anchors.rightMargin: 10
                            Text { text: "⌕"; color: theme.mutedForeground; font.pixelSize: 14 }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Quick Search drafts, actions, workspaces…")
                                color: theme.mutedForeground
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text { text: "Ctrl+K"; color: theme.mutedForeground; font.pixelSize: 11; font.family: "monospace" }
                        }
                        MouseArea {
                            id: qsMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: win.quickSearchVisible = true
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Mode buttons
                    ToolButton {
                        text: "⛓"
                        font.pixelSize: 15
                        highlighted: backend.linkMode
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Link Mode (Ctrl+L)")
                        onClicked: backend.linkMode = !backend.linkMode
                    }
                    ToolButton {
                        text: "⇅"
                        font.pixelSize: 15
                        highlighted: win.arrangeVisible
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Arrange Mode (Ctrl+Shift+A)")
                        onClicked: win.arrangeVisible = !win.arrangeVisible
                    }
                    ToolButton {
                        text: "◉"
                        font.pixelSize: 15
                        highlighted: backend.previewVisible
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Markdown Preview (Ctrl+Shift+P)")
                        onClicked: backend.previewVisible ? (backend.previewVisible = false) : backend.openMarkdownPreview()
                    }
                    ToolButton {
                        text: "⚲"
                        font.pixelSize: 15
                        highlighted: win.findVisible
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Find & Replace (Ctrl+F)")
                        onClicked: { win.findVisible = !win.findVisible; if (win.findVisible) findField.forceActiveFocus() }
                    }
                    ToolButton {
                        text: "ⓘ"
                        font.pixelSize: 15
                        highlighted: win.infoVisible
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Draft Info, Outline, Backlinks & Versions (Ctrl+I)")
                        onClicked: win.infoVisible = !win.infoVisible
                    }
                    ToolButton {
                        text: "⛶"
                        font.pixelSize: 15
                        highlighted: backend.focusMode
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Focus Mode (F11)")
                        onClicked: backend.focusMode = !backend.focusMode
                    }
                    ToolButton {
                        text: "☰"
                        font.pixelSize: 14
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Editor Settings & Backup (Ctrl+,)")
                        onClicked: win.settingsVisible = true
                    }
                    ToolButton {
                        text: "?"
                        font.pixelSize: 14
                        font.bold: true
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Keyboard Shortcuts (?)")
                        onClicked: win.helpVisible = !win.helpVisible
                    }
                    ToolButton {
                        text: "◨"
                        font.pixelSize: 16
                        checked: backend.showActionPanel && !backend.focusMode
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Toggle Action Panel (Ctrl+2)")
                        onClicked: { backend.focusMode = false; backend.showActionPanel = !backend.showActionPanel }
                    }
                }
            }

            // =================================================================
            // Main 3-Pane Workspace
            // =================================================================
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // -------------------------------------------------------------
                // LEFT PANE: Draft List, Folders & Tags
                // -------------------------------------------------------------
                Rectangle {
                    visible: backend.showDraftList && !backend.focusMode
                    Layout.preferredWidth: 306
                    Layout.fillHeight: true
                    color: theme.surface

                    Rectangle {
                        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
                        width: 1; color: theme.border
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.rightMargin: 1
                        spacing: 0

                        // Draft search input
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 8
                                radius: win.smallRadius
                                color: theme.surfaceRaised
                                border.color: listSearch.activeFocus ? theme.accent : theme.border
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8; anchors.rightMargin: 6
                                    spacing: 4
                                    Text { text: "⌕"; color: theme.mutedForeground; font.pixelSize: 13 }
                                    TextInput {
                                        id: listSearch
                                        Layout.fillWidth: true
                                        color: theme.foreground
                                        selectionColor: theme.accent
                                        selectedTextColor: theme.accentForeground
                                        font.pixelSize: 12
                                        clip: true
                                        text: backend.searchQuery
                                        onTextEdited: backend.searchQuery = text
                                        Text {
                                            anchors.fill: parent
                                            text: qsTr("Search drafts, -omit, tag:x, /re/")
                                            color: theme.mutedForeground
                                            font.pixelSize: 12
                                            visible: parent.text.length === 0
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Text {
                                        visible: backend.searchQuery.length > 0
                                        text: "×"
                                        color: theme.mutedForeground
                                        font.pixelSize: 15
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: { listSearch.text = ""; backend.searchQuery = "" }
                                        }
                                    }
                                }
                            }
                        }

                        // Folder Tabs: Inbox | Flagged | Archive | All | Trash
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 6; Layout.rightMargin: 6; Layout.bottomMargin: 6
                            spacing: 2

                            Repeater {
                                model: [
                                    { key: "inbox",   label: qsTr("Inbox") },
                                    { key: "flagged", label: qsTr("Flag") },
                                    { key: "archive", label: qsTr("Arch") },
                                    { key: "all",     label: qsTr("All") },
                                    { key: "trash",   label: qsTr("Trash") }
                                ]
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    radius: win.smallRadius
                                    property bool active: backend.activeFolder === modelData.key
                                    property int cnt: backend.folderCounts[modelData.key] || 0
                                    color: active ? theme.accent : (tabMa.containsMouse ? theme.surfaceRaised : "transparent")
                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 3
                                        Text {
                                            text: modelData.label
                                            font.pixelSize: 11
                                            font.bold: active
                                            color: active ? theme.accentForeground : theme.foreground
                                        }
                                        Text {
                                            visible: cnt > 0
                                            text: cnt
                                            font.pixelSize: 10
                                            color: active ? theme.accentForeground : theme.mutedForeground
                                            opacity: active ? 0.85 : 1.0
                                        }
                                    }
                                    MouseArea {
                                        id: tabMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.activeFolder = modelData.key
                                    }
                                }
                            }
                        }

                        // Tag Filter Strip
                        Rectangle {
                            visible: backend.allTags.length > 0 || backend.filterTags.length > 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            color: "transparent"

                            Flickable {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                contentWidth: tagFilterRow.implicitWidth
                                clip: true
                                flickableDirection: Flickable.HorizontalFlick

                                Row {
                                    id: tagFilterRow
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5

                                    Rectangle {
                                        visible: backend.filterTags.length > 0
                                        height: 22
                                        width: clearLbl.implicitWidth + 12
                                        radius: win.smallRadius
                                        color: "#3f1d1d"
                                        Text {
                                            id: clearLbl
                                            anchors.centerIn: parent
                                            text: qsTr("Clear")
                                            font.pixelSize: 11
                                            color: "#fca5a5"
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: backend.clearFilterTags()
                                        }
                                    }

                                    Repeater {
                                        model: backend.allTags
                                        delegate: Rectangle {
                                            property bool selected: backend.filterTags.indexOf(modelData.name) >= 0
                                            height: 22
                                            width: tLbl.implicitWidth + 14
                                            radius: win.smallRadius
                                            color: selected ? theme.accent : theme.surfaceRaised
                                            border.width: 1
                                            border.color: selected ? theme.accent : theme.border
                                            Text {
                                                id: tLbl
                                                anchors.centerIn: parent
                                                text: "#" + modelData.name + " " + modelData.count
                                                font.pixelSize: 11
                                                color: selected ? theme.accentForeground : theme.mutedForeground
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: backend.toggleFilterTag(modelData.name)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

                        // Trash banner when in Trash folder
                        Rectangle {
                            visible: backend.activeFolder === "trash" && backend.drafts.length > 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            color: "#2b1616"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10; anchors.rightMargin: 10
                                Text { text: qsTr("Trashed drafts"); color: "#fca5a5"; font.pixelSize: 11; Layout.fillWidth: true }
                                Text {
                                    text: qsTr("Empty Trash")
                                    color: "#f87171"; font.pixelSize: 11; font.bold: true
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.emptyTrash()
                                    }
                                }
                            }
                        }

                        // Drafts List
                        ListView {
                            id: draftsList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: backend.drafts
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            Text {
                                visible: draftsList.count === 0
                                anchors.centerIn: parent
                                text: qsTr("No drafts match")
                                color: theme.mutedForeground
                                font.pixelSize: 13
                            }

                            delegate: Item {
                                width: ListView.view.width
                                height: 74
                                property bool isCurrent: modelData.uuid === backend.currentUuid

                                Rectangle {
                                    anchors.fill: parent
                                    color: isCurrent ? theme.surfaceRaised : (rowMa.containsMouse ? Qt.rgba(1, 1, 1, 0.03) : "transparent")
                                    Rectangle {
                                        visible: isCurrent
                                        width: 3
                                        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left
                                        color: theme.accent
                                    }
                                    Rectangle {
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                                        height: 1; color: theme.border; opacity: 0.55
                                    }
                                }

                                MouseArea {
                                    id: rowMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        backend.selectDraft(modelData.uuid)
                                        editorArea.forceActiveFocus()
                                    }
                                }

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12; anchors.rightMargin: 10
                                    anchors.topMargin: 8; anchors.bottomMargin: 8
                                    spacing: 3

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text {
                                            visible: modelData.flagged
                                            text: "⚑"
                                            font.pixelSize: 12
                                            color: win.flagHex(modelData.flagColor)
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.title
                                            color: isCurrent ? theme.foreground : Qt.darker(theme.foreground, 1.06)
                                            font.pixelSize: 13
                                            font.bold: isCurrent
                                            elide: Text.ElideRight
                                        }
                                        // Quick row hover buttons
                                        Row {
                                            visible: rowMa.containsMouse
                                            spacing: 6
                                            Text {
                                                text: "⚑"
                                                font.pixelSize: 12
                                                color: modelData.flagged ? win.flagHex(modelData.flagColor) : theme.mutedForeground
                                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: backend.toggleFlag(modelData.uuid) }
                                            }
                                            Text {
                                                text: modelData.folder === "archive" ? "↩" : "⌸"
                                                font.pixelSize: 12
                                                color: theme.mutedForeground
                                                MouseArea {
                                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                    onClicked: modelData.folder === "archive" ? backend.moveDraftToInbox(modelData.uuid) : backend.archiveDraft(modelData.uuid)
                                                }
                                            }
                                            Text {
                                                text: "✕"
                                                font.pixelSize: 12
                                                color: "#f87171"
                                                MouseArea {
                                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                    onClicked: modelData.folder === "trash" ? backend.deleteDraftPermanently(modelData.uuid) : backend.trashDraft(modelData.uuid)
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.excerpt.length > 0 ? modelData.excerpt : qsTr("Empty draft")
                                        color: theme.mutedForeground
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text {
                                            text: {
                                                var _ = translator.language
                                                var d = new Date(modelData.modifiedAt)
                                                return isNaN(d.getTime()) ? "" : d.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
                                            }
                                            color: theme.mutedForeground
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.tags.length > 0 ? "#" + modelData.tags.join(" #") : ""
                                            color: theme.accent
                                            opacity: 0.85
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // CENTER PANE: Tag Bar, Find Bar, Editor Canvas, Action Bar, Status Bar
                // -------------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    // Tag & Metadata Bar
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: theme.background
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: theme.border; opacity: 0.6 }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14; anchors.rightMargin: 12
                            spacing: 6

                            // Flag button + Color dots
                            Rectangle {
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: 26
                                radius: win.smallRadius
                                color: backend.flagged ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: "⚑"
                                    font.pixelSize: 14
                                    color: backend.flagged ? win.flagHex(backend.flagColor) : theme.mutedForeground
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: function(mouse) {
                                        if (mouse.button === Qt.RightButton) flagColorMenu.open()
                                        else backend.toggleFlag()
                                    }
                                }
                                Menu {
                                    id: flagColorMenu
                                    Material.roundedScale: theme.radius
                                    Repeater {
                                        model: win.flagColors
                                        delegate: MenuItem {
                                            text: "⚑ " + modelData.name
                                            onTriggered: backend.setFlagColor(modelData.name)
                                        }
                                    }
                                }
                            }

                            // Folder pill (Archive / Inbox / Trash quick action)
                            Rectangle {
                                Layout.preferredHeight: 22
                                Layout.preferredWidth: folderPillText.implicitWidth + 14
                                radius: win.smallRadius
                                color: theme.surfaceRaised
                                border.color: theme.border; border.width: 1
                                Text {
                                    id: folderPillText
                                    anchors.centerIn: parent
                                    text: backend.folder === "archive" ? qsTr("Archive") : (backend.folder === "trash" ? qsTr("Trash") : qsTr("Inbox"))
                                    font.pixelSize: 11
                                    color: theme.mutedForeground
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: backend.folder === "inbox" ? backend.archiveDraft() : backend.moveDraftToInbox()
                                }
                            }

                            // Current Draft Tags + Inline Tag Adder
                            Flickable {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                contentWidth: tagEditorRow.implicitWidth
                                clip: true
                                flickableDirection: Flickable.HorizontalFlick

                                Row {
                                    id: tagEditorRow
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5

                                    Repeater {
                                        model: backend.tags
                                        delegate: Rectangle {
                                            height: 22
                                            width: pillRow.implicitWidth + 12
                                            radius: win.smallRadius
                                            color: Qt.rgba(0.9, 0.55, 0.05, 0.16)
                                            border.color: theme.accent
                                            border.width: 1
                                            Row {
                                                id: pillRow
                                                anchors.centerIn: parent
                                                spacing: 4
                                                Text { text: modelData; font.pixelSize: 11; color: theme.foreground }
                                                Text {
                                                    text: "×"
                                                    font.pixelSize: 12
                                                    color: theme.mutedForeground
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: backend.removeTag(modelData)
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    TextInput {
                                        id: tagInput
                                        width: Math.max(96, contentWidth + 16)
                                        height: 22
                                        verticalAlignment: TextInput.AlignVCenter
                                        color: theme.foreground
                                        selectionColor: theme.accent
                                        selectedTextColor: theme.accentForeground
                                        font.pixelSize: 12
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("+ add tag…")
                                            color: theme.mutedForeground
                                            font.pixelSize: 11
                                            visible: parent.text.length === 0
                                        }
                                        onAccepted: {
                                            if (text.trim().length > 0) {
                                                backend.addTag(text.trim())
                                                text = ""
                                            }
                                        }
                                        onTextEdited: {
                                            if (text.endsWith(",")) {
                                                var t = text.slice(0, -1).trim()
                                                if (t.length > 0) backend.addTag(t)
                                                text = ""
                                            }
                                        }
                                    }
                                }
                            }

                            // Syntax Selector
                            ComboBox {
                                id: syntaxCombo
                                Layout.preferredWidth: 118
                                Layout.preferredHeight: 26
                                font.pixelSize: 11
                                Material.roundedScale: theme.radius
                                model: [
                                    { key: "markdown",   label: "Markdown" },
                                    { key: "gfm",        label: "GitHub MD" },
                                    { key: "plain",      label: "Plain Text" },
                                    { key: "taskpaper",  label: "TaskPaper" },
                                    { key: "javascript", label: "JavaScript" },
                                    { key: "json",       label: "JSON" },
                                    { key: "html",       label: "HTML" }
                                ]
                                textRole: "label"
                                currentIndex: {
                                    for (var i = 0; i < model.length; i++)
                                        if (model[i].key === backend.syntax) return i
                                    return 0
                                }
                                onActivated: function(idx) {
                                    if (idx >= 0 && idx < model.length)
                                        backend.setSyntax(model[idx].key)
                                }
                            }
                        }
                    }

                    // Collapsible Find & Replace Bar
                    Rectangle {
                        visible: win.findVisible
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        color: theme.surface
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: theme.border }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12; anchors.rightMargin: 12
                            spacing: 6

                            TextField {
                                id: findField
                                Layout.preferredWidth: 190
                                Layout.preferredHeight: 30
                                placeholderText: qsTr("Find…")
                                font.pixelSize: 12
                                Material.roundedScale: theme.radius
                                onAccepted: {
                                    var pos = editorArea.text.toLowerCase().indexOf(text.toLowerCase(), editorArea.cursorPosition)
                                    if (pos < 0) pos = editorArea.text.toLowerCase().indexOf(text.toLowerCase())
                                    if (pos >= 0) {
                                        editorArea.select(pos, pos + text.length)
                                        editorArea.forceActiveFocus()
                                    }
                                }
                            }
                            TextField {
                                id: replaceField
                                Layout.preferredWidth: 180
                                Layout.preferredHeight: 30
                                placeholderText: qsTr("Replace…")
                                font.pixelSize: 12
                                Material.roundedScale: theme.radius
                            }
                            CheckBox {
                                id: findCaseCheck
                                text: qsTr("Aa")
                                font.pixelSize: 11
                            }
                            CheckBox {
                                id: findRegexCheck
                                text: qsTr("Regex")
                                font.pixelSize: 11
                            }
                            Button {
                                text: qsTr("Replace All")
                                font.pixelSize: 11
                                Material.roundedScale: theme.radius
                                onClicked: backend.replaceAllInCurrent(findField.text, replaceField.text, findCaseCheck.checked, findRegexCheck.checked)
                            }
                            Item { Layout.fillWidth: true }
                            ToolButton {
                                text: "✕"
                                font.pixelSize: 13
                                onClicked: { win.findVisible = false; editorArea.forceActiveFocus() }
                            }
                        }
                    }

                    // Main Editor Surface (Standard Edit Mode OR Link Mode)
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        // Standard Editing Mode
                        Flickable {
                            id: editorFlick
                            anchors.fill: parent
                            visible: !backend.linkMode
                            clip: true
                            contentWidth: width
                            contentHeight: Math.max(height, editorContainer.height + 160)
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            Item {
                                id: editorContainer
                                width: Math.min(parent.width - 48, backend.editorMaxWidth)
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: 20
                                height: Math.max(editorFlick.height - 60, editorArea.implicitHeight + 80)

                                SyntaxHighlighter {
                                    document: editorArea.textDocument
                                    syntax: backend.syntax
                                    accent: theme.accent
                                    foreground: theme.foreground
                                    muted: theme.mutedForeground
                                }

                                TextEdit {
                                    id: editorArea
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    topPadding: 8
                                    bottomPadding: 80
                                    leftPadding: 4
                                    rightPadding: 4
                                    color: theme.foreground
                                    selectionColor: theme.accent
                                    selectedTextColor: theme.accentForeground
                                    wrapMode: TextEdit.Wrap
                                    selectByMouse: true
                                    persistentSelection: true
                                    font.family: backend.fontFamily
                                    font.pixelSize: backend.fontSize

                                    Text {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.leftMargin: 4
                                        anchors.topMargin: 8
                                        text: qsTr("Start typing…")
                                        color: theme.mutedForeground
                                        font: editorArea.font
                                        visible: editorArea.length === 0
                                    }

                                    Component.onCompleted: {
                                        win.syncingEditor = true
                                        text = backend.content
                                        win.syncingEditor = false
                                    }

                                    onTextChanged: {
                                        if (!win.syncingEditor && text !== backend.content) {
                                            backend.updateContent(text)
                                        }
                                    }

                                    onCursorRectangleChanged: {
                                        if (!win.syncingEditor) {
                                            backend.setCursorPos(cursorPosition)
                                        }
                                        // Typewriter scrolling keeps active cursor vertically centered.
                                        if (backend.typewriterMode && editorArea.activeFocus) {
                                            var targetY = cursorRectangle.y + 20 - editorFlick.height * 0.45
                                            editorFlick.contentY = Math.max(0, Math.min(targetY, editorFlick.contentHeight - editorFlick.height))
                                        }
                                    }
                                }
                            }
                        }

                        // Link Mode (clickable URLs, emails, [[wiki-links]], and interactive checkboxes)
                        Flickable {
                            anchors.fill: parent
                            visible: backend.linkMode
                            clip: true
                            contentWidth: width
                            contentHeight: linkText.implicitHeight + 80
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            Column {
                                width: Math.min(parent.width - 48, backend.editorMaxWidth)
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: 20
                                spacing: 12

                                Rectangle {
                                    width: parent.width
                                    height: 30
                                    radius: win.smallRadius
                                    color: Qt.rgba(0.9, 0.55, 0.05, 0.14)
                                    border.color: theme.accent
                                    border.width: 1
                                    RowLayout {
                                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                                        Text { text: qsTr("Link Mode — click URLs, [[wiki-links]], or checkboxes directly"); color: theme.foreground; font.pixelSize: 12; Layout.fillWidth: true }
                                        Text {
                                            text: qsTr("Exit Link Mode")
                                            color: theme.accent; font.pixelSize: 11; font.bold: true
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: backend.linkMode = false }
                                        }
                                    }
                                }

                                Text {
                                    id: linkText
                                    width: parent.width
                                    textFormat: Text.RichText
                                    text: backend.linkModeHtml
                                    color: theme.foreground
                                    font.family: backend.fontFamily
                                    font.pixelSize: backend.fontSize
                                    lineHeight: backend.lineHeight
                                    wrapMode: Text.Wrap
                                    onLinkActivated: function(href) { backend.openLink(href) }
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // Action Bar (quick one-click action strip, Drafts-style)
                    // ---------------------------------------------------------
                    Rectangle {
                        visible: !backend.focusMode
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        color: theme.surface
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: theme.border }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8; anchors.rightMargin: 8
                            spacing: 6

                            // Action Bar Group switcher
                            ComboBox {
                                Layout.preferredWidth: 106
                                Layout.preferredHeight: 26
                                font.pixelSize: 11
                                Material.roundedScale: theme.radius
                                model: backend.actionGroups
                                textRole: "name"
                                currentIndex: {
                                    for (var i = 0; i < backend.actionGroups.length; i++)
                                        if (backend.actionGroups[i].id === backend.activeActionBarGroupId) return i
                                    return 0
                                }
                                onActivated: function(idx) {
                                    if (idx >= 0 && idx < backend.actionGroups.length)
                                        backend.activeActionBarGroupId = backend.actionGroups[idx].id
                                }
                            }

                            Flickable {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                contentWidth: actionBarRow.implicitWidth
                                clip: true
                                flickableDirection: Flickable.HorizontalFlick

                                Row {
                                    id: actionBarRow
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5

                                    Repeater {
                                        model: backend.actionBarActions
                                        delegate: Rectangle {
                                            height: 24
                                            width: abRow.implicitWidth + 14
                                            radius: win.smallRadius
                                            color: abMa.containsMouse ? theme.surfaceRaised : "transparent"
                                            border.color: theme.border
                                            border.width: 1

                                            Row {
                                                id: abRow
                                                anchors.centerIn: parent
                                                spacing: 4
                                                Text {
                                                    text: modelData.icon
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    color: modelData.tint.length > 0 ? modelData.tint : theme.accent
                                                }
                                                Text {
                                                    text: modelData.name
                                                    font.pixelSize: 11
                                                    color: theme.foreground
                                                }
                                            }
                                            MouseArea {
                                                id: abMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: win.runActionWithSelection(modelData.id)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // Status Bar
                    // ---------------------------------------------------------
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        color: theme.background
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: theme.border; opacity: 0.6 }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12; anchors.rightMargin: 12
                            spacing: 14

                            Text {
                                text: {
                                    var _ = translator.language
                                    return qsTr("%n words", "", backend.wordCount)
                                }
                                color: theme.mutedForeground; font.pixelSize: 11
                            }
                            Text {
                                text: {
                                    var _ = translator.language
                                    return qsTr("%n chars", "", backend.charCount)
                                }
                                color: theme.mutedForeground; font.pixelSize: 11
                            }
                            Text {
                                text: {
                                    var _ = translator.language
                                    return qsTr("%n lines", "", backend.lineCount)
                                }
                                color: theme.mutedForeground; font.pixelSize: 11
                            }
                            Text {
                                visible: backend.typewriterMode
                                text: qsTr("Typewriter")
                                color: theme.accent; font.pixelSize: 11; font.bold: true
                            }
                            Item { Layout.fillWidth: true }

                            // Live Toast Pill
                            Rectangle {
                                id: toastBanner
                                visible: opacity > 0.01 && backend.statusMessage.length > 0
                                opacity: 0
                                height: 20
                                width: toastLbl.implicitWidth + 16
                                radius: win.smallRadius
                                color: backend.statusKind === "error" ? "#7f1d1d" : (backend.statusKind === "success" ? "#14532d" : theme.surfaceRaised)
                                Behavior on opacity { NumberAnimation { duration: 180 } }
                                Text {
                                    id: toastLbl
                                    anchors.centerIn: parent
                                    text: backend.statusMessage
                                    color: theme.foreground
                                    font.pixelSize: 11
                                }
                                Timer {
                                    id: toastTimer
                                    interval: 3200
                                    onTriggered: toastBanner.opacity = 0
                                }
                            }

                            Text {
                                text: backend.activeWorkspaceName
                                color: theme.mutedForeground; font.pixelSize: 11
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // RIGHT PANE: Action Panel & Action Log
                // -------------------------------------------------------------
                Rectangle {
                    visible: backend.showActionPanel && !backend.focusMode
                    Layout.preferredWidth: 255
                    Layout.fillHeight: true
                    color: theme.surface

                    Rectangle {
                        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left
                        width: 1; color: theme.border
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 1
                        spacing: 0

                        // Header: Action Group selector + Add + Log
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 8
                            spacing: 4

                            ComboBox {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                font.pixelSize: 12
                                Material.roundedScale: theme.radius
                                model: backend.actionGroups
                                textRole: "name"
                                currentIndex: {
                                    for (var i = 0; i < backend.actionGroups.length; i++)
                                        if (backend.actionGroups[i].id === backend.activeActionGroupId) return i
                                    return 0
                                }
                                onActivated: function(idx) {
                                    if (idx >= 0 && idx < backend.actionGroups.length)
                                        backend.activeActionGroupId = backend.actionGroups[idx].id
                                }
                            }

                            ToolButton {
                                text: "+"
                                font.pixelSize: 15
                                ToolTip.visible: hovered; ToolTip.text: qsTr("New Action")
                                onClicked: win.openActionEditor(null)
                            }
                            ToolButton {
                                text: "◷"
                                font.pixelSize: 14
                                highlighted: win.showActionLogs
                                ToolTip.visible: hovered; ToolTip.text: qsTr("Action Log")
                                onClicked: win.showActionLogs = !win.showActionLogs
                            }
                        }

                        // Action filter input
                        Item {
                            visible: !win.showActionLogs
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8; anchors.bottomMargin: 6
                                radius: win.smallRadius
                                color: theme.surfaceRaised
                                border.color: actSearch.activeFocus ? theme.accent : theme.border
                                border.width: 1
                                TextInput {
                                    id: actSearch
                                    anchors.fill: parent
                                    anchors.leftMargin: 8; anchors.rightMargin: 8
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: theme.foreground
                                    font.pixelSize: 12
                                    text: backend.actionSearchQuery
                                    onTextEdited: backend.actionSearchQuery = text
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("Filter actions…")
                                        color: theme.mutedForeground
                                        font.pixelSize: 12
                                        visible: parent.text.length === 0
                                    }
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

                        // Actions List
                        ListView {
                            visible: !win.showActionLogs
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: backend.actions
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Item {
                                width: ListView.view.width
                                height: 52

                                Rectangle {
                                    anchors.fill: parent
                                    color: actMa.containsMouse ? theme.surfaceRaised : "transparent"
                                    Rectangle {
                                        width: 3
                                        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left
                                        color: modelData.tint.length > 0 ? modelData.tint : theme.accent
                                        opacity: actMa.containsMouse ? 1.0 : 0.6
                                    }
                                    Rectangle {
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                                        height: 1; color: theme.border; opacity: 0.45
                                    }
                                }

                                MouseArea {
                                    id: actMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: win.runActionWithSelection(modelData.id)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12; anchors.rightMargin: 8
                                    spacing: 8

                                    Rectangle {
                                        Layout.preferredWidth: 26
                                        Layout.preferredHeight: 26
                                        radius: win.smallRadius
                                        color: theme.surfaceRaised
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.icon
                                            font.pixelSize: 12
                                            font.bold: true
                                            color: modelData.tint.length > 0 ? modelData.tint : theme.accent
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            color: theme.foreground
                                            font.pixelSize: 12
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.description.length > 0 ? modelData.description : (modelData.steps.length + " step(s)")
                                            color: theme.mutedForeground
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Text {
                                        visible: actMa.containsMouse
                                        text: "✎"
                                        font.pixelSize: 13
                                        color: theme.mutedForeground
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: win.openActionEditor(modelData)
                                        }
                                    }
                                }
                            }
                        }

                        // Action Execution Log View
                        ColumnLayout {
                            visible: win.showActionLogs
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.margins: 8
                                Text { text: qsTr("Action Log"); color: theme.foreground; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true }
                                Text {
                                    text: qsTr("Clear")
                                    color: theme.accent; font.pixelSize: 11
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: backend.clearActionLogs() }
                                }
                            }

                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                model: backend.actionLogs
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: Item {
                                    width: ListView.view.width
                                    height: 54
                                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: theme.border; opacity: 0.4 }
                                    ColumnLayout {
                                        anchors.fill: parent; anchors.margins: 8; spacing: 2
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text { text: modelData.actionName; color: theme.foreground; font.pixelSize: 11; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                                            Text {
                                                text: modelData.status
                                                font.pixelSize: 10
                                                color: modelData.status === "error" ? "#f87171" : (modelData.status === "success" ? "#4ade80" : theme.mutedForeground)
                                            }
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.message
                                            color: theme.mutedForeground
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Overlays & Drawers
        // =====================================================================
        InfoDrawer {
            visible: win.infoVisible
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            onCloseRequested: win.infoVisible = false
            onJumpToPosition: function(pos) {
                var p = Math.max(0, Math.min(pos, editorArea.length))
                editorArea.cursorPosition = p
                editorArea.forceActiveFocus()
            }
        }

        PreviewOverlay {
            visible: backend.previewVisible
            anchors.fill: parent
            onCloseRequested: backend.previewVisible = false
        }

        ArrangeOverlay {
            visible: win.arrangeVisible
            anchors.fill: parent
            onCloseRequested: win.arrangeVisible = false
        }

        QuickSearchOverlay {
            visible: win.quickSearchVisible
            anchors.fill: parent
            onCloseRequested: { win.quickSearchVisible = false; editorArea.forceActiveFocus() }
            onActionTriggered: function(actionId) { win.runActionWithSelection(actionId) }
        }

        WorkspaceEditorDialog {
            visible: win.workspaceEditorVisible
            anchors.fill: parent
            workspaceData: win.editingWorkspace
            onCloseRequested: win.workspaceEditorVisible = false
        }

        ActionEditorDialog {
            visible: win.actionEditorVisible
            anchors.fill: parent
            actionData: win.editingAction
            onCloseRequested: win.actionEditorVisible = false
        }

        SettingsDialog {
            visible: win.settingsVisible
            anchors.fill: parent
            onCloseRequested: win.settingsVisible = false
        }

        ShortcutOverlay {
            visible: win.helpVisible
            anchors.fill: parent
            onCloseRequested: win.helpVisible = false
        }
    }
}
