import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: overlay
    color: Qt.rgba(0, 0, 0, 0.65)
    z: 60

    signal closeRequested()
    signal actionTriggered(string actionId)

    property var results: []
    property int selectedIndex: 0

    onVisibleChanged: {
        if (visible) {
            qsInput.text = ""
            refreshResults()
            qsInput.forceActiveFocus()
        }
    }

    function refreshResults() {
        overlay.results = backend.quickSearch(qsInput.text)
        overlay.selectedIndex = 0
    }

    function activateCurrent() {
        if (selectedIndex < 0 || selectedIndex >= results.length) return
        var item = results[selectedIndex]
        if (item.kind === "draft") {
            backend.selectDraft(item.id)
        } else if (item.kind === "workspace") {
            backend.selectWorkspace(item.id)
        } else if (item.kind === "action") {
            overlay.actionTriggered(item.id)
        }
        overlay.closeRequested()
    }

    MouseArea {
        anchors.fill: parent
        onClicked: overlay.closeRequested()
    }

    Rectangle {
        width: Math.min(640, parent.width - 48)
        height: Math.min(480, parent.height - 96)
        anchors.horizontalCenter: parent.horizontalCenter
        y: 64
        radius: theme.radius
        color: theme.surface
        border.color: theme.accent
        border.width: 1

        MouseArea { anchors.fill: parent } // swallow backdrop clicks

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                spacing: 8
                Text { text: "⌕"; color: theme.accent; font.pixelSize: 18 }
                TextInput {
                    id: qsInput
                    Layout.fillWidth: true
                    color: theme.foreground
                    selectionColor: theme.accent
                    selectedTextColor: theme.accentForeground
                    font.pixelSize: 15
                    onTextChanged: overlay.refreshResults()
                    Keys.onDownPressed: overlay.selectedIndex = Math.min(overlay.results.length - 1, overlay.selectedIndex + 1)
                    Keys.onUpPressed: overlay.selectedIndex = Math.max(0, overlay.selectedIndex - 1)
                    Keys.onReturnPressed: overlay.activateCurrent()
                    Keys.onEnterPressed: overlay.activateCurrent()
                    Text {
                        anchors.fill: parent
                        text: qsTr("Search drafts, run actions, or switch workspace…")
                        color: theme.mutedForeground
                        font.pixelSize: 15
                        visible: parent.text.length === 0
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: overlay.results
                currentIndex: overlay.selectedIndex
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    width: ListView.view.width
                    height: 48
                    property bool active: index === overlay.selectedIndex

                    Rectangle {
                        anchors.fill: parent
                        color: active ? theme.surfaceRaised : "transparent"
                        Rectangle {
                            visible: active
                            width: 3; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                            color: theme.accent
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14; anchors.rightMargin: 14
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 28; Layout.preferredHeight: 24
                            radius: Math.min(4, theme.radius)
                            color: modelData.kind === "action" ? Qt.rgba(0.9, 0.55, 0.05, 0.2) : theme.background
                            border.color: theme.border; border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: modelData.badge
                                color: theme.accent
                                font.pixelSize: 11; font.bold: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: theme.foreground
                                font.pixelSize: 13; font.bold: active
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.subtitle
                                color: theme.mutedForeground
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            text: modelData.kind === "action" ? qsTr("Action") : (modelData.kind === "workspace" ? qsTr("Workspace") : qsTr("Draft"))
                            color: theme.mutedForeground
                            font.pixelSize: 10
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: overlay.selectedIndex = index
                        onClicked: { overlay.selectedIndex = index; overlay.activateCurrent() }
                    }
                }
            }
        }
    }
}
