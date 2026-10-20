pragma ComponentBehavior: Bound

import ".." as Skin
import "." as Studio
import "../Effects" as Effects
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property int unitNumber
    required property color accent
    readonly property Mixxx.EffectUnitProxy unit: Mixxx.EffectsManager.getEffectUnit(unitNumber)
    readonly property string group: unit.group
    readonly property Mixxx.EffectSlotProxy slot1: Mixxx.EffectsManager.getEffectSlot(unitNumber, 1)
    readonly property Mixxx.EffectSlotProxy slot2: Mixxx.EffectsManager.getEffectSlot(unitNumber, 2)
    readonly property Mixxx.EffectSlotProxy slot3: Mixxx.EffectsManager.getEffectSlot(unitNumber, 3)

    implicitHeight: 76

    Rectangle {
        anchors.fill: parent
        color: "#1b2026"
        border.color: "#343d45"
        border.width: 1
    }

    RowLayout {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        height: 34
        spacing: 5

        Text {
            text: "FX " + root.unitNumber
            color: root.accent
            font.pixelSize: 12
            font.weight: Font.Bold
            Layout.preferredWidth: 34
        }
        Text {
            text: "ASSIGN"
            color: "#a3afb9"
            font.pixelSize: 9
            font.weight: Font.DemiBold
            Layout.preferredWidth: 39
        }
        Studio.ControlButton {
            group: root.group
            key: "group_[Channel1]_enable"
            text: "A"
            toggleable: true
            accent: root.accent
            Layout.preferredWidth: 29
            Layout.preferredHeight: 28
        }
        Studio.ControlButton {
            group: root.group
            key: "group_[Channel2]_enable"
            text: "B"
            toggleable: true
            accent: root.accent
            Layout.preferredWidth: 29
            Layout.preferredHeight: 28
        }
        Studio.Button {
            text: root.unit.presetName || "FX PRESET"
            accent: root.accent
            Layout.fillWidth: true
            Layout.minimumWidth: 80
            Layout.preferredHeight: 28
            onClicked: presetPopup.open()
        }
        Text {
            text: "D/W " + Math.round(mixControl.parameter * 100) + "%"
            color: "#c9d2d9"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            Layout.preferredWidth: 55
        }
        Text {
            text: "MAC " + Math.round(macroControl.parameter * 100) + "%"
            color: "#c9d2d9"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            Layout.preferredWidth: 60
        }
        Studio.Button {
            text: detailPopup.opened ? "−" : "+"
            accent: root.accent
            Layout.preferredWidth: 30
            Layout.preferredHeight: 28
            onClicked: detailPopup.opened ? detailPopup.close() : detailPopup.open()
        }
    }

    RowLayout {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 5
        spacing: 5

        FxSlot {
            Layout.fillWidth: true
            Layout.fillHeight: true
            unitNumber: root.unitNumber
            effectNumber: 1
            accent: root.accent
        }
        FxSlot {
            Layout.fillWidth: true
            Layout.fillHeight: true
            unitNumber: root.unitNumber
            effectNumber: 2
            accent: root.accent
        }
        FxSlot {
            Layout.fillWidth: true
            Layout.fillHeight: true
            unitNumber: root.unitNumber
            effectNumber: 3
            accent: root.accent
        }
    }

    Mixxx.ControlProxy {
        id: mixControl
        group: root.group
        key: "mix"
    }
    Mixxx.ControlProxy {
        id: macroControl
        group: root.group
        key: "super1"
    }

    Effects.EffectChainPresetPopup {
        id: presetPopup
        unit: root.unit
        slot1: root.slot1
        slot2: root.slot2
        slot3: root.slot3
    }

    Popup {
        id: detailPopup
        y: root.height - 1
        width: root.width
        height: 245
        padding: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1c2329"
            border.color: "#596771"
            radius: 4
        }

        contentItem: Column {
            spacing: 5

            Row {
                spacing: 8
                height: 42

                Text {
                    text: "FX " + root.unitNumber + "   CHAIN"
                    color: root.accent
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                    width: 86
                }
                Skin.ControlButton {
                    group: root.group
                    key: "mix_mode"
                    text: "D/W MODE"
                    toggleable: true
                    activeColor: root.accent
                    width: 84
                    height: 32
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "DRY / WET"
                    color: "#c9d2d9"
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
                Studio.Knob {
                    group: root.group
                    key: "mix"
                    label: "D/W"
                    accent: root.accent
                    width: 40
                    height: 40
                }
                Text {
                    text: "MACRO"
                    color: "#c9d2d9"
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
                Studio.Knob {
                    group: root.group
                    key: "super1"
                    label: "MAC"
                    accent: root.accent
                    width: 40
                    height: 40
                }
            }
            Effects.EffectSlot {
                unitNumber: root.unitNumber
                effectNumber: 1
                expanded: true
                width: detailPopup.availableWidth
                height: 54
            }
            Effects.EffectSlot {
                unitNumber: root.unitNumber
                effectNumber: 2
                expanded: true
                width: detailPopup.availableWidth
                height: 54
            }
            Effects.EffectSlot {
                unitNumber: root.unitNumber
                effectNumber: 3
                expanded: true
                width: detailPopup.availableWidth
                height: 54
            }
        }
    }
}
