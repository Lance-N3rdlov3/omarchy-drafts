import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: wsDialog
    color: Qt.rgba(0, 0, 0, 0.72)
    z: 65

    property var workspaceData: ({})
    signal closeRequested()

    property string wsId: ""

    function loadForm(obj) {
        wsId = obj.id || ""
        nameField.text = obj.name || ""
        tagsField.text = obj.tagFilter || ""
        queryField.text = obj.query || ""
        tagModeCombo.currentIndex = (obj.tagFilterMode === "any") ? 1 : 0
        var sorts = ["modified", "created", "accessed", "text"]
        sortCombo.currentIndex = Math.max(0, sorts.indexOf(obj.sortField || "modified"))
        descCheck.checked = (obj.sortDescending !== undefined) ? obj.sortDescending : true
        flaggedFirstCheck.checked = (obj.showFlaggedFirst !== undefined) ? obj.showFlaggedFirst : true
        var folders = ["inbox", "flagged", "archive", "all"]
        folderCombo.currentIndex = Math.max(0, folders.indexOf(obj.defaultFolder || "inbox"))

        actGrpCombo.currentIndex = 0
        barGrpCombo.currentIndex = 0
        for (var i = 0; i < backend.actionGroups.length; i++) {
            if (backend.actionGroups[i].id === obj.actionGroupId) actGrpCombo.currentIndex = i
            if (backend.actionGroups[i].id === obj.actionBarGroupId) barGrpCombo.currentIndex = i
        }
    }

    onVisibleChanged: {
        if (visible) loadForm(workspaceData || ({}))
    }

    MouseArea { anchors.fill: parent; onClicked: wsDialog.closeRequested() }

    Rectangle {
        width: Math.min(540, parent.width - 48)
        height: Math.min(580, parent.height - 64)
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
                Text { text: qsTr("Workspaces"); color: theme.foreground; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                Button {
                    text: qsTr("+ New Workspace")
                    Material.roundedScale: theme.radius
                    onClicked: wsDialog.loadForm({})
                }
                ToolButton { text: "✕"; onClicked: wsDialog.closeRequested() }
            }

            // Existing workspaces chips
            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                contentWidth: wsChips.implicitWidth
                clip: true
                flickableDirection: Flickable.HorizontalFlick
                Row {
                    id: wsChips
                    spacing: 6
                    Repeater {
                        model: backend.workspaces
                        delegate: Rectangle {
                            height: 26
                            width: chipLbl.implicitWidth + 16
                            radius: Math.min(4, theme.radius)
                            color: modelData.id === wsDialog.wsId ? theme.accent : theme.surfaceRaised
                            Text {
                                id: chipLbl
                                anchors.centerIn: parent
                                text: modelData.name
                                font.pixelSize: 11
                                color: modelData.id === wsDialog.wsId ? theme.accentForeground : theme.foreground
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: wsDialog.loadForm(modelData) }
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: formCol.implicitHeight + 12

                ColumnLayout {
                    id: formCol
                    width: parent.width
                    spacing: 10

                    TextField {
                        id: nameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Workspace Name")
                        Material.roundedScale: theme.radius
                    }

                    TextField {
                        id: tagsField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Tag Filter (comma-separated, prefix - to omit, e.g. work, -waiting)")
                        Material.roundedScale: theme.radius
                    }

                    TextField {
                        id: queryField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Default Search Query (optional)")
                        Material.roundedScale: theme.radius
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("Tag Match"); color: theme.mutedForeground; font.pixelSize: 11 }
                            ComboBox {
                                id: tagModeCombo
                                Layout.fillWidth: true
                                model: [qsTr("All tags (AND)"), qsTr("Any tag (OR)")]
                                Material.roundedScale: theme.radius
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("Default Folder"); color: theme.mutedForeground; font.pixelSize: 11 }
                            ComboBox {
                                id: folderCombo
                                Layout.fillWidth: true
                                model: ["inbox", "flagged", "archive", "all"]
                                Material.roundedScale: theme.radius
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("Sort By"); color: theme.mutedForeground; font.pixelSize: 11 }
                            ComboBox {
                                id: sortCombo
                                Layout.fillWidth: true
                                model: ["modified", "created", "accessed", "text"]
                                Material.roundedScale: theme.radius
                            }
                        }
                        CheckBox { id: descCheck; text: qsTr("Descending") }
                        CheckBox { id: flaggedFirstCheck; text: qsTr("Flagged First") }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("Linked Action Group"); color: theme.mutedForeground; font.pixelSize: 11 }
                            ComboBox {
                                id: actGrpCombo
                                Layout.fillWidth: true
                                model: backend.actionGroups
                                textRole: "name"
                                Material.roundedScale: theme.radius
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("Linked Action Bar"); color: theme.mutedForeground; font.pixelSize: 11 }
                            ComboBox {
                                id: barGrpCombo
                                Layout.fillWidth: true
                                model: backend.actionGroups
                                textRole: "name"
                                Material.roundedScale: theme.radius
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    visible: wsDialog.wsId.length > 0 && wsDialog.wsId !== "ws-default"
                    text: qsTr("Delete")
                    Material.roundedScale: theme.radius
                    onClicked: { backend.deleteWorkspace(wsDialog.wsId); wsDialog.closeRequested() }
                }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); Material.roundedScale: theme.radius; onClicked: wsDialog.closeRequested() }
                Button {
                    text: qsTr("Save Workspace")
                    highlighted: true
                    Material.roundedScale: theme.radius
                    onClicked: {
                        var sorts = ["modified", "created", "accessed", "text"]
                        var folders = ["inbox", "flagged", "archive", "all"]
                        var agId = (actGrpCombo.currentIndex >= 0 && actGrpCombo.currentIndex < backend.actionGroups.length)
                                   ? backend.actionGroups[actGrpCombo.currentIndex].id : ""
                        var bgId = (barGrpCombo.currentIndex >= 0 && barGrpCombo.currentIndex < backend.actionGroups.length)
                                   ? backend.actionGroups[barGrpCombo.currentIndex].id : ""
                        var savedId = backend.saveWorkspace({
                            id: wsDialog.wsId,
                            name: nameField.text,
                            tagFilter: tagsField.text,
                            tagFilterMode: tagModeCombo.currentIndex === 1 ? "any" : "all",
                            query: queryField.text,
                            sortField: sorts[sortCombo.currentIndex],
                            sortDescending: descCheck.checked,
                            showFlaggedFirst: flaggedFirstCheck.checked,
                            defaultFolder: folders[folderCombo.currentIndex],
                            actionGroupId: agId,
                            actionBarGroupId: bgId
                        })
                        if (savedId.length > 0) backend.selectWorkspace(savedId)
                        wsDialog.closeRequested()
                    }
                }
            }
        }
    }
}
