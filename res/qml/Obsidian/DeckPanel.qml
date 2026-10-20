import "." as Obsidian
import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC
import QtQuick.Layouts

Rectangle {
    id: root

    required property string group
    required property color accent
    required property string letter
    readonly property bool loaded: trackLoaded.value > 0
    readonly property var visibleLoopSizes: [0.5, 1, 2, 4, 8, 16]
    readonly property var extraLoopSizes: [
        { size: 0.03125, label: "1/32" },
        { size: 0.0625, label: "1/16" },
        { size: 0.125, label: "1/8" },
        { size: 0.25, label: "1/4" },
        { size: 32, label: "32" },
        { size: 64, label: "64" },
        { size: 128, label: "128" },
        { size: 256, label: "256" },
        { size: 512, label: "512" }
    ]

    color: "#191c1f"
    border.width: 1
    border.color: "#363c3e"

    Mixxx.ControlProxy { id: trackLoaded; group: root.group; key: "track_loaded" }
    Mixxx.ControlProxy { id: syncEnabled; group: root.group; key: "sync_enabled" }
    Mixxx.ControlProxy { id: syncMode; group: root.group; key: "sync_mode" }
    Mixxx.ControlProxy { id: syncLeader; group: root.group; key: "sync_leader" }
    Mixxx.ControlProxy { id: loopEnabled; group: root.group; key: "loop_enabled" }
    Mixxx.ControlProxy { id: loopSize; group: root.group; key: "beatloop_size" }
    Mixxx.ControlProxy { id: beatjumpSize; group: root.group; key: "beatjump_size" }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 43
            Layout.minimumHeight: 43
            Layout.maximumHeight: 43
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            spacing: 5

            Obsidian.ControlButton {
                Layout.preferredWidth: 47
                Layout.preferredHeight: 34
                group: root.group
                key: "play"
                toggleable: root.loaded
                text: "PLAY"
                accent: root.accent
                enabled: root.loaded
            }

            Obsidian.ControlButton {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 34
                group: root.group
                key: "cue_default"
                text: "CUE"
                accent: root.accent
                enabled: root.loaded
            }

            Obsidian.Button {
                property bool heldForLeader: false
                Layout.preferredWidth: 63
                Layout.preferredHeight: 34
                text: syncMode.value >= 2 ? "LEADER" :
                      syncMode.value === 1 ? "FOLLOW" : "SYNC"
                accent: root.accent
                checkable: false
                checked: syncEnabled.value > 0
                enabled: root.loaded
                onPressed: heldForLeader = false
                onClicked: {
                    if (!heldForLeader)
                        syncEnabled.toggle();
                }
                onPressAndHold: {
                    heldForLeader = true;
                    syncLeader.toggle();
                }
            }

            Text {
                Layout.fillWidth: true
                text: syncMode.value >= 2 ? "TEMPO SOURCE" :
                      syncMode.value === 1 ? "SYNCED TO LEADER" : "SYNC OFF"
                color: syncMode.value > 0 ? root.accent : "#9ea6a8"
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            Layout.minimumHeight: 38
            Layout.maximumHeight: 38
            color: "#202427"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 3

                Text {
                    text: "HOTCUES"
                    color: "#b7c0c1"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    visible: root.width > 480
                }

                Repeater {
                    model: 8

                    Obsidian.Button {
                        required property int index

                        readonly property int number: index + 1
                        readonly property color cueColor: colorControl.value < 0 ?
                                root.accent :
                                "#" + Math.round(colorControl.value).toString(16).padStart(6, "0")
                        Layout.fillWidth: true
                        Layout.minimumWidth: 23
                        Layout.preferredHeight: 29
                        text: number.toString()
                        accent: cueColor
                        checked: status.value > 0
                        enabled: root.loaded
                        onPressed: {
                            if (clearButton.checked) {
                                clearControl.trigger();
                                clearButton.checked = false;
                            } else {
                                activateControl.value = 1;
                            }
                        }
                        onReleased: activateControl.value = 0
                        onCanceled: activateControl.value = 0

                        Mixxx.ControlProxy {
                            id: status
                            group: root.group
                            key: "hotcue_" + number + "_status"
                        }
                        Mixxx.ControlProxy {
                            id: colorControl
                            group: root.group
                            key: "hotcue_" + number + "_color"
                        }
                        Mixxx.ControlProxy {
                            id: activateControl
                            group: root.group
                            key: "hotcue_" + number + "_activate"
                        }
                        Mixxx.ControlProxy {
                            id: clearControl
                            group: root.group
                            key: "hotcue_" + number + "_clear"
                        }
                    }
                }

                Obsidian.Button {
                    id: clearButton
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 29
                    text: "CLEAR"
                    accent: root.accent
                    checkable: true
                    enabled: root.loaded
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            Layout.minimumHeight: 42
            Layout.maximumHeight: 42
            color: "#171a1c"
            border.color: "#303638"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 3

                Text {
                    text: "LOOP"
                    color: "#b7c0c1"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }

                Repeater {
                    model: root.visibleLoopSizes

                    Obsidian.Button {
                        required property var modelData

                        readonly property real size: modelData
                        Layout.fillWidth: true
                        Layout.minimumWidth: 30
                        Layout.preferredHeight: 30
                        text: size === 0.5 ? "1/2" : size.toString()
                        accent: root.accent
                        checked: root.loaded &&
                                Math.abs(loopSize.value - size) < 0.00001
                        enabled: root.loaded
                        onClicked: {
                            if (loopEnabled.value > 0)
                                loopSize.value = size;
                            else
                                activateLoop.trigger();
                        }

                        Mixxx.ControlProxy {
                            id: activateLoop
                            group: root.group
                            key: "beatloop_" + size + "_activate"
                        }
                    }
                }

                Text {
                    text: loopSize.value > 0 ? loopSize.value + "b" : ""
                    color: "#acb6b8"
                    font.pixelSize: 10
                    visible: root.width > 470
                }

                Obsidian.Button {
                    Layout.preferredWidth: 39
                    Layout.preferredHeight: 30
                    text: "MORE"
                    accent: root.accent
                    enabled: root.loaded
                    onClicked: moreLoops.open()
                }

                Obsidian.ControlButton {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 30
                    group: root.group
                    key: "reloop_toggle"
                    text: "OFF"
                    accent: root.accent
                    enabled: root.loaded && loopEnabled.value > 0
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            Layout.minimumHeight: 28
            Layout.maximumHeight: 28

            Skin.WaveformOverview {
                anchors.fill: parent
                group: root.group
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 49
            Layout.minimumHeight: 49
            Layout.maximumHeight: 49
            color: "#1d2124"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 5

                Obsidian.ControlButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 46
                    Layout.preferredHeight: 33
                    group: root.group
                    key: "keylock"
                    toggleable: true
                    text: "KEYLOCK"
                    accent: root.accent
                    enabled: root.loaded
                }
                Obsidian.ControlButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 42
                    Layout.preferredHeight: 33
                    group: root.group
                    key: "quantize"
                    toggleable: true
                    text: "QUANT"
                    accent: root.accent
                    enabled: root.loaded
                }
                Obsidian.ControlButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 37
                    Layout.preferredHeight: 33
                    group: root.group
                    key: "slip_enabled"
                    toggleable: true
                    text: "SLIP"
                    accent: root.accent
                    enabled: root.loaded
                }

                Obsidian.ControlButton {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 33
                    group: root.group
                    key: "beatjump_backward"
                    text: "BACK"
                    accent: root.accent
                    enabled: root.loaded
                }

                QQC.ComboBox {
                    id: jumpSizePicker
                    Layout.preferredWidth: 78
                    Layout.preferredHeight: 33
                    model: [0.25, 0.5, 1, 2, 4, 8, 16, 32, 64]
                    currentIndex: [0.25, 0.5, 1, 2, 4, 8, 16, 32, 64].indexOf(beatjumpSize.value)
                    enabled: root.loaded
                    onActivated: function(index) {
                        beatjumpSize.value = model[index];
                    }

                    contentItem: Text {
                        text: jumpSizePicker.currentText + " BEATS"
                        color: "#e1e6e5"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 10
                    }
                    background: Rectangle {
                        color: "#24292b"
                        border.color: "#596163"
                        radius: 2
                    }
                }

                Obsidian.ControlButton {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 33
                    group: root.group
                    key: "beatjump_forward"
                    text: "FWD"
                    accent: root.accent
                    enabled: root.loaded
                }
            }
        }
    }

    QQC.Popup {
        id: moreLoops
        x: Math.max(4, root.width - width - 8)
        y: 121
        width: 204
        padding: 8
        modal: false
        closePolicy: QQC.Popup.CloseOnEscape | QQC.Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#202427"
            border.color: "#737b7d"
            radius: 3
        }

        contentItem: GridLayout {
            columns: 3
            columnSpacing: 4
            rowSpacing: 4

            Repeater {
                model: root.extraLoopSizes

                Obsidian.Button {
                    required property var modelData
                    readonly property real size: modelData.size

                    Layout.preferredWidth: 59
                    Layout.preferredHeight: 32
                    text: modelData.label
                    accent: root.accent
                    checked: Math.abs(loopSize.value - size) < 0.00001
                    onClicked: {
                        if (loopEnabled.value > 0)
                            loopSize.value = size;
                        else
                            activateExtraLoop.trigger();
                        moreLoops.close();
                    }

                    Mixxx.ControlProxy {
                        id: activateExtraLoop
                        group: root.group
                        key: "beatloop_" + size + "_activate"
                    }
                }
            }
        }
    }
}
