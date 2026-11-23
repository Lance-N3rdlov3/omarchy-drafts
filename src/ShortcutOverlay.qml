import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: helpRoot
    color: Qt.rgba(0, 0, 0, 0.72)
    z: 70

    signal closeRequested()

    readonly property var shortcuts: [
        { key: "Ctrl+N",           desc: qsTr("New blank draft") },
        { key: "Ctrl+K",           desc: qsTr("Quick Search (drafts, actions, workspaces)") },
        { key: "Ctrl+1 / Ctrl+2",  desc: qsTr("Toggle Draft List / Action Panel") },
        { key: "F11 / Ctrl+Shift+F", desc: qsTr("Toggle Focus Mode") },
        { key: "Ctrl+L",           desc: qsTr("Toggle Link Mode (clickable links & tasks)") },
        { key: "Ctrl+Shift+A",     desc: qsTr("Open Arrange Mode (reorder lines/paragraphs)") },
        { key: "Ctrl+Shift+P",     desc: qsTr("Toggle Markdown / HTML Preview") },
        { key: "Ctrl+I",           desc: qsTr("Toggle Inspector (Outline, Backlinks, Versions)") },
        { key: "Ctrl+F / Ctrl+H",  desc: qsTr("Find & Replace in draft") },
        { key: "Ctrl+S",           desc: qsTr("Save manual version snapshot") },
        { key: "Ctrl+Shift+L",     desc: qsTr("Toggle flag on current draft") },
        { key: "Ctrl+Shift+T",     desc: qsTr("Toggle Markdown task checkbox at cursor") },
        { key: "Ctrl+Shift+E",     desc: qsTr("Archive current draft") },
        { key: "Ctrl+Up / Down",   desc: qsTr("Previous / Next draft in list") },
        { key: "Ctrl+,",           desc: qsTr("Editor Settings & Backup") },
        { key: "? / F1",           desc: qsTr("Show / hide this shortcuts overlay") },
        { key: "Esc",              desc: qsTr("Close overlay or unfocus editor") },
        { key: "Ctrl+Q / Q",       desc: qsTr("Quit Drafts") }
    ]

    MouseArea { anchors.fill: parent; onClicked: helpRoot.closeRequested() }

    Rectangle {
        width: Math.min(560, parent.width - 48)
        height: Math.min(600, parent.height - 64)
        anchors.centerIn: parent
        radius: theme.radius
        color: theme.surface
        border.color: theme.border
        border.width: 1

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Keyboard Shortcuts"); color: theme.foreground; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                ToolButton { text: "✕"; onClicked: helpRoot.closeRequested() }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: scCol.implicitHeight

                ColumnLayout {
                    id: scCol
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: helpRoot.shortcuts
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Rectangle {
                                Layout.preferredWidth: 155
                                Layout.preferredHeight: 24
                                radius: Math.min(4, theme.radius)
                                color: theme.surfaceRaised
                                border.color: theme.border; border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.key
                                    color: theme.accent
                                    font.family: "monospace"
                                    font.pixelSize: 11; font.bold: true
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.desc
                                color: theme.foreground
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
}
