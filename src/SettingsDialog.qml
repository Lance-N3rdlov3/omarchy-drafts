import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: settingsRoot
    color: Qt.rgba(0, 0, 0, 0.72)
    z: 65

    signal closeRequested()

    MouseArea { anchors.fill: parent; onClicked: settingsRoot.closeRequested() }

    Rectangle {
        width: Math.min(540, parent.width - 48)
        height: Math.min(540, parent.height - 64)
        anchors.centerIn: parent
        radius: theme.radius
        color: theme.surface
        border.color: theme.border
        border.width: 1

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Editor Settings & Backup"); color: theme.foreground; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                ToolButton { text: "✕"; onClicked: settingsRoot.closeRequested() }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

            GridLayout {
                columns: 2
                Layout.fillWidth: true
                rowSpacing: 10; columnSpacing: 12

                Text { text: qsTr("Font Family"); color: theme.mutedForeground; font.pixelSize: 12 }
                ComboBox {
                    Layout.fillWidth: true
                    Material.roundedScale: theme.radius
                    model: ["monospace", "sans-serif", "serif", "JetBrains Mono", "Inter"]
                    currentIndex: Math.max(0, model.indexOf(backend.fontFamily))
                    onActivated: backend.fontFamily = currentText
                }

                Text { text: qsTr("Font Size (%1 px)").arg(backend.fontSize); color: theme.mutedForeground; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 11; to: 26; stepSize: 1
                    value: backend.fontSize
                    onMoved: backend.fontSize = Math.round(value)
                }

                Text { text: qsTr("Line Height (%1)").arg(backend.lineHeight.toFixed(2)); color: theme.mutedForeground; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 1.2; to: 2.2; stepSize: 0.05
                    value: backend.lineHeight
                    onMoved: backend.lineHeight = value
                }

                Text { text: qsTr("Max Line Width (%1 px)").arg(backend.editorMaxWidth); color: theme.mutedForeground; font.pixelSize: 12 }
                Slider {
                    Layout.fillWidth: true
                    from: 560; to: 1280; stepSize: 40
                    value: backend.editorMaxWidth
                    onMoved: backend.editorMaxWidth = Math.round(value)
                }
            }

            CheckBox {
                text: qsTr("Typewriter Scrolling (keep cursor vertically centered)")
                checked: backend.typewriterMode
                onToggled: backend.typewriterMode = checked
            }

            CheckBox {
                text: qsTr("Open a new blank draft on launch")
                checked: backend.newDraftOnLaunch
                onToggled: backend.newDraftOnLaunch = checked
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

            Text { text: qsTr("Backup & Data"); color: theme.mutedForeground; font.pixelSize: 12; font.bold: true }

            GridLayout {
                columns: 2
                Layout.fillWidth: true
                rowSpacing: 8; columnSpacing: 8

                Button { Layout.fillWidth: true; text: qsTr("Export JSON Backup…"); Material.roundedScale: theme.radius; onClicked: backend.exportBackupDialog() }
                Button { Layout.fillWidth: true; text: qsTr("Import JSON Backup…"); Material.roundedScale: theme.radius; onClicked: backend.importBackupDialog() }
                Button { Layout.fillWidth: true; text: qsTr("Quick Snapshot Backup"); Material.roundedScale: theme.radius; onClicked: backend.createAutoBackupNow() }
                Button { Layout.fillWidth: true; text: qsTr("Export Current Markdown…"); Material.roundedScale: theme.radius; onClicked: backend.exportCurrentMarkdownDialog() }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Tip: bind `drafts capture` in Hyprland for instant global capture"); color: theme.mutedForeground; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Button { text: qsTr("Done"); highlighted: true; Material.roundedScale: theme.radius; onClicked: settingsRoot.closeRequested() }
            }
        }
    }
}
