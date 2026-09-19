import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    objectName: "issueCalendar"
    required property var controller
    required property var statusOptions
    property date displayedMonth: new Date(new Date().getFullYear(), new Date().getMonth(), 1)
    property date selectedDate: new Date()
    property string displayMode: "received"
    signal issueRequested(string issueId)

    function dateKey(value) { return Qt.formatDate(value, "yyyy-MM-dd") }
    function sameDay(left, right) { return dateKey(left) === dateKey(right) }
    function dateForCell(index) {
        const first = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth(), 1)
        const mondayOffset = (first.getDay() + 6) % 7
        return new Date(first.getFullYear(), first.getMonth(), 1 - mondayOffset + index)
    }
    function issuesForDate(value) {
        const key = dateKey(value)
        const result = []
        const dayStart = new Date(value.getFullYear(), value.getMonth(), value.getDate()).getTime()
        const dayEnd = new Date(value.getFullYear(), value.getMonth(), value.getDate() + 1).getTime() - 1
        for (let i = 0; i < controller.calendarIssues.length; ++i) {
            const issue = controller.calendarIssues[i]
            if (displayMode === "received") {
                if ((issue.reportedAt || "").substring(0, 10) === key) result.push(issue)
            } else {
                const start = Number(issue.createdAtMs || 0)
                const resolved = Number(issue.resolvedAtMs || 0)
                const end = resolved > 0 ? resolved : Date.now()
                if (start <= dayEnd && Math.max(start, end) >= dayStart)
                    result.push(issue)
            }
        }
        result.sort((left, right) => displayMode === "received"
            ? Number(left.reportedAtMs) - Number(right.reportedAtMs)
            : Number(left.createdAtMs) - Number(right.createdAtMs))
        return result
    }
    function issueTimeLabel(issue) {
        if (displayMode === "received")
            return (issue.reportedAt || "").substring(11)
                + (issue.group_name ? " · " + issue.group_name : "")
        const end = issue.resolvedAt ? issue.resolvedAt : "进行中"
        return (issue.createdAt || "") + " → " + end
    }
    function statusLabel(value) {
        for (let i = 0; i < statusOptions.length; ++i)
            if (statusOptions[i].value === value) return statusOptions[i].label
        return value
    }
    function showPreviousMonth() {
        displayedMonth = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth() - 1, 1)
        selectedDate = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth(), 1)
    }
    function showNextMonth() {
        displayedMonth = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth() + 1, 1)
        selectedDate = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth(), 1)
    }
    function showToday() {
        const today = new Date()
        displayedMonth = new Date(today.getFullYear(), today.getMonth(), 1)
        selectedDate = today
    }

    padding: 16

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: "事件日历"; font.pixelSize: 23; font.bold: true }
            Label {
                text: root.displayMode === "received"
                    ? "按接收日期回顾每天的事件"
                    : "查看事件从创建到完成的持续区间"
                color: palette.mid
            }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: calendarMode
                objectName: "calendarDisplayMode"
                model: [{value:"received",label:"按接收日"},
                        {value:"active_range",label:"按处理区间"}]
                textRole: "label"
                valueRole: "value"
                onActivated: root.displayMode = currentValue
            }
            Button { text: "今天"; onClicked: root.showToday() }
            ToolButton { text: "‹"; onClicked: root.showPreviousMonth() }
            Label {
                Layout.preferredWidth: 120
                horizontalAlignment: Text.AlignHCenter
                text: Qt.formatDate(root.displayedMonth, "yyyy 年 M 月")
                font.bold: true
            }
            ToolButton { text: "›"; onClicked: root.showNextMonth() }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Pane {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 520
                padding: 0
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Repeater {
                            model: ["一", "二", "三", "四", "五", "六", "日"]
                            delegate: Label {
                                required property string modelData
                                Layout.fillWidth: true
                                text: modelData
                                horizontalAlignment: Text.AlignHCenter
                                color: palette.mid
                                font.pixelSize: 12
                            }
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columns: 7
                        rows: 6
                        columnSpacing: 5
                        rowSpacing: 5
                        Repeater {
                            model: 42
                            delegate: Rectangle {
                                required property int index
                                property date cellDate: root.dateForCell(index)
                                property var dayIssues: root.issuesForDate(cellDate)
                                readonly property bool inCurrentMonth:
                                    cellDate.getMonth() === root.displayedMonth.getMonth()
                                readonly property bool selected: root.sameDay(cellDate, root.selectedDate)
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumWidth: 58
                                Layout.minimumHeight: 62
                                radius: 8
                                color: selected
                                    ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                              palette.highlight.b, 0.13)
                                    : dayMouse.containsMouse ? palette.alternateBase : palette.base
                                border.width: selected || root.sameDay(cellDate, new Date()) ? 1.5 : 1
                                border.color: selected ? palette.highlight : palette.midlight
                                opacity: inCurrentMonth ? 1 : 0.48

                                Label {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.margins: 8
                                    text: parent.cellDate.getDate()
                                    font.bold: parent.selected
                                }
                                Rectangle {
                                    visible: parent.dayIssues.length > 0
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 7
                                    width: dayCount.implicitWidth + 12
                                    height: 22
                                    radius: 11
                                    color: palette.highlight
                                    Label {
                                        id: dayCount
                                        anchors.centerIn: parent
                                        text: parent.parent.dayIssues.length
                                        color: palette.highlightedText
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }
                                Label {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 7
                                    visible: parent.dayIssues.length > 0
                                    text: parent.dayIssues.length > 0
                                        ? parent.dayIssues[0].title : ""
                                    color: palette.mid
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                                MouseArea {
                                    id: dayMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.selectedDate = new Date(parent.cellDate)
                                }
                            }
                        }
                    }
                }
            }

            Pane {
                SplitView.preferredWidth: 340
                SplitView.minimumWidth: 270
                padding: 12
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label {
                        text: Qt.formatDate(root.selectedDate, "M 月 d 日 dddd")
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Label {
                        text: dayEventList.count + " 个事件"
                        color: palette.mid
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.midlight }
                    ListView {
                        id: dayEventList
                        objectName: "calendarDayEventList"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6
                        clip: true
                        model: root.issuesForDate(root.selectedDate)
                        delegate: ItemDelegate {
                            required property var modelData
                            width: dayEventList.width
                            height: 72
                            onClicked: root.issueRequested(modelData.id)
                            contentItem: ColumnLayout {
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: root.statusLabel(modelData.status)
                                        color: palette.highlight
                                        font.pixelSize: 11
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.issueTimeLabel(modelData)
                                    color: palette.mid
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: dayEventList.count === 0
                            text: root.displayMode === "received"
                                ? "这一天没有收到事件"
                                : "这一天没有处理中的事件"
                            color: palette.mid
                        }
                    }
                }
            }
        }
    }
}
