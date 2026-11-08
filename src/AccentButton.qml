import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The one main action on screen, filled with the theme accent. It draws its
// own background so the fill never depends on Material's shader-drawn one.
Button {
    id: control
    Material.roundedScale: theme.radius
    contentItem: Label {
        text: control.text
        font: control.font
        color: control.enabled ? theme.accentForeground : "#6b6b73"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        implicitWidth: 72
        implicitHeight: 40
        radius: Math.min(6, theme.radius)
        color: !control.enabled ? Qt.rgba(1, 1, 1, 0.08)
             : control.down ? Qt.darker(theme.accent, 1.15)
             : control.hovered ? Qt.lighter(theme.accent, 1.08) : theme.accent
    }
}
