import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../Theme"

ColumnLayout {
    id: root

    spacing: 12

    Mixxx.ControlProxy {
        id: available

        group: "[Flx6Filter]"
        key: "available"
    }
    Mixxx.ControlProxy {
        id: mode

        group: "[Flx6Filter]"
        key: "enabled"
    }
    Mixxx.ControlProxy {
        id: resonance1

        group: "[Flx6Filter]"
        key: "resonance1"
    }
    Mixxx.ControlProxy {
        id: resonance2

        group: "[Flx6Filter]"
        key: "resonance2"
    }
    Mixxx.ControlProxy {
        id: active1

        group: "[Flx6Filter]"
        key: "active1"
    }
    Mixxx.ControlProxy {
        id: active2

        group: "[Flx6Filter]"
        key: "active2"
    }
    Mixxx.ControlProxy {
        id: saveError

        group: "[Flx6Filter]"
        key: "save_error"
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 16

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 20
            text: qsTr("FLX6 two-deck filters")
            wrapMode: Text.WordWrap
        }
        FormButton {
            Accessible.name: qsTr("Two-deck filter mode")
            Layout.preferredHeight: 48
            Layout.preferredWidth: 170
            enabled: available.value > 0
            highlight: mode.value > 0
            text: mode.value > 0 ? qsTr("Enabled") : qsTr("Disabled")

            onClicked: mode.value = mode.value > 0 ? 0 : 1
        }
    }
    Text {
        Layout.fillWidth: true
        color: Theme.lightGray2
        font.pixelSize: 14
        text: mode.value > 0 ? qsTr("Channel 1 and 2 FILTER knobs keep their normal sweep. Channel 3 and 4 FILTER knobs control resonance below.") : qsTr("Enable to use channel 3 and 4 FILTER knobs for channel 1 and 2 resonance. Disabled keeps their normal four-deck behavior.")
        wrapMode: Text.WordWrap
    }
    ResonanceRow {
        Layout.fillWidth: true
        active: active1.value > 0
        control: resonance1
        label: qsTr("Channel 1 — use CH3 FILTER")
    }
    ResonanceRow {
        Layout.fillWidth: true
        active: active2.value > 0
        control: resonance2
        label: qsTr("Channel 2 — use CH4 FILTER")
    }
    Text {
        Layout.fillWidth: true
        color: Theme.lightGray2
        font.pixelSize: 14
        text: saveError.value > 0 ? qsTr("Settings could not be saved. Check available storage, then adjust a value to retry.") : qsTr("Settings save automatically. After startup or a touchscreen change, turn the physical knob through the saved value to pick it up. Higher resonance emphasizes frequencies around the cutoff.")
        wrapMode: Text.WordWrap
    }
    Text {
        Layout.fillWidth: true
        color: Theme.lightGray2
        font.pixelSize: 14
        text: qsTr("Resonance pauses on a channel while an unrelated Quick Effect is selected. Choose FLX6 Resonant Filter or Moog Filter to resume.")
        visible: mode.value > 0 && (active1.value < 1 || active2.value < 1)
        wrapMode: Text.WordWrap
    }

    component ResonanceRow: ColumnLayout {
        id: row

        required property bool active
        required property var control
        required property string label

        spacing: 4

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.pixelSize: 16
                text: row.label
                wrapMode: Text.WordWrap
            }
            Text {
                color: Theme.white
                font.pixelSize: 16
                text: qsTr("%1%").arg(Math.round(row.control.value * 100))
            }
        }
        RowLayout {
            Layout.fillWidth: true
            enabled: row.active
            opacity: enabled ? 1 : 0.5
            spacing: 12

            FormButton {
                Accessible.name: qsTr("Decrease resonance: %1").arg(row.label)
                Layout.preferredHeight: 48
                Layout.preferredWidth: 48
                text: "−"

                onClicked: row.control.value = Math.max(0, row.control.value - 0.01)
            }
            Text {
                color: Theme.white
                font.pixelSize: 14
                text: qsTr("Mild")
            }
            Slider {
                id: slider

                Accessible.name: row.label
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                from: 0
                stepSize: 0.001
                to: 1

                Component.onCompleted: value = row.control.value
                onMoved: row.control.value = value
            }
            Text {
                color: Theme.white
                font.pixelSize: 14
                text: qsTr("Wild")
            }
            FormButton {
                Accessible.name: qsTr("Increase resonance: %1").arg(row.label)
                Layout.preferredHeight: 48
                Layout.preferredWidth: 48
                text: "+"

                onClicked: row.control.value = Math.min(1, row.control.value + 0.01)
            }
        }
        Connections {
            function onValueChanged() {
                if (!slider.pressed) {
                    slider.value = row.control.value;
                }
            }

            target: row.control
        }
    }
}
