import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Drafts Arrange Mode: split active draft into lines or paragraphs, reorder,
// sort, reverse, duplicate, or remove blocks, then commit back into the editor.
Rectangle {
    id: arrangeRoot
    color: Qt.rgba(0, 0, 0, 0.72)
    z: 55

    signal closeRequested()

    property string splitMode: "lines" // lines | paragraphs

    ListModel { id: blockModel }

    function reloadBlocks() {
        blockModel.clear()
        var arr = backend.splitForArrange(splitMode)
        for (var i = 0; i < arr.length; i++) {
            blockModel.append({ text: arr[i] })
        }
    }

    function commitBlocks() {
        var out = []
        for (var i = 0; i < blockModel.count; i++)
            out.push(blockModel.get(i).text)
        backend.commitArrange(out, splitMode)
        arrangeRoot.closeRequested()
    }

    function sortBlocks() {
        var arr = []
        for (var i = 0; i < blockModel.count; i++) arr.push(blockModel.get(i).text)
        arr.sort(function(a, b) { return a.localeCompare(b) })
        blockModel.clear()
        for (var j = 0; j < arr.length; j++) blockModel.append({ text: arr[j] })
    }

    function reverseBlocks() {
        var arr = []
        for (var i = blockModel.count - 1; i >= 0; i--) arr.push(blockModel.get(i).text)
        blockModel.clear()
        for (var j = 0; j < arr.length; j++) blockModel.append({ text: arr[j] })
    }

    onVisibleChanged: {
        if (visible) reloadBlocks()
    }

    MouseArea { anchors.fill: parent; onClicked: arrangeRoot.closeRequested() }

    Rectangle {
        width: Math.min(720, parent.width - 48)
        height: Math.min(620, parent.height - 64)
        anchors.centerIn: parent
        radius: theme.radius
        color: theme.surface
        border.color: theme.border
        border.width: 1

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text { text: qsTr("Arrange Mode"); color: theme.foreground; font.pixelSize: 16; font.bold: true }
                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Lines")
                    highlighted: arrangeRoot.splitMode === "lines"
                    Material.roundedScale: theme.radius
                    onClicked: { arrangeRoot.splitMode = "lines"; arrangeRoot.reloadBlocks() }
                }
                Button {
                    text: qsTr("Paragraphs")
                    highlighted: arrangeRoot.splitMode === "paragraphs"
                    Material.roundedScale: theme.radius
                    onClicked: { arrangeRoot.splitMode = "paragraphs"; arrangeRoot.reloadBlocks() }
                }
                Button {
                    text: qsTr("Sort A–Z")
                    Material.roundedScale: theme.radius
                    onClicked: arrangeRoot.sortBlocks()
                }
                Button {
                    text: qsTr("Reverse")
                    Material.roundedScale: theme.radius
                    onClicked: arrangeRoot.reverseBlocks()
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: blockModel
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    width: ListView.view.width
                    height: Math.max(44, blockTxt.implicitHeight + 18)
                    radius: Math.min(4, theme.radius)
                    color: theme.surfaceRaised
                    border.color: theme.border
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 8
                        spacing: 8

                        Text { text: (index + 1) + "."; color: theme.mutedForeground; font.pixelSize: 11; Layout.preferredWidth: 24 }
                        Text {
                            id: blockTxt
                            Layout.fillWidth: true
                            text: model.text.length > 0 ? model.text : qsTr("(blank line)")
                            color: model.text.length > 0 ? theme.foreground : theme.mutedForeground
                            font.family: backend.fontFamily
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                        }

                        ToolButton {
                            text: "↑"
                            enabled: index > 0
                            onClicked: blockModel.move(index, index - 1, 1)
                        }
                        ToolButton {
                            text: "↓"
                            enabled: index < blockModel.count - 1
                            onClicked: blockModel.move(index, index + 1, 1)
                        }
                        ToolButton {
                            text: "⧉"
                            ToolTip.visible: hovered; ToolTip.text: qsTr("Duplicate")
                            onClicked: blockModel.insert(index + 1, { text: model.text })
                        }
                        ToolButton {
                            text: "✕"
                            onClicked: blockModel.remove(index)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    Material.roundedScale: theme.radius
                    onClicked: arrangeRoot.closeRequested()
                }
                Button {
                    text: qsTr("Apply Order")
                    highlighted: true
                    Material.roundedScale: theme.radius
                    onClicked: arrangeRoot.commitBlocks()
                }
            }
        }
    }
}
