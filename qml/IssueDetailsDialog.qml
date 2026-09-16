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

    function optionIndex(options, value) {
        for (let i = 0; i < options.length; ++i)
            if (options[i].value === value) return i
        return 0
    }
    function setDraftField(id, value) {
        const updated = Object.assign({}, draftIssue)
        updated[id] = value
        draftIssue = updated
        dirty = true
        saveTimer.restart()
    }
    function reload() {
        draftIssue = Object.assign({}, controller.selectedIssue)
        const totalMinutes = Math.floor(Number(controller.selectedIssue.trackedMilliseconds || 0) / 60000)
        trackedHours.value = Math.floor(totalMinutes / 60)
        trackedMinutes.value = totalMinutes % 60
        dirty = false
    }
    function flush() {
        if (!dirty || controller.selectedIssue.id === undefined) return true
        if (!controller.saveIssue(draftIssue)) return false
        dirty = false
        return true
    }

    title: "更多信息"
    modal: true
    anchors.centerIn: parent
    width: 650
    height: Math.min(parent.height - 70, 650)
    closePolicy: Popup.NoAutoClose
    onOpened: reload()

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.opened
        onActivated: if (root.flush()) root.close()
    }

    footer: DialogButtonBox {
        Button {
            text: "关闭"
            onClicked: if (root.flush()) root.close()
        }
    }

    ScrollView {
        anchors.fill: parent
        ColumnLayout {
            width: root.availableWidth
            Label { text: "事件描述"; font.bold: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 130
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                TextArea {
                    id: descriptionText
                    text: root.draftIssue.original_problem || ""
                    wrapMode: TextEdit.Wrap
                    placeholderText: "记录原始描述；可直接粘贴截图或把图片拖到这里"
                    onTextChanged: if (activeFocus) root.setDraftField("original_problem", text)
                    Keys.onPressed: function(event) {
                        if (event.matches(StandardKey.Paste) && root.controller.clipboardHasImage()) {
                            if (root.flush()) root.controller.addDescriptionClipboardImage()
                            event.accepted = true
                        }
                    }
                    DropArea {
                        anchors.fill: parent
                        onDropped: function(drop) {
                            if (drop.hasUrls && drop.urls.length > 0 && root.flush())
                                root.controller.addDescriptionImage(drop.urls[0])
                        }
                    }
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
                        width: 160; height: 138
                        ColumnLayout {
                            anchors.fill: parent
                            Image {
                                Layout.fillWidth: true; Layout.fillHeight: true
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
                                Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideMiddle }
                                ToolButton { text: "×"; onClicked: root.controller.deleteAttachment(modelData.id) }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Button {
                    objectName: "descriptionPasteImageButton"
                    text: "粘贴图片"
                    onClicked: if (root.flush()) root.controller.addDescriptionClipboardImage()
                }
                Button {
                    objectName: "descriptionChooseImageButton"
                    text: "选择图片…"
                    onClicked: if (root.flush()) descriptionImageDialog.open()
                }
                Label { text: "也可以拖放图片；双击缩略图打开"; color: palette.mid }
                Item { Layout.fillWidth: true }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.midlight }
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
                        if (root.flush() && root.controller.setSelectedIssueTrackedDuration(
                                trackedHours.value, trackedMinutes.value))
                            root.reload()
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Label {
                Layout.fillWidth: true
                text: root.controller.selectedIssue.timerRunning
                    ? "计时进行中，请先暂停计时再修改。"
                    : "可修正误计时；修改不会改变事件的最后修改时间。"
                color: root.controller.selectedIssue.timerRunning ? palette.accent : palette.mid
                wrapMode: Text.Wrap
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.midlight }
            Repeater {
                model: root.controller.formFields
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    visible: modelData.id !== "status" && modelData.id !== "original_problem"
                    Label { text: modelData.label }
                    Loader {
                        Layout.fillWidth: true
                        property var definition: modelData
                        sourceComponent: definition.readOnly ? readOnlyEditor
                            : definition.type === "select" ? selectEditor
                            : definition.type === "history_select" ? historyEditor
                            : definition.type === "multiline_text" ? multilineEditor
                            : textEditor
                        onLoaded: item.field = definition
                    }
                }
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

    Timer {
        id: saveTimer
        interval: 800
        onTriggered: if (root.dirty && root.controller.saveIssue(root.draftIssue)) root.dirty = false
    }
    Connections {
        target: root.controller
        function onSelectedIssueChanged() { root.reload() }
    }

    Component {
        id: textEditor
        TextField {
            property var field
            text: field ? (root.draftIssue[field.id] || "") : ""
            placeholderText: !field ? "" : field.id === "group_name"
                ? "多级分组使用 / 分隔，例如：业务/支付/网关"
                : field.id === "tags" ? "多个标签使用英文逗号分隔" : ""
            onTextEdited: if (field) root.setDraftField(field.id, text)
        }
    }
    Component {
        id: historyEditor
        ComboBox {
            property var field
            editable: true
            model: !field ? [] : field.id === "service"
                ? root.controller.serviceOptions : root.controller.versionOptions
            editText: field ? (root.draftIssue[field.id] || "") : ""
            onEditTextChanged: if (activeFocus && field)
                root.setDraftField(field.id, editText.trim())
            onAccepted: if (field) root.setDraftField(field.id, editText.trim())
            onActivated: if (field) root.setDraftField(field.id, currentText)
        }
    }
    Component {
        id: multilineEditor
        ScrollView {
            id: multilineContainer
            property var field
            implicitHeight: 110
            TextArea {
                text: multilineContainer.field ? (root.draftIssue[multilineContainer.field.id] || "") : ""
                wrapMode: TextEdit.Wrap
                placeholderText: multilineContainer.field && multilineContainer.field.id === "original_problem"
                    ? "支持 Markdown；图片可直接作为事件记录粘贴，并会随导出保留" : ""
                onTextChanged: if (activeFocus && multilineContainer.field)
                    root.setDraftField(multilineContainer.field.id, text)
            }
        }
    }
    Component {
        id: selectEditor
        ComboBox {
            property var field
            model: field ? (field.options || []) : []
            textRole: "label"
            valueRole: "value"
            currentIndex: field ? root.optionIndex(model, root.draftIssue[field.id]) : 0
            onActivated: if (field) root.setDraftField(field.id, currentValue)
        }
    }
    Component {
        id: readOnlyEditor
        Label {
            property var field
            text: field ? (root.draftIssue[field.id] || "—") : "—"
            color: palette.mid
            padding: 6
        }
    }
}
