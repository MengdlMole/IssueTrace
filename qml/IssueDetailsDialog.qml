import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property var controller
    property var draftIssue: ({})
    property bool dirty: false

    function optionIndex(options, value) {
        for (let i = 0; i < options.length; ++i)
            if (options[i].value === value) return i
        return 0
    }
    function setDraftField(id, value) {
        const updated = Object.assign({}, draftIssue)
        updated[id] = value
        draftIssue = updated
        dirty = true
        saveTimer.restart()
    }
    function reload() {
        draftIssue = Object.assign({}, controller.selectedIssue)
        dirty = false
    }
    function flush() {
        if (!dirty || controller.selectedIssue.id === undefined) return true
        if (!controller.saveIssue(draftIssue)) return false
        dirty = false
        return true
    }

    title: "更多信息"
    modal: true
    anchors.centerIn: parent
    width: 650
    height: Math.min(parent.height - 70, 650)
    closePolicy: Popup.NoAutoClose
    onOpened: reload()

    Shortcut {
        sequence: StandardKey.Cancel
        enabled: root.opened
        onActivated: if (root.flush()) root.close()
    }

    footer: DialogButtonBox {
        Button {
            text: "关闭"
            onClicked: if (root.flush()) root.close()
        }
    }

    ScrollView {
        anchors.fill: parent
        ColumnLayout {
            width: root.availableWidth
            Repeater {
                model: root.controller.formFields
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    visible: modelData.id !== "status" && modelData.id !== "priority"
                    Label { text: modelData.label }
                    Loader {
                        Layout.fillWidth: true
                        property var definition: modelData
                        sourceComponent: definition.readOnly ? readOnlyEditor
                            : definition.type === "select" ? selectEditor
                            : definition.type === "multiline_text" ? multilineEditor
                            : textEditor
                        onLoaded: item.field = definition
                    }
                }
            }
        }
    }

    Timer {
        id: saveTimer
        interval: 800
        onTriggered: if (root.dirty && root.controller.saveIssue(root.draftIssue)) root.dirty = false
    }
    Connections {
        target: root.controller
        function onSelectedIssueChanged() { root.reload() }
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
        Label {
            property var field
            text: field ? (root.draftIssue[field.id] || "—") : "—"
            color: palette.mid
            padding: 6
        }
    }
}
