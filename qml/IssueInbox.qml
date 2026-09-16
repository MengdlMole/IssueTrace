import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    objectName: "issueInbox"
    required property var controller
    required property var statusOptions
    property string selectedIssueId: ""
    property string selectedGroupPath: ""
    property var selectedTags: []
    property var collapsedGroups: ({})
    property bool advancedFiltersVisible: false
    readonly property bool groupDragUsesMovableProxy: true
    readonly property string selectedIssueStatusText: {
        for (let i = 0; i < controller.issues.length; ++i) {
            const issue = controller.issues[i]
            if (issue.id === selectedIssueId) return statusLabel(issue.status)
        }
        return ""
    }
    signal issueRequested(string issueId)
    signal editIssueRequested(string issueId)
    signal filterRequested(string text, string status, string priority, string tags,
                           int minimumMinutes, int maximumMinutes, string groupPath,
                           string service, string version, string ticket, string sort)

    function statusLabel(value) {
        for (let i = 0; i < statusOptions.length; ++i)
            if (statusOptions[i].value === value) return statusOptions[i].label
        return value
    }
    function priorityLabel(value) {
        if (value === "urgent") return "紧急"
        if (value === "high") return "高"
        if (value === "low") return "低"
        return "普通"
    }
    function priorityColor(value) {
        if (value === "urgent") return "#d92d20"
        if (value === "high") return "#d97706"
        if (value === "low") return "#667085"
        return "#2563eb"
    }
    function statusColor(value) {
        if (value === "investigating") return "#2563eb"
        if (value === "waiting") return "#d97706"
        if (value === "completed") return "#15803d"
        return "#667085"
    }
    function durationLabel(milliseconds) {
        const minutes = Math.floor(Number(milliseconds || 0) / 60000)
        if (minutes < 60) return minutes + " 分钟"
        return Math.floor(minutes / 60) + " 小时 " + (minutes % 60) + " 分钟"
    }
    function tagsFor(value) {
        if (!value) return []
        return value.split(",").map(tag => tag.trim()).filter(tag => tag.length > 0)
    }
    function tagSelected(tag) { return selectedTags.indexOf(tag) >= 0 }
    function toggleTag(tag) {
        const next = selectedTags.slice()
        const index = next.indexOf(tag)
        if (index >= 0) next.splice(index, 1)
        else next.push(tag)
        selectedTags = next
        applyFilters()
    }
    function clearTags() {
        selectedTags = []
        applyFilters()
    }
    function selectedGroupLabel() {
        if (selectedGroupPath === "") return "全部事件"
        if (selectedGroupPath === "__default__") return "默认分组"
        return selectedGroupPath.replace(/\//g, "  ›  ")
    }
    function groupVisible(path) {
        if (!path || path === "__default__") return true
        const levels = path.split("/")
        let parent = ""
        for (let i = 0; i < levels.length - 1; ++i) {
            parent = parent ? parent + "/" + levels[i] : levels[i]
            if (collapsedGroups[parent]) return false
        }
        return true
    }
    function toggleGroup(path) {
        if (!path) return
        const next = Object.assign({}, collapsedGroups)
        next[path] = !next[path]
        collapsedGroups = next
    }
    function setAllCollapsed(collapsed) {
        const next = {}
        if (collapsed) {
            for (let i = 0; i < controller.issueGroups.length; ++i) {
                const group = controller.issueGroups[i]
                if (group.hasChildren && group.path) next[group.path] = true
            }
        }
        collapsedGroups = next
    }
    function applyFilters() {
        filterRequested(searchInput.text, statusFilter.currentValue,
                        priorityFilter.currentValue, selectedTags.join(","),
                        minimumDuration.value, maximumDuration.value,
                        selectedGroupPath,
                        serviceFilter.currentIndex > 0 ? serviceFilter.currentText : "",
                        versionFilter.currentIndex > 0 ? versionFilter.currentText : "",
                        ticketFilter.text, sortFilter.currentValue)
    }
    function selectGroup(path) {
        selectedGroupPath = path
        applyFilters()
    }
    function activeFilterCount() {
        let count = 0
        if (searchInput.text.trim()) ++count
        if (statusFilter.currentValue) ++count
        if (priorityFilter.currentValue) ++count
        if (selectedGroupPath) ++count
        count += selectedTags.length
        if (serviceFilter.currentIndex > 0) ++count
        if (versionFilter.currentIndex > 0) ++count
        if (ticketFilter.text.trim()) ++count
        if (minimumDuration.value > 0) ++count
        if (maximumDuration.value > 0) ++count
        return count
    }
    function advancedFilterCount() {
        return (serviceFilter.currentIndex > 0 ? 1 : 0)
            + (versionFilter.currentIndex > 0 ? 1 : 0)
            + (ticketFilter.text.trim() ? 1 : 0)
            + (minimumDuration.value > 0 ? 1 : 0)
            + (maximumDuration.value > 0 ? 1 : 0)
    }
    function clearAllFilters() {
        searchInput.clear()
        statusFilter.currentIndex = 0
        priorityFilter.currentIndex = 0
        serviceFilter.currentIndex = 0
        versionFilter.currentIndex = 0
        ticketFilter.clear()
        minimumDuration.value = 0
        maximumDuration.value = 0
        selectedTags = []
        selectedGroupPath = ""
        applyFilters()
    }

    padding: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Label { text: "事件管理"; font.pixelSize: 23; font.bold: true }
                Label { Layout.fillWidth: true; text: "按分组浏览、筛选和维护事件"; color: palette.mid }
                Label {
                    text: root.controller.issues.length + " 个结果"
                    color: palette.mid
                    font.bold: true
                }
                Button {
                    visible: root.activeFilterCount() > 0
                    objectName: "resetManagementFiltersButton"
                    text: "重置筛选"
                    flat: true
                    onClicked: root.clearAllFilters()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    objectName: "managementSearchInput"
                    placeholderText: "搜索事件、描述、进展、标签、服务、版本或问题单"
                    onTextEdited: searchDelay.restart()
                    onAccepted: root.applyFilters()
                }
                ComboBox {
                    id: statusFilter
                    model: [{value:"",label:"全部状态"}].concat(root.statusOptions)
                    textRole: "label"; valueRole: "value"
                    onActivated: root.applyFilters()
                }
                ComboBox {
                    id: priorityFilter
                    model: [{value:"",label:"全部优先级"},{value:"urgent",label:"紧急"},
                        {value:"high",label:"高"},{value:"normal",label:"普通"},{value:"low",label:"低"}]
                    textRole: "label"; valueRole: "value"
                    onActivated: root.applyFilters()
                }
                Button {
                    objectName: "advancedFiltersButton"
                    function activate() {
                        root.advancedFiltersVisible = !root.advancedFiltersVisible
                    }
                    text: "更多筛选" + (root.advancedFilterCount() > 0
                        ? " · " + root.advancedFilterCount() : "")
                        + (root.advancedFiltersVisible ? "  ▴" : "  ▾")
                    checkable: true
                    checked: root.advancedFiltersVisible
                    highlighted: root.advancedFilterCount() > 0
                    onClicked: activate()
                }
            }

            Rectangle {
                objectName: "advancedFiltersPanel"
                Layout.fillWidth: true
                Layout.preferredHeight: advancedGrid.implicitHeight + 20
                visible: root.advancedFiltersVisible
                color: palette.alternateBase
                border.color: palette.midlight
                radius: 8
                GridLayout {
                    id: advancedGrid
                    anchors.fill: parent
                    anchors.margins: 10
                    columns: width >= 760 ? 4 : 2
                    columnSpacing: 10
                    rowSpacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label { text: "服务"; color: palette.mid; font.pixelSize: 11 }
                        ComboBox {
                            id: serviceFilter
                            Layout.fillWidth: true
                            model: ["全部服务"].concat(root.controller.serviceOptions)
                            onActivated: root.applyFilters()
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label { text: "版本号"; color: palette.mid; font.pixelSize: 11 }
                        ComboBox {
                            id: versionFilter
                            Layout.fillWidth: true
                            model: ["全部版本"].concat(root.controller.versionOptions)
                            onActivated: root.applyFilters()
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label { text: "问题单"; color: palette.mid; font.pixelSize: 11 }
                        TextField {
                            id: ticketFilter
                            Layout.fillWidth: true
                            placeholderText: "编号包含…"
                            onTextEdited: searchDelay.restart()
                            onAccepted: root.applyFilters()
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label { text: "处理时长（分钟）"; color: palette.mid; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            SpinBox {
                                id: minimumDuration
                                Layout.fillWidth: true
                                from: 0; to: 100000; editable: true
                                onValueModified: searchDelay.restart()
                                ToolTip.visible: hovered; ToolTip.text: "最小时长；0 表示不限"
                            }
                            Label { text: "—"; color: palette.mid }
                            SpinBox {
                                id: maximumDuration
                                Layout.fillWidth: true
                                from: 0; to: 100000; editable: true
                                onValueModified: searchDelay.restart()
                                ToolTip.visible: hovered; ToolTip.text: "最大时长；0 表示不限"
                            }
                        }
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                visible: root.activeFilterCount() > 0
                spacing: 6
                Label {
                    text: "当前条件"
                    height: 28
                    verticalAlignment: Text.AlignVCenter
                    color: palette.mid
                }
                Button {
                    visible: searchInput.text.trim().length > 0
                    text: "搜索：" + searchInput.text.trim() + "  ×"
                    height: 28; flat: true
                    onClicked: { searchInput.clear(); root.applyFilters() }
                }
                Button {
                    visible: root.selectedGroupPath.length > 0
                    text: "分组：" + root.selectedGroupLabel() + "  ×"
                    height: 28; flat: true
                    onClicked: root.selectGroup("")
                }
                Button {
                    visible: statusFilter.currentIndex > 0
                    text: "状态：" + statusFilter.currentText + "  ×"
                    height: 28; flat: true
                    onClicked: { statusFilter.currentIndex = 0; root.applyFilters() }
                }
                Button {
                    visible: priorityFilter.currentIndex > 0
                    text: "优先级：" + priorityFilter.currentText + "  ×"
                    height: 28; flat: true
                    onClicked: { priorityFilter.currentIndex = 0; root.applyFilters() }
                }
                Button {
                    visible: serviceFilter.currentIndex > 0
                    text: "服务：" + serviceFilter.currentText + "  ×"
                    height: 28; flat: true
                    onClicked: { serviceFilter.currentIndex = 0; root.applyFilters() }
                }
                Button {
                    visible: versionFilter.currentIndex > 0
                    text: "版本：" + versionFilter.currentText + "  ×"
                    height: 28; flat: true
                    onClicked: { versionFilter.currentIndex = 0; root.applyFilters() }
                }
                Button {
                    visible: ticketFilter.text.trim().length > 0
                    text: "问题单：" + ticketFilter.text.trim() + "  ×"
                    height: 28; flat: true
                    onClicked: { ticketFilter.clear(); root.applyFilters() }
                }
                Button {
                    visible: minimumDuration.value > 0
                    text: "时长 ≥ " + minimumDuration.value + " 分钟  ×"
                    height: 28; flat: true
                    onClicked: { minimumDuration.value = 0; root.applyFilters() }
                }
                Button {
                    visible: maximumDuration.value > 0
                    text: "时长 ≤ " + maximumDuration.value + " 分钟  ×"
                    height: 28; flat: true
                    onClicked: { maximumDuration.value = 0; root.applyFilters() }
                }
                Repeater {
                    model: root.selectedTags
                    delegate: Button {
                        required property string modelData
                        text: "标签：#" + modelData + "  ×"
                        height: 28; flat: true
                        onClicked: root.toggleTag(modelData)
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.midlight }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Pane {
                SplitView.preferredWidth: 250
                SplitView.minimumWidth: 210
                SplitView.maximumWidth: 360
                padding: 10
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "分组"; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                        ToolButton { text: "展开"; onClicked: root.setAllCollapsed(false) }
                        ToolButton { text: "折叠"; onClicked: root.setAllCollapsed(true) }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "拖到目标行上方/下方排序，拖到中部设为子分组"
                        wrapMode: Text.Wrap
                        color: palette.mid
                        font.pixelSize: 11
                    }
                    ListView {
                        id: groupList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 3
                        model: root.controller.issueGroups
                        delegate: Rectangle {
                            id: groupRow
                            objectName: "groupRow"
                            required property var modelData
                            property string groupPath: modelData.path
                            property string dropPlacement: "child"
                            readonly property bool usesMovableDragProxy:
                                groupDragArea.drag.target === groupDragProxy
                            width: groupList.width
                            height: root.groupVisible(modelData.path) ? 40 : 0
                            visible: height > 0
                            radius: 7
                            color: root.selectedGroupPath === modelData.path
                                ? palette.highlight
                                : (groupDrop.containsDrag && dropPlacement === "child"
                                    ? palette.alternateBase : "transparent")
                            border.width: groupDrop.containsDrag && dropPlacement === "child" ? 1 : 0
                            border.color: palette.highlight
                            opacity: groupDragArea.drag.active ? 0.55 : 1

                            Item {
                                id: groupDragProxy
                                width: 1
                                height: 1
                                x: groupRow.width / 2
                                y: groupRow.height / 2
                                Drag.active: groupDragArea.drag.active
                                Drag.source: groupRow
                                Drag.keys: ["issuetrace-group"]
                                Drag.supportedActions: Qt.MoveAction
                            }

                            DropArea {
                                id: groupDrop
                                anchors.fill: parent
                                enabled: groupRow.modelData.path !== "__default__"
                                keys: ["issuetrace-group"]
                                onPositionChanged: function(drag) {
                                    if (!groupRow.groupPath) groupRow.dropPlacement = "root"
                                    else if (drag.y < height * 0.3) groupRow.dropPlacement = "before"
                                    else if (drag.y > height * 0.7) groupRow.dropPlacement = "after"
                                    else groupRow.dropPlacement = "child"
                                }
                                onExited: groupRow.dropPlacement = "child"
                                onDropped: function(drop) {
                                    const source = drop.source ? drop.source.groupPath : ""
                                    if (source && source !== groupRow.groupPath &&
                                            root.controller.moveIssueGroup(
                                                source, groupRow.groupPath,
                                                groupRow.dropPlacement))
                                        drop.acceptProposedAction()
                                    groupRow.dropPlacement = "child"
                                }
                            }
                            Rectangle {
                                visible: groupDrop.containsDrag &&
                                         groupRow.dropPlacement === "before"
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                height: 3; radius: 1; color: palette.highlight
                            }
                            Rectangle {
                                visible: groupDrop.containsDrag &&
                                         groupRow.dropPlacement === "after"
                                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                                height: 3; radius: 1; color: palette.highlight
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.selectGroup(groupRow.modelData.path)
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 9 + Number(groupRow.modelData.depth || 0) * 20
                                anchors.rightMargin: 9
                                spacing: 5
                                Item {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    ToolButton {
                                        anchors.centerIn: parent
                                        width: 22; height: 22
                                        visible: groupRow.modelData.hasChildren
                                        text: root.collapsedGroups[groupRow.modelData.path] ? "▸" : "▾"
                                        onClicked: root.toggleGroup(groupRow.modelData.path)
                                    }
                                }
                                Item {
                                    Layout.preferredWidth: 15
                                    Layout.preferredHeight: 22
                                    Label {
                                        anchors.centerIn: parent
                                        visible: groupRow.modelData.draggable
                                        text: "⋮⋮"
                                        color: root.selectedGroupPath === groupRow.modelData.path
                                            ? palette.highlightedText : palette.mid
                                        ToolTip.visible: groupDragArea.drag.active
                                        ToolTip.text: "拖动分组"
                                    }
                                    MouseArea {
                                        id: groupDragArea
                                        objectName: "groupDragHandle"
                                        anchors.fill: parent
                                        enabled: groupRow.modelData.draggable
                                        cursorShape: Qt.OpenHandCursor
                                        drag.target: groupDragProxy
                                        drag.axis: Drag.XAndYAxis
                                        onPressed: {
                                            groupDragProxy.x = groupRow.width / 2
                                            groupDragProxy.y = groupRow.height / 2
                                            cursorShape = Qt.ClosedHandCursor
                                        }
                                        onReleased: {
                                            cursorShape = Qt.OpenHandCursor
                                            // A successful drop refreshes the model and can destroy
                                            // this delegate synchronously, so submitting the drop must
                                            // be the final operation in this handler.
                                            groupDragProxy.Drag.drop()
                                        }
                                        onCanceled: {
                                            groupDragProxy.Drag.cancel()
                                            groupDragProxy.x = groupRow.width / 2
                                            groupDragProxy.y = groupRow.height / 2
                                            cursorShape = Qt.OpenHandCursor
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: groupRow.modelData.label
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    color: root.selectedGroupPath === groupRow.modelData.path
                                        ? palette.highlightedText : palette.text
                                }
                                Label {
                                    text: groupRow.modelData.count
                                    font.pixelSize: 11
                                    color: root.selectedGroupPath === groupRow.modelData.path
                                        ? palette.highlightedText : palette.mid
                                }
                            }
                        }
                    }
                }
            }

            Pane {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 540
                padding: 14
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 9
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            objectName: "managementEventHeading"
                            text: "事件"
                            font.pixelSize: 20
                            font.bold: true
                        }
                        Label { text: root.selectedGroupLabel(); color: palette.mid }
                        Label { text: "· " + root.controller.issues.length + " 个"; color: palette.mid }
                        Item { Layout.fillWidth: true }
                        Label { text: "排序"; color: palette.mid }
                        ComboBox {
                            id: sortFilter
                            model: [{value:"updated_desc",label:"最后修改时间"},
                                {value:"created_desc",label:"创建时间"},
                                {value:"priority_desc",label:"优先级"},
                                {value:"title_asc",label:"事件名称"},
                                {value:"tracked_desc",label:"事件处理时间"}]
                            textRole: "label"; valueRole: "value"
                            onActivated: root.applyFilters()
                        }
                    }
                    ListView {
                        id: issueList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 7
                        model: root.controller.issues
                        delegate: Rectangle {
                            id: issueCard
                            required property var modelData
                            property var issue: modelData
                            readonly property bool selected:
                                root.selectedIssueId === issue.id
                            width: issueList.width
                            height: Math.max(134, cardContent.implicitHeight + 18)
                            color: selected
                                ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                          palette.highlight.b, 0.08)
                                : cardMouse.containsMouse ? palette.alternateBase : palette.base
                            border.color: selected ? palette.highlight : palette.midlight
                            border.width: selected ? 1.5 : 1
                            radius: 10

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.margins: 5
                                width: 4
                                radius: 2
                                color: root.priorityColor(issueCard.issue.priority)
                            }
                            MouseArea {
                                id: cardMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.issueRequested(issueCard.issue.id)
                            }
                            ColumnLayout {
                                id: cardContent
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 12
                                anchors.topMargin: 8
                                anchors.bottomMargin: 8
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Label {
                                        Layout.fillWidth: true
                                        text: issueCard.issue.title
                                        font.bold: true
                                        font.pixelSize: 16
                                        elide: Text.ElideRight
                                    }
                                    Rectangle {
                                        property color tone: root.priorityColor(issueCard.issue.priority)
                                        Layout.preferredWidth: priorityText.implicitWidth + 16
                                        Layout.preferredHeight: 25
                                        radius: 12
                                        color: Qt.rgba(tone.r, tone.g, tone.b, 0.12)
                                        Label {
                                            id: priorityText
                                            anchors.centerIn: parent
                                            text: root.priorityLabel(issueCard.issue.priority)
                                            color: parent.tone
                                            font.pixelSize: 11
                                            font.bold: issueCard.issue.priority === "urgent"
                                        }
                                    }
                                    Rectangle {
                                        property color tone: root.statusColor(issueCard.issue.status)
                                        Layout.preferredWidth: statusText.implicitWidth + 18
                                        Layout.preferredHeight: 25
                                        radius: 12
                                        color: Qt.rgba(tone.r, tone.g, tone.b, 0.12)
                                        Label {
                                            id: statusText
                                            anchors.centerIn: parent
                                            text: root.statusLabel(issueCard.issue.status)
                                            color: parent.tone
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                    ToolButton {
                                        text: "编辑"
                                        Layout.preferredHeight: 28
                                        onClicked: root.editIssueRequested(issueCard.issue.id)
                                        ToolTip.visible: hovered
                                        ToolTip.text: "编辑事件信息"
                                    }
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: [
                                            {label: "分组", value: issueCard.issue.groupDisplay || "默认分组"},
                                            {label: "服务", value: issueCard.issue.service || ""},
                                            {label: "版本", value: issueCard.issue.version || ""},
                                            {label: "问题单", value: issueCard.issue.ticket || ""}
                                        ]
                                        delegate: Rectangle {
                                            required property var modelData
                                            visible: modelData.value.length > 0
                                            width: Math.min(metadataText.implicitWidth + 18,
                                                            issueList.width - 54)
                                            height: 26
                                            radius: 5
                                            color: "transparent"
                                            border.width: 1
                                            border.color: palette.midlight
                                            Label {
                                                id: metadataText
                                                anchors.fill: parent
                                                anchors.leftMargin: 9
                                                anchors.rightMargin: 9
                                                text: modelData.label + " · " + modelData.value
                                                color: palette.mid
                                                font.pixelSize: 11
                                                verticalAlignment: Text.AlignVCenter
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: root.tagsFor(issueCard.issue.tags)
                                        delegate: Rectangle {
                                            required property string modelData
                                            readonly property bool selected: root.tagSelected(modelData)
                                            width: tagText.implicitWidth + 18
                                            height: 25
                                            radius: 12
                                            color: selected
                                                ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                                          palette.highlight.b, 0.16)
                                                : "transparent"
                                            border.width: 1
                                            border.color: selected ? palette.highlight : palette.midlight
                                            Label {
                                                id: tagText
                                                anchors.centerIn: parent
                                                text: "#" + modelData
                                                color: selected ? palette.highlight : palette.mid
                                                font.pixelSize: 11
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.toggleTag(modelData)
                                            }
                                        }
                                    }
                                    Label {
                                        visible: !issueCard.issue.tags
                                        text: "暂无标签"
                                        color: palette.mid
                                        height: 25
                                        font.pixelSize: 11
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: palette.midlight
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    Label {
                                        text: "处理时长  " + root.durationLabel(
                                                  issueCard.issue.trackedTotalMilliseconds)
                                        color: palette.mid
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: "更新于  " + issueCard.issue.updatedAt
                                        color: palette.mid
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: issueList.count === 0
                            text: "当前分组或筛选条件下没有事件"
                            color: palette.mid
                        }
                    }
                }
            }
        }
    }

    Timer { id: searchDelay; interval: 250; onTriggered: root.applyFilters() }
    Connections {
        target: root.controller
        function onIssueGroupMoved(sourcePath, destinationPath) {
            if (root.selectedGroupPath === sourcePath ||
                    root.selectedGroupPath.indexOf(sourcePath + "/") === 0) {
                root.selectedGroupPath = destinationPath
                    + root.selectedGroupPath.substring(sourcePath.length)
                root.applyFilters()
            }
        }
    }
}
