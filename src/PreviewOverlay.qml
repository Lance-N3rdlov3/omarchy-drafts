import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: previewRoot
    color: Qt.rgba(0, 0, 0, 0.72)
    z: 55

    signal closeRequested()

    MouseArea { anchors.fill: parent; onClicked: previewRoot.closeRequested() }

    Rectangle {
        width: Math.min(820, parent.width - 56)
        height: Math.min(680, parent.height - 64)
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
                Text { text: qsTr("HTML Preview"); color: theme.foreground; font.pixelSize: 16; font.bold: true }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Copy HTML")
                    Material.roundedScale: theme.radius
                    onClicked: backend.copyPreviewHtml()
                }
                Button {
                    text: qsTr("Export HTML")
                    Material.roundedScale: theme.radius
                    onClicked: backend.exportCurrentHtmlDialog()
                }
                Button {
                    text: qsTr("Open in Browser")
                    Material.roundedScale: theme.radius
                    onClicked: backend.openPreviewInBrowser()
                }
                ToolButton {
                    text: "✕"
                    onClicked: previewRoot.closeRequested()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: htmlView.implicitHeight + 32
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Text {
                    id: htmlView
                    width: parent.width - 16
                    x: 8; y: 8
                    textFormat: Text.RichText
                    text: backend.previewHtml
                    color: theme.foreground
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    onLinkActivated: function(href) {
                        previewRoot.closeRequested()
                        backend.openLink(href)
                    }
                }
            }
        }
    }
}
