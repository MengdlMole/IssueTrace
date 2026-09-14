import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "IssueTrace · 问题收件箱"

    property var draftIssue: ({})
    property bool detailDirty: false
    property string pendingAttachmentEntryId: ""
    property url pendingRestoreFolder: ""
    property url pendingUpdateArchive: ""
    property var statusOptions: [
        { value: "pending", label: "待处理" },
        { value: "investigating", label: "处理中" },
        { value: "waiting", label: "等待" },
        { value: "completed", label: "已完成" }
    ]
    property var filterStatusOptions: [{ value: "", label: "全部状态" }].concat(statusOptions)
    property var timelineTypeOptions: [
        { value: "note", label: "记录" },
        { value: "progress", label: "进展" },
        { value: "conclusion", label: "结论" }
    ]

    function optionIndex(options, value) {
        for (let i = 0; i < options.length; ++i)
            if (options[i].value === value) return i
        return 0
    }
    function optionLabel(options, value) {
        const item = options[optionIndex(options, value)]
        return item ? item.label : value
    }
    function liveSelectedIssue() {
        const selectedId = appController.selectedIssue.id
        for (let i = 0; i < appController.issues.length; ++i)
            if (appController.issues[i].id === selectedId) return appController.issues[i]
        return appController.selectedIssue
    }
    function flushIssue() {
        if (!detailDirty || appController.selectedIssue.id === undefined) return true
        if (!appController.saveIssue(draftIssue)) return false
        detailDirty = false
        return true
    }
    function selectIssue(id) {
        if (!flushIssue()) return
        if (appController.selectedIssue.id !== undefined)
            appController.saveTimelineDraft(appController.selectedIssue.id,
                timelineType.currentValue, timelineInput.text)
        appController.selectIssue(id)
    }
    function applyFilter() {
        appController.filterIssues(searchInput.text, statusFilter.currentValue,
            "", "", "updated_desc", 0)
    }
    function submitTimeline(asProgress) {
        if (!timelineInput.text.trim()) return
        const type = asProgress ? "progress" : timelineType.currentValue
        if (appController.addTimelineEntry(type, timelineInput.text)) {
            timelineInput.clear()
            appController.saveTimelineDraft(appController.selectedIssue.id, type, "")
            timelineInput.forceActiveFocus()
        }
    }
    function markStatus(status) {
        if (appController.setSelectedIssueStatus(status)) detailDirty = false
    }
    function createIssue(startNow) {
        if (!appController.createQuickIssue(quickTitle.text, quickReporter.text)) return
        if (startNow) appController.setSelectedIssueStatus("investigating")
        quickCreate.close()
        timelineInput.forceActiveFocus()
    }
    function setDraftField(id, value) {
        const updated = Object.assign({}, draftIssue)
        updated[id] = value
        draftIssue = updated
        detailDirty = true
        detailSaveTimer.restart()
    }

    onClosing: function(close) {
        if (!flushIssue()) {
            close.accepted = false
            return
        }
        if (appController.selectedIssue.id !== undefined)
            appController.saveTimelineDraft(appController.selectedIssue.id,
                timelineType.currentValue, timelineInput.text)
    }
    Shortcut { sequences: [StandardKey.New]; onActivated: quickCreate.open() }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            Label { text: "IssueTrace " + appController.appVersion; font.pixelSize: 20; font.bold: true }
            Label {
                Layout.fillWidth: true
                text: "记下来 · 持续跟进 · 到时提醒"
                color: palette.mid
            }
            Label {
                visible: root.detailDirty
                text: "保存中…"
                color: "#b66a00"
            }
            Button { text: "工具"; onClicked: toolsMenu.popup() }
            Button { text: "+ 记录问题"; highlighted: true; onClicked: quickCreate.open() }
        }
    }

    SplitView {
        anchors.fill: parent

        Pane {
            SplitView.preferredWidth: 390
            SplitView.minimumWidth: 310
            padding: 0
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: 14
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "问题收件箱"; font.pixelSize: 23; font.bold: true; Layout.fillWidth: true }
                        Label { text: appController.issues.length + " 项"; color: palette.mid }
                    }
                    TextField {
                        id: searchInput
                        Layout.fillWidth: true
                        placeholderText: "搜索问题和处理记录"
                        onTextEdited: searchDelay.restart()
                        onAccepted: root.applyFilter()
                    }
                    ComboBox {
                        id: statusFilter
                        Layout.fillWidth: true
                        model: root.filterStatusOptions
                        textRole: "label"
                        valueRole: "value"
                        onActivated: root.applyFilter()
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.midlight }
                Label {
                    visible: appController.attentionIssues.length > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.topMargin: 10
                    text: "需要关注 · " + appController.attentionIssues.length
                    font.bold: true
                    color: "#b42318"
                }
                ListView {
                    id: attentionList
                    visible: count > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 190)
                    clip: true
                    model: appController.attentionIssues
                    delegate: ItemDelegate {
                        required property var modelData
                        width: attentionList.width
                        height: 66
                        onClicked: root.selectIssue(modelData.id)
                        contentItem: ColumnLayout {
                            Label { Layout.fillWidth: true; text: "!  " + modelData.title; font.bold: true; elide: Text.ElideRight; color: "#b42318" }
                            Label { Layout.fillWidth: true; text: modelData.reminderDue ? "提醒已到期" : "已 " + modelData.inactiveText + " 未更新"; color: palette.mid; font.pixelSize: 12 }
                        }
                    }
                }
                Label { Layout.leftMargin: 14; Layout.topMargin: 8; text: "全部问题"; font.bold: true }
                ListView {
                    id: issueList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 1
                    model: appController.issues
                    delegate: ItemDelegate {
                        required property var modelData
                        width: issueList.width
                        height: 82
                        highlighted: appController.selectedIssue.id === modelData.id
                        onClicked: root.selectIssue(modelData.id)
                        contentItem: ColumnLayout {
                            RowLayout {
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: modelData.title; font.bold: true; elide: Text.ElideRight }
                                Label { text: root.optionLabel(root.statusOptions, modelData.status); color: palette.highlight }
                            }
                            Label { Layout.fillWidth: true; text: modelData.progress || "尚无进展"; color: palette.mid; elide: Text.ElideRight }
                            Label { text: "当前状态 " + modelData.statusDurationText + " · " + modelData.inactiveText + " 未更新"; color: palette.mid; font.pixelSize: 11 }
                        }
                    }
                    Label { anchors.centerIn: parent; visible: issueList.count === 0; text: "还没有问题\n按 Ctrl/Cmd + N 快速记录"; horizontalAlignment: Text.AlignHCenter; color: palette.mid }
                }
            }
        }

        Pane {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 550
            padding: 14
            ColumnLayout {
                anchors.fill: parent
                visible: appController.selectedIssue.id !== undefined
                spacing: 9
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: appController.selectedIssue.title || ""; font.pixelSize: 22; font.bold: true; elide: Text.ElideRight }
                        Label { text: "问题已记录 " + (root.liveSelectedIssue().ageText || "") + " · 当前状态停留 " + (root.liveSelectedIssue().statusDurationText || ""); color: palette.mid }
                    }
                    Button { text: "更多信息"; onClicked: detailsDialog.open() }
                    Button { text: "导出总结"; onClicked: exportDialog.open() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "待处理"; highlighted: appController.selectedIssue.status === "pending"; onClicked: root.markStatus("pending") }
                    Button { text: "开始处理"; highlighted: appController.selectedIssue.status === "investigating"; onClicked: root.markStatus("investigating") }
                    Button { text: "等待"; highlighted: appController.selectedIssue.status === "waiting"; onClicked: root.markStatus("waiting") }
                    Button { text: "完成"; highlighted: appController.selectedIssue.status === "completed"; onClicked: root.markStatus("completed") }
                    Item { Layout.fillWidth: true }
                    Label { visible: (appController.selectedIssue.remindAt || "").length > 0; text: "提醒 " + appController.selectedIssue.remindAt; color: appController.selectedIssue.reminderDue ? "#b42318" : palette.mid }
                    Button { text: "稍后提醒 ▾"; onClicked: reminderMenu.popup() }
                }
                Frame {
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
                                    root.submitTimeline((event.modifiers & Qt.ShiftModifier) !== 0)
                                    event.accepted = true
                                } else if (event.matches(StandardKey.Paste) && appController.clipboardHasImage()) {
                                    if (appController.addTimelineEntryWithClipboardImage(timelineType.currentValue, text)) {
                                        clear()
                                        forceActiveFocus()
                                    }
                                    event.accepted = true
                                }
                            }
                            DropArea {
                                anchors.fill: parent
                                onDropped: function(drop) {
                                    if (drop.hasUrls && appController.addTimelineEntryWithFiles(
                                            timelineType.currentValue, timelineInput.text, drop.urls))
                                        timelineInput.clear()
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox { id: timelineType; Layout.preferredWidth: 110; model: root.timelineTypeOptions; textRole: "label"; valueRole: "value"; onActivated: draftSaveTimer.restart() }
                            Button { text: "粘贴截图"; onClicked: if (appController.addTimelineEntryWithClipboardImage(timelineType.currentValue, timelineInput.text)) timelineInput.clear() }
                            Button { text: "附件/日志…"; onClicked: composerAttachmentDialog.open() }
                            Label { text: "草稿自动保存"; color: palette.mid; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Button { text: "记录进展"; enabled: timelineInput.text.trim().length > 0; onClicked: root.submitTimeline(true) }
                            Button { text: "记录"; highlighted: true; enabled: timelineInput.text.trim().length > 0; onClicked: root.submitTimeline(false) }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "处理时间线"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                    Label { text: "最近更新：" + (root.liveSelectedIssue().inactiveText || "刚刚") + "前"; color: palette.mid }
                }
                ListView {
                    id: timelineList
                    objectName: "detailScroll"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 7
                    reuseItems: true
                    model: appController.timeline
                    delegate: Frame {
                        required property var modelData
                        property bool editing: false
                        width: timelineList.width
                        height: timelineCard.implicitHeight + 18
                        ColumnLayout {
                            id: timelineCard
                            anchors.fill: parent
                            anchors.margins: 9
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: root.optionLabel(root.timelineTypeOptions, modelData.type); font.bold: true; color: palette.highlight }
                                Label { text: modelData.occurredAt; color: palette.mid; Layout.fillWidth: true }
                                Button { text: editing ? "取消" : "编辑"; flat: true; onClicked: editing = !editing }
                                Button { text: "删除"; flat: true; onClicked: appController.deleteTimelineEntry(modelData.id) }
                            }
                            Label { visible: !editing; Layout.fillWidth: true; text: modelData.content; wrapMode: Text.Wrap; textFormat: Text.MarkdownText }
                            TextArea { id: editContent; visible: editing; Layout.fillWidth: true; text: modelData.content; wrapMode: TextEdit.Wrap; implicitHeight: Math.max(80, contentHeight + 20) }
                            Button { visible: editing; text: "保存"; highlighted: true; onClicked: if (appController.saveTimelineEntry(modelData.id, modelData.type, editContent.text)) editing = false }
                            Repeater {
                                model: modelData.attachments
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Image { visible: modelData.mimeType.indexOf("image/") === 0; source: visible ? modelData.url : ""; Layout.preferredWidth: visible ? 100 : 0; Layout.preferredHeight: visible ? 68 : 0; fillMode: Image.PreserveAspectFit; asynchronous: true }
                                    Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideMiddle }
                                    Button { text: "打开"; flat: true; onClicked: appController.openAttachment(modelData.url) }
                                    Button { text: "删除"; flat: true; onClicked: appController.deleteAttachment(modelData.id) }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                Button { text: "添加截图"; flat: true; onClicked: appController.pasteScreenshot(modelData.id) }
                                Button { text: "添加附件"; flat: true; onClicked: { root.pendingAttachmentEntryId = modelData.id; attachmentDialog.open() } }
                            }
                        }
                    }
                    Label { anchors.centerIn: parent; visible: timelineList.count === 0; text: "还没有处理记录\n从上方随手记下第一条"; horizontalAlignment: Text.AlignHCenter; color: palette.mid }
                }
            }
            Label { anchors.centerIn: parent; visible: appController.selectedIssue.id === undefined; text: "从左侧选择问题\n或按 Ctrl/Cmd + N 快速记录"; horizontalAlignment: Text.AlignHCenter; color: palette.mid }
        }
    }

    footer: ToolBar {
        Label { anchors.fill: parent; anchors.leftMargin: 16; verticalAlignment: Text.AlignVCenter; text: appController.status; color: text.indexOf("失败") >= 0 ? "#b42318" : palette.mid; elide: Text.ElideRight }
    }

    Timer { id: searchDelay; interval: 250; onTriggered: root.applyFilter() }
    Timer { id: detailSaveTimer; interval: 800; onTriggered: if (root.detailDirty && appController.saveIssue(root.draftIssue)) root.detailDirty = false }
    Timer { id: draftSaveTimer; interval: 500; onTriggered: if (appController.selectedIssue.id !== undefined) appController.saveTimelineDraft(appController.selectedIssue.id, timelineType.currentValue, timelineInput.text) }
    Timer { interval: 60000; running: true; repeat: true; onTriggered: appController.refreshIssues() }

    Connections {
        target: appController
        function onSelectedIssueChanged() {
            root.draftIssue = Object.assign({}, appController.selectedIssue)
            root.detailDirty = false
            timelineList.contentY = 0
            const issueId = appController.selectedIssue.id || ""
            timelineInput.text = appController.loadTimelineDraft(issueId)
            timelineType.currentIndex = root.optionIndex(root.timelineTypeOptions,
                appController.loadTimelineDraftType(issueId))
        }
    }

    Component {
        id: textEditor
        TextField {
            property var field
            text: field ? (root.draftIssue[field.id] || "") : ""
            onTextEdited: if (field) root.setDraftField(field.id, text)
        }
    }
    Component {
        id: multilineEditor
        TextArea {
            property var field
            text: field ? (root.draftIssue[field.id] || "") : ""
            implicitHeight: 85
            wrapMode: TextEdit.Wrap
            onTextChanged: if (activeFocus && field) root.setDraftField(field.id, text)
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
        Label { property var field; text: field ? (root.draftIssue[field.id] || "—") : "—"; color: palette.mid; padding: 6 }
    }

    Dialog {
        id: quickCreate
        title: "快速记录问题"
        modal: true
        anchors.centerIn: parent
        width: 520
        onOpened: { quickTitle.clear(); quickReporter.clear(); quickTitle.forceActiveFocus() }
        ColumnLayout {
            anchors.fill: parent
            Label { text: "问题 *"; font.bold: true }
            TextArea { id: quickTitle; Layout.fillWidth: true; implicitHeight: 110; wrapMode: TextEdit.Wrap; placeholderText: "先记下来，其他信息可以稍后补充" }
            Label { text: "问题提出人" }
            TextField { id: quickReporter; Layout.fillWidth: true; placeholderText: "选填" }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "取消"; onClicked: quickCreate.close() }
                Button {
                    text: "先记下"
                    enabled: quickTitle.text.trim().length > 0
                    onClicked: root.createIssue(false)
                }
                Button {
                    text: "保存并开始处理"
                    highlighted: true
                    enabled: quickTitle.text.trim().length > 0
                    onClicked: root.createIssue(true)
                }
            }
        }
    }

    Dialog {
        id: detailsDialog
        title: "更多信息"
        modal: true
        anchors.centerIn: parent
        width: 650
        height: Math.min(root.height - 70, 650)
        standardButtons: Dialog.Close
        ScrollView {
            anchors.fill: parent
            ColumnLayout {
                width: detailsDialog.availableWidth
                Repeater {
                    model: appController.formFields
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        visible: modelData.id !== "status" && modelData.id !== "priority"
                        Label { text: modelData.label }
                        Loader { Layout.fillWidth: true; property var definition: modelData; sourceComponent: definition.readOnly ? readOnlyEditor : definition.type === "select" ? selectEditor : definition.type === "multiline_text" ? multilineEditor : textEditor; onLoaded: item.field = definition }
                    }
                }
            }
        }
    }

    Menu {
        id: reminderMenu
        MenuItem { text: "30 分钟后"; onTriggered: appController.remindSelectedIssueIn(30) }
        MenuItem { text: "2 小时后"; onTriggered: appController.remindSelectedIssueIn(120) }
        MenuItem { text: "明天此时"; onTriggered: appController.remindSelectedIssueIn(1440) }
        MenuSeparator {}
        MenuItem { text: "取消提醒"; enabled: (appController.selectedIssue.remindAt || "").length > 0; onTriggered: appController.clearSelectedIssueReminder() }
    }
    Menu {
        id: toolsMenu
        MenuItem { text: "验证工作区"; onTriggered: appController.verifyWorkspace() }
        MenuItem { text: "切换工作区…"; onTriggered: workspaceDialog.open() }
        MenuItem { text: "创建完整备份…"; onTriggered: backupDialog.open() }
        MenuItem { text: "从备份恢复…"; onTriggered: restoreFolderDialog.open() }
        MenuSeparator {}
        MenuItem { text: "导出问题列表 XLSX…"; onTriggered: xlsxExportDialog.open() }
        MenuItem { text: "从新版压缩包升级…"; onTriggered: updateArchiveDialog.open() }
    }

    FolderDialog { id: workspaceDialog; title: "选择或新建 IssueTrace 工作区"; onAccepted: appController.chooseWorkspace(selectedFolder) }
    FolderDialog { id: backupDialog; title: "选择备份保存位置"; onAccepted: appController.backupWorkspace(selectedFolder) }
    FolderDialog { id: restoreFolderDialog; title: "选择 IssueTraceBackup 备份目录"; onAccepted: { root.pendingRestoreFolder = selectedFolder; restoreConfirm.open() } }
    Dialog { id: restoreConfirm; title: "确认恢复工作区？"; modal: true; anchors.centerIn: parent; standardButtons: Dialog.Yes | Dialog.No; onAccepted: appController.restoreWorkspace(root.pendingRestoreFolder); Label { width: 450; wrapMode: Text.Wrap; text: "当前工作区将替换为备份；恢复前会自动创建安全快照。" } }
    FileDialog { id: composerAttachmentDialog; title: "选择日志或附件（每个最大 100 MB）"; fileMode: FileDialog.OpenFiles; onAccepted: if (appController.addTimelineEntryWithFiles(timelineType.currentValue, timelineInput.text, selectedFiles)) timelineInput.clear() }
    FileDialog { id: attachmentDialog; title: "选择附件（最大 100 MB）"; fileMode: FileDialog.OpenFile; onAccepted: appController.attachFile(root.pendingAttachmentEntryId, selectedFile) }
    FolderDialog { id: exportDialog; title: "选择 Markdown 总结导出位置"; onAccepted: appController.exportMarkdown(selectedFolder) }
    FileDialog { id: xlsxExportDialog; title: "导出问题列表"; fileMode: FileDialog.SaveFile; nameFilters: ["Excel 工作簿 (*.xlsx)"]; defaultSuffix: "xlsx"; onAccepted: appController.exportXlsx(selectedFile) }
    FileDialog { id: updateArchiveDialog; title: "选择更高版本的完整 IssueTrace 压缩包"; fileMode: FileDialog.OpenFile; nameFilters: ["IssueTrace 发布包 (*.zip *.tar.gz *.tgz)"]; onAccepted: { root.pendingUpdateArchive = selectedFile; updateConfirm.open() } }
    Dialog { id: updateConfirm; title: "确认离线升级？"; modal: true; anchors.centerIn: parent; standardButtons: Dialog.Yes | Dialog.No; onAccepted: appController.applyUpdate(root.pendingUpdateArchive); Label { width: 460; wrapMode: Text.Wrap; text: "IssueTrace 会校验包内文件并备份工作区，再替换和启动新版。包内哈希只能检查完整性，不能证明发布者身份；请只使用可信渠道取得且已单独核对 SHA-256 的压缩包。" } }
}
