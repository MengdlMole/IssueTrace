import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "trashDialog"
    required property var controller
    signal permanentIssueRequested(string issueId, string title)
    signal permanentTimelineRequested(string entryId, string preview)

    title: "回收站"
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 60, 820)
    height: Math.min(parent.height - 60, 620)
    closePolicy: Popup.CloseOnEscape
    onOpened: controller.refreshTrash()

    footer: DialogButtonBox {
        Button { text: "关闭"; onClicked: root.close() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: "恢复不会丢失原有记录和附件；永久删除后无法恢复。"
            color: palette.mid
            wrapMode: Text.Wrap
        }

        TabBar {
            id: trashTabs
            Layout.fillWidth: true
            TabButton { text: "事件（" + root.controller.trashIssues.length + "）" }
            TabButton { text: "事件记录（" + root.controller.trashTimelineEntries.length + "）" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: trashTabs.currentIndex

            Item {
                ListView {
                    id: deletedIssueList
                    objectName: "deletedIssueList"
                    anchors.fill: parent
                    clip: true
                    spacing: 7
                    reuseItems: true
                    model: root.controller.trashIssues
                    delegate: Frame {
                        required property var modelData
                        width: deletedIssueList.width
                        height: issueRow.implicitHeight + 18
                        ColumnLayout {
                            id: issueRow
                            anchors.fill: parent
                            anchors.margins: 9
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                QuietButton {
                                    objectName: "restoreDeletedIssueButton"
                                    text: "恢复"
                                    onClicked: root.controller.restoreIssue(modelData.id)
                                }
                                QuietButton {
                                    objectName: "purgeDeletedIssueButton"
                                    text: "永久删除"
                                    onClicked: root.permanentIssueRequested(
                                        modelData.id, modelData.title)
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "删除于 " + (modelData.deletedAt || "—")
                                    + (modelData.groupDisplay
                                       ? " · " + modelData.groupDisplay : "")
                                color: palette.mid
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: deletedIssueList.count === 0
                    text: "没有已删除事件"
                    color: palette.mid
                }
            }

            Item {
                ListView {
                    id: deletedTimelineList
                    objectName: "deletedTimelineList"
                    anchors.fill: parent
                    clip: true
                    spacing: 7
                    reuseItems: true
                    model: root.controller.trashTimelineEntries
                    delegate: Frame {
                        required property var modelData
                        width: deletedTimelineList.width
                        height: timelineRow.implicitHeight + 18
                        ColumnLayout {
                            id: timelineRow
                            anchors.fill: parent
                            anchors.margins: 9
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.issueTitle || "未知事件"
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                QuietButton {
                                    objectName: "restoreDeletedTimelineButton"
                                    text: "恢复"
                                    onClicked: root.controller.restoreTimelineEntry(modelData.id)
                                }
                                QuietButton {
                                    objectName: "purgeDeletedTimelineButton"
                                    text: "永久删除"
                                    onClicked: root.permanentTimelineRequested(
                                        modelData.id, modelData.content)
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.content
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "记录于 " + modelData.occurredAt
                                    + " · 删除于 " + modelData.deletedAt
                                color: palette.mid
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: deletedTimelineList.count === 0
                    text: "没有已删除事件记录"
                    color: palette.mid
                }
            }
        }
    }
}
