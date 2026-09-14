import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Frame {
    id: root
    required property var controller
    required property var typeOptions

    function optionIndex(value) {
        for (let i = 0; i < typeOptions.length; ++i)
            if (typeOptions[i].value === value) return i
        return 0
    }
    function saveDraft() {
        const issueId = controller.selectedIssue.id
        if (issueId !== undefined)
            controller.saveTimelineDraft(issueId, timelineType.currentValue, timelineInput.text)
    }
    function reloadDraft() {
        const issueId = controller.selectedIssue.id || ""
        timelineInput.text = controller.loadTimelineDraft(issueId)
        timelineType.currentIndex = optionIndex(controller.loadTimelineDraftType(issueId))
    }
    function submit(asProgress) {
        if (!timelineInput.text.trim()) return
        const type = asProgress ? "progress" : timelineType.currentValue
        if (controller.addTimelineEntry(type, timelineInput.text)) {
            timelineInput.clear()
            controller.saveTimelineDraft(controller.selectedIssue.id, type, "")
            timelineInput.forceActiveFocus()
        }
    }
    function forceInputFocus() { timelineInput.forceActiveFocus() }

    Layout.fillWidth: true

    ColumnLayout {
        anchors.fill: parent
        TextArea {
            id: timelineInput
            Layout.fillWidth: true
            implicitHeight: 96
            wrapMode: TextEdit.Wrap
            placeholderText: "随手记下发现、进展或下一步；可粘贴截图、拖入日志和附件"
            onTextChanged: draftSaveTimer.restart()
            Keys.onPressed: function(event) {
                if ((event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) &&
                        (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                    root.submit((event.modifiers & Qt.ShiftModifier) !== 0)
                    event.accepted = true
                } else if (event.matches(StandardKey.Paste) && controller.clipboardHasImage()) {
                    if (controller.addTimelineEntryWithClipboardImage(timelineType.currentValue, text)) {
                        clear()
                        forceActiveFocus()
                    }
                    event.accepted = true
                }
            }
            DropArea {
                anchors.fill: parent
                onDropped: function(drop) {
                    if (drop.hasUrls && controller.addTimelineEntryWithFiles(
                            timelineType.currentValue, timelineInput.text, drop.urls)) {
                        timelineInput.clear()
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: timelineType
                Layout.preferredWidth: 110
                model: root.typeOptions
                textRole: "label"
                valueRole: "value"
                onActivated: draftSaveTimer.restart()
            }
            Button {
                text: "粘贴截图"
                onClicked: if (controller.addTimelineEntryWithClipboardImage(
                    timelineType.currentValue, timelineInput.text)) timelineInput.clear()
            }
            Button { text: "附件/日志…"; onClicked: attachmentDialog.open() }
            Label { text: "草稿自动保存"; color: palette.mid; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Button {
                text: "记录进展"
                enabled: timelineInput.text.trim().length > 0
                onClicked: root.submit(true)
            }
            Button {
                text: "记录"
                highlighted: true
                enabled: timelineInput.text.trim().length > 0
                onClicked: root.submit(false)
            }
        }
    }

    Timer { id: draftSaveTimer; interval: 500; onTriggered: root.saveDraft() }
    Component.onCompleted: reloadDraft()
    Connections {
        target: root.controller
        function onSelectedIssueChanged() { root.reloadDraft() }
    }
    FileDialog {
        id: attachmentDialog
        title: "选择日志或附件（每个最大 100 MB）"
        fileMode: FileDialog.OpenFiles
        onAccepted: if (controller.addTimelineEntryWithFiles(
            timelineType.currentValue, timelineInput.text, selectedFiles)) timelineInput.clear()
    }
}
