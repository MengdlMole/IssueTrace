import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    objectName: "issueEditorList"
    required property var controller
    required property var statusOptions
    property string selectedIssueId: ""
    signal issueRequested(string issueId)

    function statusLabel(value) {
        for (let i = 0; i < statusOptions.length; ++i)
            if (statusOptions[i].value === value) return statusOptions[i].label
        return value
    }
    function attentionReason(issue) {
        if (issue.reminderDue) return "提醒时间 " + issue.remindAt + " 已到"
        if (issue.status === "pending") return "待处理已 " + issue.ageText
        return "已 " + issue.inactiveText + " 没有更新"
    }
    function matchesAttention(issue) {
        if (priorityFilter.currentValue && issue.priority !== priorityFilter.currentValue)
            return false
        const needle = searchInput.text.trim().toLowerCase()
        if (!needle) return true
        return [issue.title, issue.originalProblem, issue.tags, issue.progress]
            .join(" ").toLowerCase().indexOf(needle) >= 0
    }
    function priorityRank(value) {
        if (value === "urgent") return 0
        if (value === "high") return 1
        if (value === "normal") return 2
        return 3
    }
    function filteredAttention() {
        const values = []
        for (let i = 0; i < controller.attentionIssues.length; ++i)
            if (matchesAttention(controller.attentionIssues[i]))
                values.push(controller.attentionIssues[i])
        values.sort(function(left, right) {
            if (sortFilter.currentValue === "priority_desc")
                return priorityRank(left.priority) - priorityRank(right.priority)
            if (sortFilter.currentValue === "created_desc")
                return Number(right.createdAtMs || 0) - Number(left.createdAtMs || 0)
            return Number(right.updatedAtMs || 0) - Number(left.updatedAtMs || 0)
        })
        return values
    }
    function applyFilter() {
        controller.filterEditorIssues(priorityFilter.currentValue,
                                      sortFilter.currentValue, searchInput.text)
    }

    padding: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 7
            Label { text: "事件列表"; font.pixelSize: 18; font.bold: true }
            TextField {
                id: searchInput
                Layout.fillWidth: true
                placeholderText: "搜索标题、描述、进展或标签"
                onTextEdited: searchDelay.restart()
                onAccepted: root.applyFilter()
            }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: priorityFilter
                    Layout.fillWidth: true
                    model: [{value:"",label:"全部优先级"},{value:"urgent",label:"紧急"},
                        {value:"high",label:"高"},{value:"normal",label:"普通"},{value:"low",label:"低"}]
                    textRole: "label"; valueRole: "value"
                    onActivated: root.applyFilter()
                }
                ComboBox {
                    id: sortFilter
                    Layout.fillWidth: true
                    model: [{value:"updated_desc",label:"最近修改"},
                        {value:"created_desc",label:"创建时间"},
                        {value:"priority_desc",label:"优先级"}]
                    textRole: "label"; valueRole: "value"
                    onActivated: root.applyFilter()
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.midlight }

        SplitView {
            orientation: Qt.Vertical
            Layout.fillWidth: true
            Layout.fillHeight: true

            Pane {
                SplitView.preferredHeight: 220
                SplitView.minimumHeight: 90
                padding: 0
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4
                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.topMargin: 8
                        text: "需要关注 · " + attentionList.count
                        font.bold: true
                        color: "#b42318"
                    }
                    ListView {
                        id: attentionList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.filteredAttention()
                        delegate: ItemDelegate {
                            id: attentionDelegate
                            required property var modelData
                            x: 4
                            width: attentionList.width - 8
                            height: 72
                            hoverEnabled: true
                            highlighted: root.selectedIssueId === modelData.id
                            onClicked: root.issueRequested(modelData.id)
                            background: Rectangle {
                                radius: 8
                                color: attentionDelegate.highlighted
                                    ? palette.highlight
                                    : attentionDelegate.hovered
                                      ? palette.alternateBase : "transparent"
                                border.width: attentionDelegate.highlighted ? 0 : 1
                                border.color: palette.midlight
                            }
                            contentItem: ColumnLayout {
                                spacing: 3
                                Label { Layout.fillWidth: true; text: "!  " + modelData.title; font.bold: true; color: attentionDelegate.highlighted ? palette.highlightedText : "#b42318"; elide: Text.ElideRight }
                                Label { Layout.fillWidth: true; text: root.attentionReason(modelData); color: attentionDelegate.highlighted ? palette.highlightedText : "#b42318"; font.pixelSize: 12; elide: Text.ElideRight }
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: attentionList.count === 0
                            text: "当前没有需要关注的事件"
                            color: palette.mid
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Pane {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 120
                padding: 0
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4
                    Label {
                        Layout.leftMargin: 10
                        Layout.topMargin: 8
                        text: "全部事件 · " + normalList.count
                        font.bold: true
                    }
                    ListView {
                        id: normalList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.controller.editorIssues
                        delegate: ItemDelegate {
                            id: eventDelegate
                            required property var modelData
                            x: 4
                            width: normalList.width - 8
                            height: 68
                            hoverEnabled: true
                            highlighted: root.selectedIssueId === modelData.id
                            onClicked: root.issueRequested(modelData.id)
                            background: Rectangle {
                                radius: 8
                                color: eventDelegate.highlighted
                                    ? palette.highlight
                                    : eventDelegate.hovered
                                      ? palette.alternateBase : "transparent"
                                border.width: eventDelegate.highlighted ? 0 : 1
                                border.color: palette.midlight
                            }
                            contentItem: ColumnLayout {
                                spacing: 3
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { Layout.fillWidth: true; text: modelData.title; font.bold: true; color: eventDelegate.highlighted ? palette.highlightedText : palette.text; elide: Text.ElideRight }
                                    Label { text: root.statusLabel(modelData.status); color: eventDelegate.highlighted ? palette.highlightedText : palette.highlight; font.pixelSize: 11 }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "修改于 " + modelData.updatedAt
                                        + (modelData.progress ? " · " + modelData.progress : "")
                                    color: eventDelegate.highlighted ? palette.highlightedText : palette.mid
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Timer { id: searchDelay; interval: 250; onTriggered: root.applyFilter() }
}
