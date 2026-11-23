import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Draft Info Drawer: Metadata, Document Outline, Backlinks, and Version History.
Rectangle {
    id: drawer
    width: 340
    color: theme.surface
    z: 50

    signal closeRequested()
    signal jumpToPosition(int position)

    property int activeTab: 0 // 0=Outline & Links, 1=Versions, 2=Info

    Rectangle {
        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left
        width: 1; color: theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 1
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            Text { text: qsTr("Draft Inspector"); color: theme.foreground; font.pixelSize: 14; font.bold: true; Layout.fillWidth: true }
            ToolButton { text: "✕"; onClicked: drawer.closeRequested() }
        }

        // Sub-tabs
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 8
            spacing: 4
            Repeater {
                model: [qsTr("Navigate"), qsTr("Versions"), qsTr("Info")]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    radius: Math.min(4, theme.radius)
                    color: drawer.activeTab === index ? theme.accent : theme.surfaceRaised
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: 11; font.bold: drawer.activeTab === index
                        color: drawer.activeTab === index ? theme.accentForeground : theme.foreground
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: drawer.activeTab = index }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

        // TAB 0: Outline & Backlinks
        Flickable {
            visible: drawer.activeTab === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: navCol.implicitHeight + 24
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: navCol
                width: parent.width - 20
                x: 10; y: 10
                spacing: 8

                Text { text: qsTr("Outline & Links"); color: theme.mutedForeground; font.pixelSize: 11; font.bold: true }

                Text {
                    visible: backend.outline.length === 0
                    text: qsTr("No headings or [[wiki-links]] in this draft")
                    color: theme.mutedForeground; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
                }

                Repeater {
                    model: backend.outline
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        radius: Math.min(4, theme.radius)
                        color: navMa.containsMouse ? theme.surfaceRaised : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6 + (modelData.level - 1) * 10
                            anchors.rightMargin: 6
                            spacing: 6
                            Text {
                                text: modelData.kind === "wikilink" ? "[[]]" : "#"
                                color: theme.accent; font.pixelSize: 11; font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: theme.foreground; font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea {
                            id: navMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modelData.kind === "wikilink") backend.openWikiLink(modelData.title)
                                else drawer.jumpToPosition(modelData.position)
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: theme.border; Layout.topMargin: 6 }

                Text {
                    text: qsTr("Backlinks (%1)").arg(backend.backlinks.length)
                    color: theme.mutedForeground; font.pixelSize: 11; font.bold: true
                }

                Text {
                    visible: backend.backlinks.length === 0
                    text: qsTr("No other drafts link to [[%1]]").arg(backend.title)
                    color: theme.mutedForeground; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
                }

                Repeater {
                    model: backend.backlinks
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: Math.min(4, theme.radius)
                        color: blMa.containsMouse ? theme.surfaceRaised : theme.background
                        border.color: theme.border; border.width: 1
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 1
                            Text { text: modelData.title; color: theme.accent; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { text: modelData.excerpt; color: theme.mutedForeground; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        MouseArea {
                            id: blMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.selectDraft(modelData.uuid)
                        }
                    }
                }
            }
        }

        // TAB 1: Version History
        ColumnLayout {
            visible: drawer.activeTab === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 10
                Text {
                    text: {
                        var _ = translator.language
                        return qsTr("%n versions", "", backend.versions.length)
                    }
                    color: theme.mutedForeground; font.pixelSize: 11; Layout.fillWidth: true
                }
                Button {
                    text: qsTr("Save Version")
                    font.pixelSize: 11
                    Material.roundedScale: theme.radius
                    onClicked: backend.saveManualVersion()
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: backend.versions
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    width: ListView.view.width
                    height: 82
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: theme.border; opacity: 0.5 }
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: {
                                    var _ = translator.language
                                    var d = new Date(modelData.createdAt)
                                    return isNaN(d.getTime()) ? modelData.createdAt : d.toLocaleString(Qt.locale(), Locale.ShortFormat)
                                }
                                color: theme.foreground; font.pixelSize: 11; font.bold: true; Layout.fillWidth: true
                            }
                            Text { text: modelData.charCount + " ch"; color: theme.mutedForeground; font.pixelSize: 10 }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.preview
                            color: theme.mutedForeground; font.pixelSize: 11
                            elide: Text.ElideRight; maximumLineCount: 1
                        }
                        RowLayout {
                            spacing: 10
                            Text {
                                text: qsTr("Restore")
                                color: theme.accent; font.pixelSize: 11; font.bold: true
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: backend.restoreVersion(modelData.id) }
                            }
                            Text {
                                text: qsTr("Copy")
                                color: theme.mutedForeground; font.pixelSize: 11
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: { backend.copyToClipboard(modelData.content); backend.showToast(qsTr("Copied version text"), "info") }
                                }
                            }
                        }
                    }
                }
            }
        }

        // TAB 2: Metadata & Quick Actions
        Flickable {
            visible: drawer.activeTab === 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: infoCol.implicitHeight + 24

            ColumnLayout {
                id: infoCol
                width: parent.width - 20
                x: 10; y: 12
                spacing: 8

                Text { text: qsTr("UUID"); color: theme.mutedForeground; font.pixelSize: 10; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: backend.currentUuid; color: theme.foreground; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Text {
                        text: qsTr("Copy")
                        color: theme.accent; font.pixelSize: 11
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { backend.copyToClipboard(backend.currentUuid); backend.showToast(qsTr("Copied UUID"), "info") } }
                    }
                }

                Text { text: qsTr("Permalink"); color: theme.mutedForeground; font.pixelSize: 10; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "drafts://open?uuid=" + backend.currentUuid; color: theme.foreground; font.family: "monospace"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Text {
                        text: qsTr("Copy")
                        color: theme.accent; font.pixelSize: 11
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { backend.copyToClipboard("drafts://open?uuid=" + backend.currentUuid); backend.showToast(qsTr("Copied permalink"), "info") } }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    rowSpacing: 4; columnSpacing: 8
                    Text { text: qsTr("Created"); color: theme.mutedForeground; font.pixelSize: 11 }
                    Text { text: backend.createdAt.slice(0, 19).replace("T", " "); color: theme.foreground; font.pixelSize: 11 }
                    Text { text: qsTr("Modified"); color: theme.mutedForeground; font.pixelSize: 11 }
                    Text { text: backend.modifiedAt.slice(0, 19).replace("T", " "); color: theme.foreground; font.pixelSize: 11 }
                    Text { text: qsTr("Accessed"); color: theme.mutedForeground; font.pixelSize: 11 }
                    Text { text: backend.accessedAt.slice(0, 19).replace("T", " "); color: theme.foreground; font.pixelSize: 11 }
                    Text { text: qsTr("Words / Chars"); color: theme.mutedForeground; font.pixelSize: 11 }
                    Text { text: backend.wordCount + " / " + backend.charCount; color: theme.foreground; font.pixelSize: 11 }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

                Button { Layout.fillWidth: true; text: qsTr("Duplicate Draft"); Material.roundedScale: theme.radius; onClicked: backend.duplicateCurrentDraft() }
                Button { Layout.fillWidth: true; text: qsTr("Export Markdown…"); Material.roundedScale: theme.radius; onClicked: backend.exportCurrentMarkdownDialog() }
                Button { Layout.fillWidth: true; text: qsTr("Export HTML…"); Material.roundedScale: theme.radius; onClicked: backend.exportCurrentHtmlDialog() }
                Button {
                    Layout.fillWidth: true
                    text: backend.folder === "trash" ? qsTr("Delete Permanently") : qsTr("Move to Trash")
                    Material.roundedScale: theme.radius
                    onClicked: backend.folder === "trash" ? backend.deleteDraftPermanently() : backend.trashDraft()
                }
            }
        }
    }
}
