import QtQuick
import QtQuick.Controls as Controls

Controls.Button {
    id: root
    property color accent: "#d8b17c"
    property bool highlight: false
    property color borderColor: root.activeFocus || root.highlight || root.checked ? root.accent : "#4b5055"
    property real borderWidth: 1
    implicitWidth: Math.max(38, contentItem.implicitWidth + 18)
    implicitHeight: 34
    padding: 4
    font.pixelSize: 11
    font.bold: true
    opacity: enabled ? 1 : 0.4
    contentItem: Text {
        text: root.text
        font: root.font
        color: root.highlight || root.checked ? root.accent : "#e3e5e7"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 8
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 3
        border.color: root.borderColor
        border.width: root.borderWidth
        gradient: Gradient {
            GradientStop { position: 0; color: root.down ? "#171a1d" : "#3c4147" }
            GradientStop { position: 1; color: root.down ? "#262a2e" : "#22262a" }
        }
        Rectangle {
            x: 1; y: 1; width: parent.width - 2; height: 1
            color: root.down ? "#101214" : "#687078"
            opacity: 0.6
        }
    }
}
