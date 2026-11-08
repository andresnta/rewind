import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The buttons on a list row. Every one has the same size and shape, so
// rows line up whatever they offer; a vivid one is filled with the accent.
Button {
    id: control
    property bool vivid: false
    // 0..1 fills the button with the accent from the left, as a progress
    // bar; -1 leaves it plain.
    property real fill: -1

    implicitWidth: 84
    implicitHeight: 32
    // Material pads and insets its buttons generously; these are compact
    // and fixed-size, so the label gets the whole button.
    padding: 0
    leftPadding: 6
    rightPadding: 6
    topPadding: 0
    bottomPadding: 0
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    font.pixelSize: 13
    font.weight: Font.Medium

    contentItem: Label {
        text: control.text
        font: control.font
        color: control.vivid ? theme.accentForeground : "#e6e6ea"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Math.min(4, theme.radius)
        clip: true
        color: control.vivid
               ? (control.down ? Qt.darker(theme.accent, 1.15) : control.hovered ? Qt.lighter(theme.accent, 1.08) : theme.accent)
               : Qt.rgba(1, 1, 1, control.down ? 0.16 : control.hovered ? 0.12 : 0.07)
        Rectangle {
            visible: control.fill >= 0
            width: parent.width * Math.max(0, Math.min(1, control.fill))
            height: parent.height
            radius: parent.radius
            color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.45)
            Behavior on width { NumberAnimation { duration: 250 } }
        }
    }
}
