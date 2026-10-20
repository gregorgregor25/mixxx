import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin

Rectangle {
    id: root
    color: "#202427"
    border.color: "#353a3f"
    Row {
        anchors.top: parent.top; anchors.bottom: crossfader.top
        width: parent.width
        Repeater {
            model: 2
            Item {
                id: channel
                required property int index
                readonly property string group: "[Channel" + (index + 1) + "]"
                readonly property color accent: index === 0 ? "#d8b17c" : "#8db8d7"
                width: root.width / 2; height: parent.height
                Mixxx.ControlProxy { id: volume; group: channel.group; key: "volume" }
                Mixxx.ControlProxy { id: cue; group: channel.group; key: "pfl" }
                Mixxx.ControlProxy {
                    id: filterMode
                    group: "[Flx6Filter]"
                    key: "filter_mode" + (channel.index + 1)
                }
                Text {
                    y: 4; width: 100; horizontalAlignment: Text.AlignHCenter
                    text: "CH " + (channel.index + 1); color: channel.accent
                    font.bold: true; font.pixelSize: 10
                }
                Row {
                    id: knobs
                    x: 2; y: 20
                    Column {
                        Knob { group: "[EqualizerRack1_" + channel.group + "_Effect1]"; key: "parameter3"; label: "HI" }
                        Knob { group: "[EqualizerRack1_" + channel.group + "_Effect1]"; key: "parameter2"; label: "MID" }
                        Knob { group: "[EqualizerRack1_" + channel.group + "_Effect1]"; key: "parameter1"; label: "LOW" }
                    }
                    Column {
                        Knob { group: channel.group; key: "pregain"; label: "GAIN" }
                        Knob {
                            id: filterKnob
                            group: "[QuickEffectRack1_" + channel.group + "]"
                            key: "super1"
                            label: filterMode.value >= 1.5 ? "FX ▾" :
                                    filterMode.value >= 0.5 ? "MOOG ▾" : "FILTER ▾"
                            labelSelectable: true
                            accent: channel.accent
                            onLabelClicked: filterChoice.open()
                            Controls.Popup {
                                id: filterChoice
                                x: -42; y: filterKnob.height
                                width: 132; height: 94; padding: 4
                                modal: false
                                closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
                                background: Rectangle {
                                    color: "#171b1e"; radius: 3; border.color: "#666e74"
                                }
                                contentItem: Column {
                                    spacing: 2
                                    Button {
                                        width: parent.width; height: 40
                                        text: "FLX6 filter"
                                        highlight: filterMode.value < 0.5
                                        onClicked: {
                                            filterMode.value = 0
                                            filterChoice.close()
                                        }
                                    }
                                    Button {
                                        width: parent.width; height: 40
                                        text: "Moog filter"
                                        highlight: filterMode.value >= 0.5 && filterMode.value < 1.5
                                        onClicked: {
                                            filterMode.value = 1
                                            filterChoice.close()
                                        }
                                    }
                                }
                            }
                        }
                        Knob {
                            group: "[Flx6Filter]"; key: "resonance" + (channel.index + 1)
                            label: "RES"; accent: channel.accent
                        }
                    }
                }
                Skin.VuMeter {
                    group: channel.group; key: "vu_meter_left"
                    x: 98; y: 30
                    width: 3; height: Math.max(30, channel.height - y - 22)
                }
                Skin.VuMeter {
                    group: channel.group; key: "vu_meter_right"
                    x: parent.width - 5; y: 30
                    width: 3; height: Math.max(30, channel.height - y - 22)
                }
                Controls.Slider {
                    id: channelFader
                    x: 102; y: 23
                    width: 34; height: Math.max(40, channel.height - y - 4)
                    orientation: Qt.Vertical; from: 0; to: 1; value: volume.parameter
                    onMoved: volume.parameter = value
                    background: Rectangle {
                        x: (channelFader.width - width) / 2; y: channelFader.topPadding
                        width: 5; height: channelFader.availableHeight; color: "#080a0b"; border.color: "#4b5054"
                    }
                    handle: Rectangle {
                        x: (channelFader.width - width) / 2
                        y: channelFader.topPadding + channelFader.visualPosition * (channelFader.availableHeight - height)
                        width: 28; height: 12; radius: 2; border.color: "#858b90"
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#b3b7ba" }
                            GradientStop { position: 0.5; color: "#747a7f" }
                            GradientStop { position: 1; color: "#3e4449" }
                        }
                        Rectangle { width: parent.width; height: 1; y: 5; color: "#191c1f" }
                    }
                }
            }
        }
    }
    Mixxx.ControlProxy { id: cross; group: "[Master]"; key: "crossfader" }
    Controls.Slider {
        id: crossfader
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 20; height: 24
        from: -1; to: 1; value: cross.value
        onMoved: cross.value = value
        background: Rectangle {
            x: crossfader.leftPadding; y: (crossfader.height - height) / 2
            width: crossfader.availableWidth; height: 4; color: "#07090a"; border.color: "#4a5055"
        }
        handle: Rectangle {
            x: crossfader.leftPadding + crossfader.visualPosition * (crossfader.availableWidth - width)
            y: (crossfader.height - height) / 2
            width: 17; height: 10; radius: 2; color: "#92999e"; border.color: "#c5c9cc"
        }
    }
}
