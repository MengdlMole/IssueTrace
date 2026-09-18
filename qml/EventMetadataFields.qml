import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: root
    required property var controller
    property var values: ({})
    property bool showReporter: true
    property bool showTicket: true
    property bool compact: false
    signal fieldEdited(string fieldId, var value)

    function value(fieldId, fallback) {
        const current = values && values[fieldId] !== undefined ? values[fieldId] : fallback
        return current === undefined || current === null ? "" : current
    }

    columns: width >= 620 ? 2 : 1
    columnSpacing: 14
    rowSpacing: compact ? 7 : 10

    component HistoryField: ColumnLayout {
        required property string fieldId
        required property string fieldLabel
        required property var choices
        property string hint: "可选择历史值，也可输入新值"
        property string currentValue: root.value(fieldId, "")
        Layout.fillWidth: true
        spacing: 4
        onCurrentValueChanged: {
            if (!historyInput.activeFocus && historyInput.editText !== currentValue)
                historyInput.editText = currentValue
        }
        Label { text: parent.fieldLabel; font.bold: true }
        ComboBox {
            id: historyInput
            objectName: "metadata-" + parent.fieldId
            Layout.fillWidth: true
            editable: true
            model: parent.choices
            Component.onCompleted: editText = parent.currentValue
            onEditTextChanged: if (activeFocus)
                root.fieldEdited(parent.fieldId, editText)
            onAccepted: root.fieldEdited(parent.fieldId, editText)
            onActivated: {
                const selectedText = currentText
                editText = selectedText
                root.fieldEdited(parent.fieldId, selectedText)
            }
            ToolTip.visible: hovered
            ToolTip.text: parent.hint
        }
    }

    HistoryField {
        visible: root.showReporter
        fieldId: "reporter"
        fieldLabel: "提出人"
        choices: root.controller.reporterOptions
    }

    ColumnLayout {
        id: priorityField
        property string currentValue: root.value("priority", "normal")
        function optionIndex() {
            for (let i = 0; i < priorityInput.model.length; ++i)
                if (priorityInput.model[i].value === currentValue) return i
            return 2
        }
        Layout.fillWidth: true
        spacing: 4
        onCurrentValueChanged: priorityInput.currentIndex = optionIndex()
        Label { text: "优先级"; font.bold: true }
        ComboBox {
            id: priorityInput
            objectName: "metadata-priority"
            Layout.fillWidth: true
            model: [{value:"urgent",label:"紧急"},{value:"high",label:"高"},
                    {value:"normal",label:"普通"},{value:"low",label:"低"}]
            textRole: "label"
            valueRole: "value"
            Component.onCompleted: currentIndex = priorityField.optionIndex()
            onActivated: root.fieldEdited("priority", currentValue)
        }
    }

    HistoryField {
        fieldId: "assignee"
        fieldLabel: "处理人"
        choices: root.controller.assigneeOptions
    }

    HistoryField {
        fieldId: "group_name"
        fieldLabel: "分组"
        choices: root.controller.groupOptions
        hint: "可选择历史分组；多级分组使用 / 分隔"
    }

    HistoryField {
        fieldId: "version"
        fieldLabel: "版本号"
        choices: root.controller.versionOptions
    }

    ColumnLayout {
        id: tagsField
        property string currentValue: root.value("tags", "")
        Layout.fillWidth: true
        spacing: 4
        onCurrentValueChanged: {
            if (!tagsInput.activeFocus && tagsInput.text !== currentValue)
                tagsInput.text = currentValue
        }
        Label { text: "标签"; font.bold: true }
        TextField {
            id: tagsInput
            objectName: "metadata-tags"
            Layout.fillWidth: true
            Component.onCompleted: text = tagsField.currentValue
            placeholderText: "多个标签使用英文逗号分隔"
            onTextEdited: root.fieldEdited("tags", text)
        }
    }

    HistoryField {
        fieldId: "service"
        fieldLabel: "服务"
        choices: root.controller.serviceOptions
    }

    ColumnLayout {
        id: ticketField
        property string currentValue: root.value("ticket", "")
        visible: root.showTicket
        Layout.fillWidth: true
        spacing: 4
        onCurrentValueChanged: {
            if (!ticketInput.activeFocus && ticketInput.text !== currentValue)
                ticketInput.text = currentValue
        }
        Label { text: "问题单"; font.bold: true }
        TextField {
            id: ticketInput
            objectName: "metadata-ticket"
            Layout.fillWidth: true
            Component.onCompleted: text = ticketField.currentValue
            placeholderText: "选填"
            onTextEdited: root.fieldEdited("ticket", text)
        }
    }
}
