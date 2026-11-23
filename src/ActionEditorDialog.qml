import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: actDialog
    color: Qt.rgba(0, 0, 0, 0.72)
    z: 65

    property var actionData: ({})
    signal closeRequested()

    property string actId: ""
    property var stepsList: []
    property int activeStepIdx: 0

    readonly property var stepTypes: ["script", "clipboard", "file", "insert_text", "shell", "url", "mail", "html_preview"]

    function loadAction(obj) {
        actId = obj.id || ""
        actName.text = obj.name || ""
        actDesc.text = obj.description || ""
        actIcon.text = obj.icon || "⚡"
        actTint.text = obj.tint || theme.accent

        actGroupCombo.currentIndex = 0
        for (var i = 0; i < backend.actionGroups.length; i++) {
            if (backend.actionGroups[i].id === (obj.groupId || backend.activeActionGroupId)) {
                actGroupCombo.currentIndex = i; break
            }
        }

        var afters = ["nothing", "archive", "trash"]
        afterCombo.currentIndex = Math.max(0, afters.indexOf(obj.afterSuccess || "nothing"))

        var rawSteps = obj.steps || []
        var copy = []
        for (var s = 0; s < rawSteps.length; s++) {
            copy.push({ type: rawSteps[s].type, config: Object.assign({}, rawSteps[s].config || {}) })
        }
        if (copy.length === 0) {
            copy.push({ type: "script", config: { code: "" } })
        }
        stepsList = copy
        activeStepIdx = 0
        syncStepFields()
    }

    function syncStepFields() {
        if (activeStepIdx < 0 || activeStepIdx >= stepsList.length) return
        var st = stepsList[activeStepIdx]
        stepTypeCombo.currentIndex = Math.max(0, stepTypes.indexOf(st.type))
        var cfg = st.config || {}
        stepPathField.text = cfg.path || cfg.to || ""
        stepExtraField.text = cfg.subject || cfg.mode || cfg.outputMode || ""
        stepBodyArea.text = cfg.code || cfg.script || cfg.template || cfg.body || ""
    }

    function captureCurrentStep() {
        if (activeStepIdx < 0 || activeStepIdx >= stepsList.length) return
        var type = stepTypes[stepTypeCombo.currentIndex]
        var cfg = {}
        if (type === "script") {
            cfg.code = stepBodyArea.text
        } else if (type === "shell") {
            cfg.script = stepBodyArea.text
            cfg.outputMode = stepExtraField.text.length > 0 ? stepExtraField.text : "append"
        } else if (type === "file") {
            cfg.path = stepPathField.text.length > 0 ? stepPathField.text : "~/Documents/Drafts/[[safe_title]].md"
            cfg.mode = stepExtraField.text.length > 0 ? stepExtraField.text : "write"
            cfg.template = stepBodyArea.text
        } else if (type === "clipboard") {
            cfg.mode = stepExtraField.text.length > 0 ? stepExtraField.text : "replace"
            cfg.template = stepBodyArea.text
        } else if (type === "mail") {
            cfg.to = stepPathField.text
            cfg.subject = stepExtraField.text.length > 0 ? stepExtraField.text : "[[title]]"
            cfg.body = stepBodyArea.text
        } else {
            cfg.template = stepBodyArea.text
        }
        stepsList[activeStepIdx] = { type: type, config: cfg }
    }

    onVisibleChanged: {
        if (visible) loadAction(actionData || ({}))
    }

    MouseArea { anchors.fill: parent; onClicked: actDialog.closeRequested() }

    Rectangle {
        width: Math.min(720, parent.width - 48)
        height: Math.min(640, parent.height - 56)
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
                Text { text: actDialog.actId.length > 0 ? qsTr("Edit Action") : qsTr("New Action"); color: theme.foreground; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                Button {
                    visible: actDialog.actId.length > 0
                    text: qsTr("Copy JSON")
                    Material.roundedScale: theme.radius
                    onClicked: {
                        backend.copyToClipboard(backend.exportActionJson(actDialog.actId))
                        backend.showToast(qsTr("Copied Action JSON to clipboard"), "success")
                    }
                }
                Button {
                    text: qsTr("Import JSON")
                    Material.roundedScale: theme.radius
                    onClicked: importPopup.open()
                }
                ToolButton { text: "✕"; onClicked: actDialog.closeRequested() }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField { id: actName; Layout.fillWidth: true; placeholderText: qsTr("Action Name"); Material.roundedScale: theme.radius }
                TextField { id: actIcon; Layout.preferredWidth: 64; placeholderText: qsTr("Icon"); Material.roundedScale: theme.radius }
                TextField { id: actTint; Layout.preferredWidth: 96; placeholderText: "#e68e0d"; Material.roundedScale: theme.radius }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField { id: actDesc; Layout.fillWidth: true; placeholderText: qsTr("Description"); Material.roundedScale: theme.radius }
                ComboBox {
                    id: actGroupCombo
                    Layout.preferredWidth: 140
                    model: backend.actionGroups
                    textRole: "name"
                    Material.roundedScale: theme.radius
                }
                ComboBox {
                    id: afterCombo
                    Layout.preferredWidth: 135
                    model: [qsTr("After: Nothing"), qsTr("After: Archive"), qsTr("After: Trash")]
                    Material.roundedScale: theme.radius
                }
            }

            // Multi-step bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: qsTr("Steps:"); color: theme.mutedForeground; font.pixelSize: 12; font.bold: true }
                Repeater {
                    model: actDialog.stepsList.length
                    delegate: Rectangle {
                        height: 26
                        width: stLbl.implicitWidth + 16
                        radius: Math.min(4, theme.radius)
                        color: index === actDialog.activeStepIdx ? theme.accent : theme.surfaceRaised
                        Text {
                            id: stLbl
                            anchors.centerIn: parent
                            text: (index + 1) + ". " + actDialog.stepsList[index].type
                            font.pixelSize: 11
                            color: index === actDialog.activeStepIdx ? theme.accentForeground : theme.foreground
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                actDialog.captureCurrentStep()
                                actDialog.activeStepIdx = index
                                actDialog.syncStepFields()
                            }
                        }
                    }
                }
                Button {
                    text: "+ Step"
                    Material.roundedScale: theme.radius
                    onClicked: {
                        actDialog.captureCurrentStep()
                        var next = actDialog.stepsList.slice()
                        next.push({ type: "clipboard", config: { template: "[[draft]]", mode: "replace" } })
                        actDialog.stepsList = next
                        actDialog.activeStepIdx = next.length - 1
                        actDialog.syncStepFields()
                    }
                }
                Button {
                    visible: actDialog.stepsList.length > 1
                    text: qsTr("Remove Step")
                    Material.roundedScale: theme.radius
                    onClicked: {
                        var next = actDialog.stepsList.slice()
                        next.splice(actDialog.activeStepIdx, 1)
                        actDialog.stepsList = next
                        actDialog.activeStepIdx = Math.max(0, actDialog.activeStepIdx - 1)
                        actDialog.syncStepFields()
                    }
                }
                Item { Layout.fillWidth: true }
            }

            // Active step configuration
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ComboBox {
                    id: stepTypeCombo
                    Layout.preferredWidth: 150
                    model: actDialog.stepTypes
                    Material.roundedScale: theme.radius
                }
                TextField {
                    id: stepPathField
                    Layout.fillWidth: true
                    visible: stepTypeCombo.currentText === "file" || stepTypeCombo.currentText === "mail"
                    placeholderText: stepTypeCombo.currentText === "file" ? qsTr("File path (e.g. ~/Documents/[[safe_title]].md)") : qsTr("Recipient email")
                    Material.roundedScale: theme.radius
                }
                TextField {
                    id: stepExtraField
                    Layout.preferredWidth: 180
                    visible: stepTypeCombo.currentText === "file" || stepTypeCombo.currentText === "clipboard" || stepTypeCombo.currentText === "shell" || stepTypeCombo.currentText === "mail"
                    placeholderText: stepTypeCombo.currentText === "shell" ? qsTr("Output: none|append|replace|clipboard")
                                   : (stepTypeCombo.currentText === "mail" ? qsTr("Subject template") : qsTr("Mode: write|append|prepend"))
                    Material.roundedScale: theme.radius
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Math.min(4, theme.radius)
                color: theme.background
                border.color: theme.border
                border.width: 1

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    contentWidth: width
                    contentHeight: Math.max(height, stepBodyArea.implicitHeight)
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    TextArea {
                        id: stepBodyArea
                        width: parent.width
                        background: Item {}
                        color: theme.foreground
                        font.family: "monospace"
                        font.pixelSize: 13
                        wrapMode: TextEdit.Wrap
                        placeholderText: stepTypeCombo.currentText === "script"
                            ? qsTr("JavaScript using draft, editor, app, Draft, ShellScript, FileManager, HTTP, context…")
                            : qsTr("Template or command using [[draft]], [[title]], [[body]], [[selection]], [[date|%Y-%m-%d]], [[markdown]], [[clipboard]]…")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    visible: actDialog.actId.length > 0
                    text: qsTr("Delete Action")
                    Material.roundedScale: theme.radius
                    onClicked: { backend.deleteAction(actDialog.actId); actDialog.closeRequested() }
                }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); Material.roundedScale: theme.radius; onClicked: actDialog.closeRequested() }
                Button {
                    text: qsTr("Save Action")
                    highlighted: true
                    Material.roundedScale: theme.radius
                    onClicked: {
                        actDialog.captureCurrentStep()
                        var afters = ["nothing", "archive", "trash"]
                        var grpId = (actGroupCombo.currentIndex >= 0 && actGroupCombo.currentIndex < backend.actionGroups.length)
                                    ? backend.actionGroups[actGroupCombo.currentIndex].id : backend.activeActionGroupId
                        backend.saveAction({
                            id: actDialog.actId,
                            groupId: grpId,
                            name: actName.text,
                            description: actDesc.text,
                            icon: actIcon.text.length > 0 ? actIcon.text : "⚡",
                            tint: actTint.text,
                            afterSuccess: afters[afterCombo.currentIndex],
                            steps: actDialog.stepsList
                        })
                        actDialog.closeRequested()
                    }
                }
            }
        }

        // Import Action JSON inline dialog
        Popup {
            id: importPopup
            anchors.centerIn: parent
            width: 480; height: 280
            modal: true
            Material.roundedScale: theme.radius
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                Text { text: qsTr("Paste Action JSON"); color: theme.foreground; font.pixelSize: 14; font.bold: true }
                TextArea {
                    id: importJsonArea
                    Layout.fillWidth: true; Layout.fillHeight: true
                    font.family: "monospace"; font.pixelSize: 11
                    placeholderText: "{\"name\": \"My Action\", \"steps\": [...]}"
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button { text: qsTr("Cancel"); onClicked: importPopup.close() }
                    Button {
                        text: qsTr("Import")
                        highlighted: true
                        onClicked: {
                            if (backend.importActionJson(importJsonArea.text, backend.activeActionGroupId)) {
                                importPopup.close()
                                actDialog.closeRequested()
                            }
                        }
                    }
                }
            }
        }
    }
}
