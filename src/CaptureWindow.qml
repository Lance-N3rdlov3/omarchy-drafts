import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Drafts 1.0

// Lightweight floating Quick Capture window (`drafts capture`) for instant
// capture from a Hyprland global keybinding.
ApplicationWindow {
    id: capWin
    width: 600
    height: 360
    minimumWidth: 420
    minimumHeight: 240
    visible: true
    title: qsTr("Quick Capture — Drafts")
    color: theme.background

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool flagged: false
    property string syntax: "markdown"

    function saveAndQuit() {
        if (captureEditor.text.trim().length > 0) {
            backend.quickCaptureSave(captureEditor.text, tagsField.text, capWin.flagged, capWin.syntax)
        }
        Qt.quit()
    }

    Shortcut { sequence: "Ctrl+Return"; context: Qt.ApplicationShortcut; onActivated: capWin.saveAndQuit() }
    Shortcut { sequence: "Ctrl+Enter"; context: Qt.ApplicationShortcut; onActivated: capWin.saveAndQuit() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }

    Component.onCompleted: captureEditor.forceActiveFocus()

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Quick Capture"); color: theme.accent; font.pixelSize: 13; font.bold: true }
                Item { Layout.fillWidth: true }
                Text { text: "Ctrl+Enter " + qsTr("Save") + "  •  Esc " + qsTr("Cancel"); color: theme.mutedForeground; font.pixelSize: 11 }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Math.min(4, theme.radius)
                color: theme.surface
                border.color: theme.border
                border.width: 1

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    contentWidth: width
                    contentHeight: Math.max(height, captureEditor.implicitHeight)

                    SyntaxHighlighter {
                        document: captureEditor.textDocument
                        syntax: capWin.syntax
                        accent: theme.accent
                        foreground: theme.foreground
                        muted: theme.mutedForeground
                    }

                    TextEdit {
                        id: captureEditor
                        width: parent.width
                        height: Math.max(parent.height, implicitHeight)
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.accentForeground
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        font.family: backend.fontFamily
                        font.pixelSize: backend.fontSize

                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            text: qsTr("Capture a thought, task, link, or snippet…")
                            color: theme.mutedForeground
                            font: captureEditor.font
                            visible: captureEditor.length === 0
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: tagsField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("Tags (comma-separated, e.g. work/ideas, status::todo)")
                    font.pixelSize: 12
                    Material.roundedScale: theme.radius
                }

                ToolButton {
                    text: "⚑"
                    highlighted: capWin.flagged
                    onClicked: capWin.flagged = !capWin.flagged
                }

                Button {
                    text: qsTr("Save Draft")
                    highlighted: true
                    Material.roundedScale: theme.radius
                    onClicked: capWin.saveAndQuit()
                }
            }
        }
    }
}
