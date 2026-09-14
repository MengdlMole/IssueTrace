import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root
    required property var controller
    property string selectedIssueId: ""
    required property var statusOptions
    signal issueRequested(string issueId)
    signal filterRequested(string text, string status)

    function statusLabel(value) {
        for (let i = 0; i < statusOptions.length; ++i)
            if (statusOptions[i].value === value) return statusOptions[i].label
        return value
    }

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
                Label {
                    text: "问题收件箱"
                    font.pixelSize: 23
                    font.bold: true
                    Layout.fillWidth: true
                }
                Label { text: root.controller.issues.length + " 项"; color: palette.mid }
            }
            TextField {
                id: searchInput
                Layout.fillWidth: true
                placeholderText: "搜索问题和处理记录"
                onTextEdited: searchDelay.restart()
                onAccepted: root.filterRequested(text, statusFilter.currentValue)
            }
            ComboBox {
                id: statusFilter
                Layout.fillWidth: true
                model: [{ value: "", label: "全部状态" }].concat(root.statusOptions)
                textRole: "label"
                valueRole: "value"
                onActivated: root.filterRequested(searchInput.text, currentValue)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: palette.midlight
        }
        Label {
            visible: root.controller.attentionIssues.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.topMargin: 10
            text: "需要关注 · " + root.controller.attentionIssues.length
            font.bold: true
            color: "#b42318"
        }
        ListView {
            id: attentionList
            visible: count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 190)
            clip: true
            model: root.controller.attentionIssues
            delegate: ItemDelegate {
                required property var modelData
                width: attentionList.width
                height: 66
                onClicked: root.issueRequested(modelData.id)
                contentItem: ColumnLayout {
                    Label {
                        Layout.fillWidth: true
                        text: "!  " + modelData.title
                        font.bold: true
                        elide: Text.ElideRight
                        color: "#b42318"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.reminderDue ? "提醒已到期" : "已 " + modelData.inactiveText + " 未更新"
                        color: palette.mid
                        font.pixelSize: 12
                    }
                }
            }
        }
        Label {
            Layout.leftMargin: 14
            Layout.topMargin: 8
            text: "全部问题"
            font.bold: true
        }
        ListView {
            id: issueList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 1
            model: root.controller.issues
            delegate: ItemDelegate {
                required property var modelData
                width: issueList.width
                height: 82
                highlighted: root.selectedIssueId === modelData.id
                onClicked: root.issueRequested(modelData.id)
                contentItem: ColumnLayout {
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
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.progress || "尚无进展"
                        color: palette.mid
                        elide: Text.ElideRight
                    }
                    Label {
                        text: "当前状态 " + modelData.statusDurationText + " · " + modelData.inactiveText + " 未更新"
                        color: palette.mid
                        font.pixelSize: 11
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: issueList.count === 0
                text: "还没有问题\n按 Ctrl/Cmd + N 快速记录"
                horizontalAlignment: Text.AlignHCenter
                color: palette.mid
            }
        }
    }

    Timer {
        id: searchDelay
        interval: 250
        onTriggered: root.filterRequested(searchInput.text, statusFilter.currentValue)
    }
}
