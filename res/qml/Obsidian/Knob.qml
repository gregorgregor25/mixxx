import QtQuick
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls

Item {
    id: root
    required property string group
    required property string key
    property string label: ""
    property bool labelSelectable: false
    property color accent: "#c9ced2"
    property alias value: control.parameter
    signal labelClicked()
    implicitWidth: 48
    implicitHeight: 49
    Mixxx.ControlProxy { id: control; group: root.group; key: root.key }
    MixxxControls.Knob {
        id: dial
        width: Math.min(34, root.width - 10)
        height: width
        anchors.horizontalCenter: parent.horizontalCenter
        y: 2
        value: control.parameter
        angle: 135
        arc: true
        arcColor: root.accent
        arcRadius: width / 2
        arcWidth: 2
        onTurned: value => { control.parameter = value; }
        background: Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: width / 2
            border.color: "#70777e"
            gradient: Gradient {
                GradientStop { position: 0; color: "#5a6167" }
                GradientStop { position: 0.5; color: "#33393e" }
                GradientStop { position: 1; color: "#16191c" }
            }
            Rectangle {
                anchors.fill: parent; anchors.margins: 3
                radius: width / 2; color: "#292e32"; border.color: "#454c52"
            }
        }
        foreground: Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 6; height: 10; width: 2; radius: 1; color: "#ecf0f2"
        }
        TapHandler { onDoubleTapped: control.reset() }
    }
    Text {
        anchors.top: dial.bottom; anchors.topMargin: 3
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: root.label; color: "#c1c7cc"; font.pixelSize: 9; font.bold: true
        TapHandler {
            enabled: root.labelSelectable
            onTapped: root.labelClicked()
        }
    }
}
