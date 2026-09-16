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
    title: "IssueTrace · 事件管理"
    property double clockNow: Date.now()
    property int currentPage: 0

    property url pendingRestoreFolder: ""
    property url pendingUpdateArchive: ""
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
    function markStatus(status) {
        if (!issueDetails.flush()) return
        App.setSelectedIssueStatus(status)
    }
    function createIssue(startNow) {
        if (!App.createQuickIssue(quickTitle.text, quickReporter.text)) return
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
                Layout.preferredWidth: 230
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
                }
            }
            Label {
                visible: issueDetails.dirty
                text: "保存中…"
                color: "#b66a00"
            }
            Button { text: "工具"; onClicked: toolsMenu.popup() }
            Button { text: "+ 记录事件"; highlighted: true; onClicked: quickCreate.open() }
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
        width: 520
        onOpened: {
            quickTitle.clear()
            quickReporter.clear()
            quickTitle.forceActiveFocus()
        }
        ColumnLayout {
            anchors.fill: parent
            Label { text: "事件 *"; font.bold: true }
            TextArea {
                id: quickTitle
                Layout.fillWidth: true
                implicitHeight: 110
                wrapMode: TextEdit.Wrap
                placeholderText: "先记下来，其他信息可以稍后补充"
            }
            Label { text: "事件提出人" }
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
    Menu {
        id: toolsMenu
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
