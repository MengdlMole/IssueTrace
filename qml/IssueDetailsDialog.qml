import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "eventDetailsDialog"
    required property var controller
    property var draftIssue: ({})
    property bool dirty: false
    property bool saving: false

    function setDraftField(id, value) {
        const updated = Object.assign({}, draftIssue)
        updated[id] = value
        draftIssue = updated
        dirty = true
    }
    function reloadTrackedDuration() {
        draftIssue = Object.assign({}, controller.selectedIssue)
        const totalMinutes = Math.floor(Number(
            controller.selectedIssue.trackedMilliseconds || 0) / 60000)
        trackedHours.value = Math.floor(totalMinutes / 60)
        trackedMinutes.value = totalMinutes % 60
    }
    function reload() {
        reloadTrackedDuration()
        dirty = false
    }
    function save() {
        if (!dirty || controller.selectedIssue.id === undefined) return true
        saving = true
        const saved = controller.saveIssue(draftIssue)
        saving = false
        if (!saved) return false
        reload()
        return true
    }
    function discard() {
        dirty = false
        reload()
    }
    // Kept for callers that switch pages or close the application. Metadata is
    // never committed implicitly; only save() writes the draft to the store.
    function flush() {
        return true
    }

    title: "编辑事件"
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 48, 820)
    height: Math.min(parent.height - 48, 720)
    closePolicy: Popup.NoAutoClose
    onOpened: reload()

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.opened
        onActivated: {
            root.discard()
            root.close()
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "取消"
            onClicked: {
                root.discard()
                root.close()
            }
        }
        Button {
            objectName: "saveIssueDetailsButton"
            text: "保存"
            highlighted: true
            enabled: root.dirty && !root.saving
            onClicked: if (root.save()) root.close()
        }
    }

    ScrollView {
        id: detailsScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: detailsScroll.availableWidth
            spacing: 12

            EventFormFields {
                Layout.fillWidth: true
                controller: root.controller
                values: root.draftIssue
                enableDescriptionImages: true
                onFieldEdited: (fieldId, value) => root.setDraftField(fieldId, value)
                onDescriptionImagePasteRequested:
                    root.controller.addDescriptionClipboardImage()
                onDescriptionImageDropped: function(source) {
                    root.controller.addDescriptionImage(source)
                }
            }

            Flow {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(0, childrenRect.height)
                spacing: 8
                Repeater {
                    model: root.controller.descriptionAttachments
                    delegate: Frame {
                        required property var modelData
                        width: 160
                        height: 138
                        ColumnLayout {
                            anchors.fill: parent
                            Image {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: modelData.url
                                fillMode: Image.PreserveAspectFit
                                autoTransform: true
                                MouseArea {
                                    anchors.fill: parent
                                    onDoubleClicked: root.controller.openAttachment(modelData.url)
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    elide: Text.ElideMiddle
                                }
                                ToolButton {
                                    text: "×"
                                    onClicked: root.controller.deleteAttachment(modelData.id)
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Button {
                    objectName: "descriptionPasteImageButton"
                    text: "粘贴图片"
                    onClicked: root.controller.addDescriptionClipboardImage()
                }
                Button {
                    objectName: "descriptionChooseImageButton"
                    text: "选择图片…"
                    onClicked: descriptionImageDialog.open()
                }
                Label { text: "也可以拖放图片；双击缩略图打开"; color: palette.mid }
                Item { Layout.fillWidth: true }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.midlight }

            GridLayout {
                Layout.fillWidth: true
                columns: width >= 620 ? 2 : 1
                columnSpacing: 14
                rowSpacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "事件进展"; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        text: root.draftIssue.progress || "尚未记录进展"
                        color: root.draftIssue.progress ? palette.text : palette.mid
                        wrapMode: Text.Wrap
                        padding: 7
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "事件结论"; font.bold: true }
                    TextArea {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 92
                        text: root.draftIssue.conclusion || ""
                        wrapMode: TextEdit.Wrap
                        placeholderText: "根因、解决结果或最终结论"
                        onTextChanged: if (activeFocus)
                            root.setDraftField("conclusion", text)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "提出时间"; font.bold: true }
                    Label { text: root.draftIssue.reported_at || "—"; color: palette.mid; padding: 7 }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "解决时间"; font.bold: true }
                    Label { text: root.draftIssue.resolved_at || "—"; color: palette.mid; padding: 7 }
                }
            }

            Label { text: "累计处理时间"; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                SpinBox {
                    id: trackedHours
                    objectName: "trackedHoursEditor"
                    from: 0
                    to: 99999
                    editable: true
                    enabled: !root.controller.selectedIssue.timerRunning
                }
                Label { text: "小时" }
                SpinBox {
                    id: trackedMinutes
                    objectName: "trackedMinutesEditor"
                    from: 0
                    to: 59
                    editable: true
                    enabled: !root.controller.selectedIssue.timerRunning
                }
                Label { text: "分钟" }
                Button {
                    objectName: "saveTrackedDurationButton"
                    text: "保存计时"
                    enabled: !root.controller.selectedIssue.timerRunning
                    onClicked: {
                        if (root.controller.setSelectedIssueTrackedDuration(
                                trackedHours.value, trackedMinutes.value)) {
                            const totalMinutes = Math.floor(Number(
                                root.controller.selectedIssue.trackedMilliseconds || 0) / 60000)
                            trackedHours.value = Math.floor(totalMinutes / 60)
                            trackedMinutes.value = totalMinutes % 60
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Label {
                Layout.fillWidth: true
                text: root.controller.selectedIssue.timerRunning
                    ? "计时进行中，请先暂停计时再修改。"
                    : "后续计时会从此累计值继续增加。"
                color: root.controller.selectedIssue.timerRunning ? palette.accent : palette.mid
                wrapMode: Text.Wrap
            }
        }
    }

    FileDialog {
        id: descriptionImageDialog
        title: "选择事件描述图片"
        fileMode: FileDialog.OpenFile
        nameFilters: ["图片 (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"]
        onAccepted: root.controller.addDescriptionImage(selectedFile)
    }

    Connections {
        target: root.controller
        function onSelectedIssueChanged() {
            if (!root.opened || (!root.dirty && !root.saving)) root.reload()
        }
    }
}
