import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var controller
    property var values: ({})
    property bool enableDescriptionImages: false
    property bool compact: false
    signal fieldEdited(string fieldId, var value)
    signal descriptionImagePasteRequested()
    signal descriptionImageDropped(url source)

    function value(fieldId) {
        const current = values && values[fieldId] !== undefined ? values[fieldId] : ""
        return current === undefined || current === null ? "" : current
    }
    function focusTitle() { titleInput.forceActiveFocus() }

    spacing: compact ? 9 : 12

    Label { text: "事件 *"; font.bold: true }
    TextArea {
        id: titleInput
        objectName: "event-form-title"
        Layout.fillWidth: true
        Layout.preferredHeight: root.compact ? 88 : 82
        text: root.value("title")
        wrapMode: TextEdit.Wrap
        placeholderText: "简明描述事件"
        onTextChanged: if (activeFocus) root.fieldEdited("title", text)
    }

    Label { text: "事件描述"; font.bold: true }
    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: root.compact ? 105 : 125
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        TextArea {
            id: descriptionInput
            objectName: "event-form-description"
            text: root.value("original_problem")
            wrapMode: TextEdit.Wrap
            placeholderText: root.enableDescriptionImages
                ? "记录原始描述；可直接粘贴截图或把图片拖到这里"
                : "记录收到事件时的原始描述（选填）"
            onTextChanged: if (activeFocus)
                root.fieldEdited("original_problem", text)
            Keys.onPressed: function(event) {
                if (root.enableDescriptionImages &&
                        event.matches(StandardKey.Paste) &&
                        root.controller.clipboardHasImage()) {
                    root.descriptionImagePasteRequested()
                    event.accepted = true
                }
            }
            DropArea {
                anchors.fill: parent
                enabled: root.enableDescriptionImages
                onDropped: function(drop) {
                    if (drop.hasUrls && drop.urls.length > 0)
                        root.descriptionImageDropped(drop.urls[0])
                }
            }
        }
    }

    EventMetadataFields {
        Layout.fillWidth: true
        controller: root.controller
        values: root.values
        compact: root.compact
        onFieldEdited: (fieldId, value) => root.fieldEdited(fieldId, value)
    }
}
