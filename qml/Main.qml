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
    title: "IssueTrace · " + (currentPage === 0 ? "事件管理"
                               : currentPage === 1 ? "事件记录" : "事件日历")
    property double clockNow: Date.now()
    property int currentPage: 0
    property var quickDraft: ({priority: "normal"})

    property url pendingRestoreFolder: ""
    property url pendingUpdateArchive: ""
    property string pendingDeleteIssueId: ""
    property string pendingDeleteIssueTitle: ""
    property string pendingDeleteTimelineId: ""
    property string pendingDeleteTimelinePreview: ""
    property string pendingPermanentDeleteKind: ""
    property string pendingPermanentDeleteId: ""
    property string pendingPermanentDeleteTitle: ""
    readonly property var statusOptions: [
        { value: "pending", label: "待处理" },
        { value: "investigating", label: "处理中" },
        { value: "waiting", label: "等待" },
        { value: "completed", label: "已完成" }
    ]
    readonly property var timelineTypeOptions: [
        { value: "note", label: "记录" },
        { value: "progress", label: "进展" },
        { value: "conclusion", label: "结论" }
    ]

    function liveSelectedIssue() {
        const selectedId = App.selectedIssue.id
        for (let i = 0; i < App.issues.length; ++i)
            if (App.issues[i].id === selectedId) return App.issues[i]
        for (let i = 0; i < App.editorIssues.length; ++i)
            if (App.editorIssues[i].id === selectedId) return App.editorIssues[i]
        return App.selectedIssue
    }
    function selectIssue(id) {
        if (!issueDetails.flush()) return
        timelineComposer.saveDraft()
        App.selectIssue(id)
        currentPage = 1
    }
    function editIssue(id) {
        if (!issueDetails.flush()) return
        timelineComposer.saveDraft()
        App.selectIssue(id)
        currentPage = 0
        issueDetails.open()
    }
    function showManagement() {
        if (!issueDetails.flush()) return
        timelineComposer.saveDraft()
        currentPage = 0
    }
    function showCalendar() {
        if (!issueDetails.flush()) return
        timelineComposer.saveDraft()
        currentPage = 2
    }
    function markStatus(status) {
        if (!issueDetails.flush()) return
        App.setSelectedIssueStatus(status)
    }
    function createIssue(startNow) {
        if (!App.createQuickIssue(quickDraft.title || "", quickDraft.reporter || "",
                                  quickDraft.assignee || "",
                                  quickDraft.group_name || "",
                                  quickDraft.tags || "",
                                  quickDraft.version || "",
                                  quickDraft.service || "",
                                  quickDraft.priority || "normal",
                                  quickDraft.original_problem || "",
                                  quickDraft.ticket || "")) return
        if (startNow) App.setSelectedIssueStatus("investigating")
        quickCreate.close()
        currentPage = 1
        timelineComposer.forceInputFocus()
    }
    function openCustomReminder() {
        customReminderValue.text = Qt.formatDateTime(
            new Date(Date.now() + 2 * 60 * 60 * 1000), "yyyy-MM-dd HH:mm")
        customReminderError.text = ""
        customReminderDialog.open()
    }
    function requestIssueDeletion(issueId, issueTitle) {
        pendingDeleteIssueId = issueId
        pendingDeleteIssueTitle = issueTitle
        deleteIssueConfirm.open()
    }
    function requestTimelineDeletion(entryId, preview) {
        pendingDeleteTimelineId = entryId
        pendingDeleteTimelinePreview = preview
        deleteTimelineConfirm.open()
    }
    function requestPermanentDeletion(kind, id, title) {
        pendingPermanentDeleteKind = kind
        pendingPermanentDeleteId = id
        pendingPermanentDeleteTitle = title
        permanentDeleteConfirm.open()
    }
    function trackedDurationText() {
        let value = Number(App.selectedIssue.trackedMilliseconds || 0)
        if (App.selectedIssue.timerRunning)
            value += Math.max(0, clockNow - Number(App.selectedIssue.timerStartedAtMs || clockNow))
        const seconds = Math.floor(value / 1000)
        const hours = Math.floor(seconds / 3600)
        const minutes = Math.floor((seconds % 3600) / 60)
        const rest = seconds % 60
        return (hours > 0 ? hours + ":" : "")
            + String(minutes).padStart(2, "0") + ":" + String(rest).padStart(2, "0")
    }

    onClosing: function(close) {
        if (!issueDetails.flush()) {
            close.accepted = false
            return
        }
        timelineComposer.saveDraft()
    }
    Shortcut { sequences: [StandardKey.New]; onActivated: quickCreate.open() }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            Label {
                text: "IssueTrace " + App.appVersion
                font.pixelSize: 20
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: "记下来 · 持续跟进 · 到时提醒"
                color: palette.mid
            }
            Rectangle {
                id: navigationTabs
                objectName: "mainNavigationTabs"
                property int currentIndex: root.currentPage
                Layout.preferredWidth: 330
                Layout.preferredHeight: 38
                color: root.palette.midlight
                radius: 7
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3
                Rectangle {
                    id: managementTab
                    objectName: "managementTab"
                    property bool checked: root.currentPage === 0
                    function activate() { root.showManagement() }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 5
                    color: checked ? root.palette.highlight : "transparent"
                    Label {
                        anchors.centerIn: parent
                        text: "事件管理"
                        color: managementTab.checked ? root.palette.highlightedText : root.palette.text
                        font.bold: managementTab.checked
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: managementTab.activate()
                    }
                }
                Rectangle {
                    id: recordTab
                    objectName: "recordTab"
                    property bool checked: root.currentPage === 1
                    enabled: App.selectedIssue.id !== undefined
                    function activate() { if (enabled) root.currentPage = 1 }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 5
                    color: checked ? root.palette.highlight : "transparent"
                    opacity: enabled ? 1 : 0.45
                    Label {
                        anchors.centerIn: parent
                        text: "事件记录"
                        color: recordTab.checked ? root.palette.highlightedText : root.palette.text
                        font.bold: recordTab.checked
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: recordTab.enabled
                        cursorShape: Qt.PointingHandCursor
                        onClicked: recordTab.activate()
                    }
                }
                Rectangle {
                    id: calendarTab
                    objectName: "calendarTab"
                    property bool checked: root.currentPage === 2
                    function activate() { root.showCalendar() }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 5
                    color: checked ? root.palette.highlight : "transparent"
                    Label {
                        anchors.centerIn: parent
                        text: "事件日历"
                        color: calendarTab.checked ? root.palette.highlightedText : root.palette.text
                        font.bold: calendarTab.checked
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: calendarTab.activate()
                    }
                }
                }
            }
            Label {
                visible: issueDetails.dirty
                text: "有未保存修改"
                color: "#b66a00"
            }
            Button { text: "工具"; onClicked: toolsMenu.popup() }
            Button {
                id: quickCreateButton
                objectName: "quickCreateButton"
                text: "+ 记录事件"
                onClicked: quickCreate.open()
                contentItem: Label {
                    text: quickCreateButton.text
                    color: root.palette.highlightedText
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 7
                    color: root.palette.highlight
                    opacity: quickCreateButton.down ? 0.82
                        : quickCreateButton.hovered ? 0.92 : 1
                }
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.currentPage

        IssueInbox {
            controller: App
            selectedIssueId: App.selectedIssue.id || ""
            statusOptions: root.statusOptions
            onIssueRequested: issueId => root.selectIssue(issueId)
            onEditIssueRequested: issueId => root.editIssue(issueId)
            onDeleteIssueRequested: (issueId, issueTitle) =>
                root.requestIssueDeletion(issueId, issueTitle)
            onFilterRequested: (text, status, priority, tags, minimumMinutes,
                                maximumMinutes, groupPath, service, version,
                                ticket, sort) =>
                App.filterIssues(text, status, service, "", sort, 0, priority,
                                 tags, "", minimumMinutes, maximumMinutes,
                                 groupPath, version, ticket)
        }

        SplitView {
            IssueEditorList {
                SplitView.preferredWidth: 320
                SplitView.minimumWidth: 265
                SplitView.maximumWidth: 430
                controller: App
                selectedIssueId: App.selectedIssue.id || ""
                statusOptions: root.statusOptions
                onIssueRequested: issueId => root.selectIssue(issueId)
            }

            Pane {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 560
                padding: 14

            ColumnLayout {
                anchors.fill: parent
                visible: App.selectedIssue.id !== undefined
                spacing: 9

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "‹ 返回管理"
                        onClicked: root.showManagement()
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: App.selectedIssue.title || ""
                            font.pixelSize: 22
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            text: "事件已记录 " + (root.liveSelectedIssue().ageText || "")
                                + " · 当前状态停留 "
                                + (root.liveSelectedIssue().statusDurationText || "")
                            color: palette.mid
                        }
                    }
                    Button { text: "更多信息"; onClicked: issueDetails.open() }
                    Label {
                        text: root.trackedDurationText()
                        font.family: "monospace"
                        font.bold: true
                    }
                    Button {
                        text: App.selectedIssue.timerRunning ? "暂停计时" : "开始计时"
                        highlighted: App.selectedIssue.timerRunning === true
                        onClicked: App.selectedIssue.timerRunning
                            ? App.pauseSelectedIssueTimer() : App.startSelectedIssueTimer()
                    }
                    Button { text: "导出事件"; onClicked: exportDialog.open() }
                    QuietButton {
                        text: "删除事件"
                        onClicked: root.requestIssueDeletion(
                            App.selectedIssue.id, App.selectedIssue.title)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    ButtonGroup { id: statusButtonGroup; exclusive: true }
                    Button {
                        objectName: "statusPendingButton"
                        text: "待处理"
                        checkable: true
                        checked: App.selectedIssueStatus === "pending"
                        highlighted: checked
                        ButtonGroup.group: statusButtonGroup
                        onClicked: root.markStatus("pending")
                    }
                    Button {
                        objectName: "statusInvestigatingButton"
                        text: "开始处理"
                        checkable: true
                        checked: App.selectedIssueStatus === "investigating"
                        highlighted: checked
                        ButtonGroup.group: statusButtonGroup
                        onClicked: root.markStatus("investigating")
                    }
                    Button {
                        objectName: "statusWaitingButton"
                        text: "等待"
                        checkable: true
                        checked: App.selectedIssueStatus === "waiting"
                        highlighted: checked
                        ButtonGroup.group: statusButtonGroup
                        onClicked: root.markStatus("waiting")
                    }
                    Button {
                        objectName: "statusCompletedButton"
                        text: "完成"
                        checkable: true
                        checked: App.selectedIssueStatus === "completed"
                        highlighted: checked
                        ButtonGroup.group: statusButtonGroup
                        onClicked: root.markStatus("completed")
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        visible: (App.selectedIssue.remindAt || "").length > 0
                        text: "提醒 " + App.selectedIssue.remindAt
                        color: App.selectedIssue.reminderDue ? "#b42318" : palette.mid
                    }
                    Button { text: "稍后提醒 ▾"; onClicked: reminderMenu.popup() }
                }

                TimelineComposer {
                    id: timelineComposer
                    controller: App
                    typeOptions: root.timelineTypeOptions
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "处理时间线"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Label {
                        text: "最近更新：" + (root.liveSelectedIssue().inactiveText || "刚刚") + "前"
                        color: palette.mid
                    }
                }

                TimelineList {
                    id: timelineList
                    controller: App
                    typeOptions: root.timelineTypeOptions
                    onDeleteRequested: (entryId, preview) =>
                        root.requestTimelineDeletion(entryId, preview)
                }
            }

                Label {
                    anchors.centerIn: parent
                    visible: App.selectedIssue.id === undefined
                    text: "请从左侧选择事件\n或按 Ctrl/Cmd + N 快速记录"
                    horizontalAlignment: Text.AlignHCenter
                    color: palette.mid
                }
            }
        }

        IssueCalendar {
            controller: App
            statusOptions: root.statusOptions
            onIssueRequested: issueId => root.selectIssue(issueId)
        }
    }

    footer: ToolBar {
        Label {
            anchors.fill: parent
            anchors.leftMargin: 16
            verticalAlignment: Text.AlignVCenter
            text: App.status
            color: text.indexOf("失败") >= 0 ? "#b42318" : palette.mid
            elide: Text.ElideRight
        }
    }

    Timer { interval: 1000; running: true; repeat: true; onTriggered: root.clockNow = Date.now() }
    Timer { interval: 60000; running: true; repeat: true; onTriggered: App.refreshIssues() }
    Connections {
        target: App
        function onSelectedIssueChanged() { timelineList.contentY = 0 }
    }

    IssueDetailsDialog { id: issueDetails; controller: App }

    Dialog {
        id: quickCreate
        title: "快速记录事件"
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 60, 780)
        height: Math.min(parent.height - 60, 650)
        onOpened: {
            root.quickDraft = {priority: "normal"}
            quickForm.focusTitle()
        }

        footer: DialogButtonBox {
            Button { text: "取消"; onClicked: quickCreate.close() }
            Button {
                text: "先记下"
                enabled: (root.quickDraft.title || "").trim().length > 0
                onClicked: root.createIssue(false)
            }
            Button {
                text: "保存并开始处理"
                highlighted: true
                enabled: (root.quickDraft.title || "").trim().length > 0
                onClicked: root.createIssue(true)
            }
        }

        ScrollView {
            id: quickScroll
            anchors.fill: parent
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: quickScroll.availableWidth
                spacing: 12
                EventFormFields {
                    id: quickForm
                    Layout.fillWidth: true
                    controller: App
                    values: root.quickDraft
                    compact: true
                    onFieldEdited: function(fieldId, value) {
                        const updated = Object.assign({}, root.quickDraft)
                        updated[fieldId] = value
                        root.quickDraft = updated
                    }
                }
            }
        }
    }

    Menu {
        id: reminderMenu
        MenuItem { text: "30 分钟后"; onTriggered: App.remindSelectedIssueIn(30) }
        MenuItem { text: "2 小时后"; onTriggered: App.remindSelectedIssueIn(120) }
        MenuItem { text: "明天此时"; onTriggered: App.remindSelectedIssueIn(1440) }
        MenuItem { text: "自定义时间…"; onTriggered: root.openCustomReminder() }
        MenuSeparator {}
        MenuItem {
            text: "取消提醒"
            enabled: (App.selectedIssue.remindAt || "").length > 0
            onTriggered: App.clearSelectedIssueReminder()
        }
    }
    Dialog {
        id: customReminderDialog
        title: "自定义提醒时间"
        modal: true
        anchors.centerIn: parent
        width: 430

        ColumnLayout {
            anchors.fill: parent
            Label { text: "提醒时间（本机时区）"; font.bold: true }
            TextField {
                id: customReminderValue
                Layout.fillWidth: true
                placeholderText: "yyyy-MM-dd HH:mm"
                selectByMouse: true
                onAccepted: customReminderSave.clicked()
            }
            Label {
                text: "示例：2026-09-16 14:30"
                color: palette.mid
                font.pixelSize: 12
            }
            Label {
                id: customReminderError
                Layout.fillWidth: true
                visible: text.length > 0
                color: "#b42318"
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "取消"; onClicked: customReminderDialog.close() }
                Button {
                    id: customReminderSave
                    text: "设置提醒"
                    highlighted: true
                    onClicked: {
                        if (App.remindSelectedIssueAt(customReminderValue.text)) {
                            customReminderDialog.close()
                        } else {
                            customReminderError.text = App.status
                            customReminderValue.forceActiveFocus()
                        }
                    }
                }
            }
        }
        onOpened: customReminderValue.forceActiveFocus()
    }
    Dialog {
        id: deleteIssueConfirm
        objectName: "deleteIssueConfirmDialog"
        title: "确认删除事件？"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            if (App.deleteIssue(root.pendingDeleteIssueId)) root.currentPage = 0
            root.pendingDeleteIssueId = ""
            root.pendingDeleteIssueTitle = ""
        }
        onRejected: {
            root.pendingDeleteIssueId = ""
            root.pendingDeleteIssueTitle = ""
        }
        ColumnLayout {
            width: 420
            Label {
                Layout.fillWidth: true
                text: root.pendingDeleteIssueTitle
                font.bold: true
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: "删除后事件将进入回收站，并从管理、记录和日历页隐藏。"
                color: palette.mid
                wrapMode: Text.Wrap
            }
        }
    }
    Dialog {
        id: deleteTimelineConfirm
        objectName: "deleteTimelineConfirmDialog"
        title: "确认删除这条记录？"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            App.deleteTimelineEntry(root.pendingDeleteTimelineId)
            root.pendingDeleteTimelineId = ""
            root.pendingDeleteTimelinePreview = ""
        }
        onRejected: {
            root.pendingDeleteTimelineId = ""
            root.pendingDeleteTimelinePreview = ""
        }
        ColumnLayout {
            width: 420
            Label {
                Layout.fillWidth: true
                text: root.pendingDeleteTimelinePreview
                maximumLineCount: 4
                elide: Text.ElideRight
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: "该记录将进入回收站；关联的图片和附件会一并隐藏。"
                color: palette.mid
                wrapMode: Text.Wrap
            }
        }
    }
    TrashDialog {
        id: trashDialog
        controller: App
        onPermanentIssueRequested: (issueId, title) =>
            root.requestPermanentDeletion("issue", issueId, title)
        onPermanentTimelineRequested: (entryId, preview) =>
            root.requestPermanentDeletion("timeline", entryId, preview)
    }
    Dialog {
        id: permanentDeleteConfirm
        objectName: "permanentDeleteConfirmDialog"
        title: "确认永久删除？"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            if (root.pendingPermanentDeleteKind === "issue")
                App.permanentlyDeleteIssue(root.pendingPermanentDeleteId)
            else if (root.pendingPermanentDeleteKind === "timeline")
                App.permanentlyDeleteTimelineEntry(root.pendingPermanentDeleteId)
            root.pendingPermanentDeleteKind = ""
            root.pendingPermanentDeleteId = ""
            root.pendingPermanentDeleteTitle = ""
        }
        onRejected: {
            root.pendingPermanentDeleteKind = ""
            root.pendingPermanentDeleteId = ""
            root.pendingPermanentDeleteTitle = ""
        }
        ColumnLayout {
            width: 440
            Label {
                Layout.fillWidth: true
                text: root.pendingPermanentDeleteTitle
                font.bold: true
                maximumLineCount: 3
                elide: Text.ElideRight
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: root.pendingPermanentDeleteKind === "issue"
                    ? "事件、全部处理记录及附件文件都会被永久删除，且无法恢复。"
                    : "该记录及其附件文件都会被永久删除，且无法恢复。"
                color: "#b42318"
                wrapMode: Text.Wrap
            }
        }
    }
    Menu {
        id: toolsMenu
        MenuItem { text: "回收站…"; onTriggered: trashDialog.open() }
        MenuSeparator {}
        MenuItem { text: "验证工作区"; onTriggered: App.verifyWorkspace() }
        MenuItem { text: "切换工作区…"; onTriggered: workspaceDialog.open() }
        MenuItem { text: "创建完整备份…"; onTriggered: backupDialog.open() }
        MenuItem { text: "从备份恢复…"; onTriggered: restoreFolderDialog.open() }
        MenuSeparator {}
        MenuItem { text: "导出事件列表 XLSX…"; onTriggered: xlsxExportDialog.open() }
        MenuItem { text: "从新版压缩包升级…"; onTriggered: updateArchiveDialog.open() }
    }

    FolderDialog {
        id: workspaceDialog
        title: "选择或新建 IssueTrace 工作区"
        onAccepted: App.chooseWorkspace(selectedFolder)
    }
    FolderDialog {
        id: backupDialog
        title: "选择备份保存位置"
        onAccepted: App.backupWorkspace(selectedFolder)
    }
    FolderDialog {
        id: restoreFolderDialog
        title: "选择 IssueTraceBackup 备份目录"
        onAccepted: {
            root.pendingRestoreFolder = selectedFolder
            restoreConfirm.open()
        }
    }
    Dialog {
        id: restoreConfirm
        title: "确认恢复工作区？"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: App.restoreWorkspace(root.pendingRestoreFolder)
        Label {
            width: 450
            wrapMode: Text.Wrap
            text: "当前工作区将替换为备份；恢复前会自动创建安全快照。"
        }
    }
    FolderDialog {
        id: exportDialog
        title: "选择事件 Markdown 导出位置"
        onAccepted: App.exportMarkdown(selectedFolder)
    }
    FileDialog {
        id: xlsxExportDialog
        title: "导出事件列表"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Excel 工作簿 (*.xlsx)"]
        defaultSuffix: "xlsx"
        onAccepted: App.exportXlsx(selectedFile)
    }
    FileDialog {
        id: updateArchiveDialog
        title: "选择更高版本的完整 IssueTrace 压缩包"
        fileMode: FileDialog.OpenFile
        nameFilters: ["IssueTrace 发布包 (*.zip *.tar.gz *.tgz)"]
        onAccepted: {
            root.pendingUpdateArchive = selectedFile
            updateConfirm.open()
        }
    }
    Dialog {
        id: updateConfirm
        title: "确认离线升级？"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: App.applyUpdate(root.pendingUpdateArchive)
        Label {
            width: 460
            wrapMode: Text.Wrap
            text: "IssueTrace 会校验包内文件并备份工作区，再替换和启动新版。包内哈希只能检查完整性，不能证明发布者身份；请只使用可信渠道取得且已单独核对 SHA-256 的压缩包。"
        }
    }
}
