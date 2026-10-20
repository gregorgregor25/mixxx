import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts

Rectangle {
    id: root

    required property string group
    required property color accent
    required property string letter
    readonly property var player: Mixxx.PlayerManager.getPlayer(group)
    readonly property var track: player?.currentTrack
    readonly property bool loaded: player?.isLoaded ?? false
    readonly property bool faderUp: volumeControl.value > 0.01
    readonly property real remainingSeconds: Math.max(0,
            durationControl.value * (1 - playPositionControl.value))

    function formatTime(seconds) {
        if (!Number.isFinite(seconds))
            return "--:--";
        const total = Math.floor(seconds);
        return Math.floor(total / 60).toString().padStart(2, "0") +
                ":" + (total % 60).toString().padStart(2, "0");
    }

    color: "#191c1f"

    Mixxx.ControlProxy { id: volumeControl; group: root.group; key: "volume" }
    Mixxx.ControlProxy { id: bpmControl; group: root.group; key: "bpm" }
    Mixxx.ControlProxy { id: rateRatioControl; group: root.group; key: "rate_ratio" }
    Mixxx.ControlProxy { id: durationControl; group: root.group; key: "duration" }
    Mixxx.ControlProxy { id: playPositionControl; group: root.group; key: "playposition" }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: 3
            color: root.faderUp ? Qt.darker(root.accent, 3.8) : "#24282a"
            border.width: 1
            border.color: root.faderUp ? root.accent : "#51575a"

            Text {
                anchors.centerIn: parent
                text: root.letter
                color: root.faderUp ? root.accent : "#b8bec0"
                font.pixelSize: 21
                font.weight: Font.Bold
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.loaded ? (root.track?.title || "Untitled track") : "No track loaded"
                color: "#f0f1f0"
                elide: Text.ElideRight
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: root.loaded ? (root.track?.artist || "Unknown artist") : ""
                color: "#b6bec0"
                elide: Text.ElideRight
                font.pixelSize: 11
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 72
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: root.loaded && Number.isFinite(bpmControl.value) ?
                        bpmControl.value.toFixed(2) : "--.--"
                color: "#f0f1f0"
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 17
                font.weight: Font.Bold
                font.family: "Roboto"
            }

            Text {
                Layout.fillWidth: true
                text: root.loaded && Number.isFinite(rateRatioControl.value) ?
                        "BPM  " + ((rateRatioControl.value - 1) * 100 >= 0 ? "+" : "") +
                        ((rateRatioControl.value - 1) * 100).toFixed(1) + "%" : "BPM"
                color: "#aeb6b8"
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 9
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 57
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: root.loaded ? root.formatTime(root.remainingSeconds) : "--:--"
                color: "#f0f1f0"
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 16
                font.weight: Font.DemiBold
                font.family: "Roboto"
            }

            Text {
                Layout.fillWidth: true
                text: "REMAIN"
                color: "#aeb6b8"
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 9
            }
        }
    }
}
