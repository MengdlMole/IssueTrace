import QtQuick
import QtQuick.Controls

Button {
    id: control
    flat: true
    leftPadding: 10
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5

    background: Rectangle {
        radius: 6
        color: control.down
            ? control.palette.midlight
            : control.hovered ? control.palette.alternateBase : "transparent"
        border.width: 1
        border.color: control.hovered || control.activeFocus
            ? control.palette.mid : control.palette.midlight
        opacity: control.enabled ? 1 : 0.5
    }
}
