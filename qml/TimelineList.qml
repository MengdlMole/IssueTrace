import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ListView {
    id: root
    required property var controller
    required property var typeOptions

    function typeLabel(value) {
        for (let i = 0; i < typeOptions.length; ++i)
            if (typeOptions[i].value === value) return typeOptions[i].label
        return value
    }

    objectName: "detailScroll"
    Layout.fillWidth: true
    Layout.fillHeight: true
    clip: true
    spacing: 7
    reuseItems: true
    model: controller.timeline

    delegate: Frame {
        required property var modelData
        property bool editing: false
        width: root.width
        height: timelineCard.implicitHeight + 18

        ColumnLayout {
            id: timelineCard
            anchors.fill: parent
            anchors.margins: 9
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: root.typeLabel(modelData.type)
                    font.bold: true
                    color: palette.highlight
                }
                Label { text: modelData.occurredAt; color: palette.mid; Layout.fillWidth: true }
                QuietButton {
                    text: editing ? "取消" : "编辑"
                    onClicked: editing = !editing
                }
                QuietButton {
                    text: "删除"
                    onClicked: root.controller.deleteTimelineEntry(modelData.id)
                }
            }
            Label {
                visible: !editing
                Layout.fillWidth: true
                text: modelData.content
                wrapMode: Text.Wrap
                textFormat: Text.MarkdownText
            }
            TextArea {
                id: editContent
                visible: editing
                Layout.fillWidth: true
                text: modelData.content
                wrapMode: TextEdit.Wrap
                implicitHeight: Math.max(80, contentHeight + 20)
            }
            Button {
                visible: editing
                text: "保存"
                highlighted: true
                onClicked: if (root.controller.saveTimelineEntry(
                    modelData.id, modelData.type, editContent.text)) editing = false
            }
            Repeater {
                model: modelData.attachments
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Image {
                        visible: modelData.mimeType.indexOf("image/") === 0
                        source: visible ? modelData.url : ""
                        Layout.preferredWidth: visible ? 100 : 0
                        Layout.preferredHeight: visible ? 68 : 0
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideMiddle }
                    QuietButton {
                        text: "打开"
                        onClicked: root.controller.openAttachment(modelData.url)
                    }
                    QuietButton {
                        text: "删除"
                        onClicked: root.controller.deleteAttachment(modelData.id)
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                QuietButton {
                    text: "添加截图"
                    onClicked: root.controller.pasteScreenshot(modelData.id)
                }
                QuietButton {
                    text: "添加附件"
                    onClicked: {
                        attachmentDialog.timelineEntryId = modelData.id
                        attachmentDialog.open()
                    }
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: root.count === 0
        text: "还没有处理记录\n从上方随手记下第一条"
        horizontalAlignment: Text.AlignHCenter
        color: palette.mid
    }

    FileDialog {
        id: attachmentDialog
        property string timelineEntryId: ""
        title: "选择附件（最大 100 MB）"
        fileMode: FileDialog.OpenFile
        onAccepted: root.controller.attachFile(timelineEntryId, selectedFile)
    }
}
