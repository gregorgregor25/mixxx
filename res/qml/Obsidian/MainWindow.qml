import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin

Item {
    id: root
    required property Controls.ApplicationWindow applicationWindow
    readonly property var menuBar: null
    property bool expandedLibrary: false
    readonly property real uiScale: Math.min(width / 1280, height / 740)

    Connections {
        target: root.applicationWindow
        function onClosing(close) {
            if (settings.visible) {
                close.accepted = false;
                settings.returnToDecks();
            }
        }
    }

    TapHandler {
        onTapped: function(point) {
            if (!library.searchContains(root, point.position.x, point.position.y))
                library.dismissSearch();
        }
    }

    Mixxx.ControlProxy { group: "[App]"; key: "num_decks"; onInitializedChanged: function(ready) { if (ready) value = 4; } }
    Mixxx.ControlProxy { group: "[App]"; key: "num_samplers"; onInitializedChanged: function(ready) { if (ready) value = 16; } }
    Mixxx.ControlProxy { group: "[App]"; key: "num_preview_decks"; onInitializedChanged: function(ready) { if (ready) value = 1; } }
    Mixxx.ControlProxy {
        id: cueA; group: "[Channel1]"; key: "pfl"
        onInitializedChanged: function(ready) { if (ready) value = 0; }
    }
    Mixxx.ControlProxy {
        id: cueB; group: "[Channel2]"; key: "pfl"
        onInitializedChanged: function(ready) { if (ready) value = 0; }
    }
    Mixxx.ControlProxy { id: clipping; group: "[Main]"; key: "peak_indicator" }
    Mixxx.ControlProxy { id: recording; group: "[Recording]"; key: "status" }
    Skin.LibraryScanSummaryDialog {}

    Rectangle {
        id: surface
        color: "#101214"
        width: root.width / root.uiScale
        height: root.height / root.uiScale
        scale: root.uiScale
        transformOrigin: Item.TopLeft
        ColumnLayout {
            anchors.fill: parent
            spacing: 1
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                Layout.minimumHeight: 38
                Layout.maximumHeight: 38
                color: "#24282c"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8; spacing: 10
                    Text { text: "Mixxx"; color: "#f0f1f2"; font.pixelSize: 20; font.bold: true }
                    Text { text: "PERFORMANCE"; color: "#aeb5bb"; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "CUE  " + (cueA.value > 0 ? "A" : "—") + "  " + (cueB.value > 0 ? "B" : "—")
                        color: cueA.value > 0 || cueB.value > 0 ? "#cbd6df" : "#929ba2"
                        font.pixelSize: 10
                    }
                    Text { text: clipping.value > 0 ? "MASTER  CLIP" : "MASTER"; color: clipping.value > 0 ? "#ff8b7c" : "#b9c1c8"; font.pixelSize: 10 }
                    Skin.VuMeter { group: "[Main]"; key: "vu_meter_left"; width: 5; Layout.preferredHeight: 24 }
                    Skin.VuMeter { group: "[Main]"; key: "vu_meter_right"; width: 5; Layout.preferredHeight: 24 }
                    Button { text: recording.value === 2 ? "REC ●" : "Tools"; onClicked: tools.open() }
                    Button { text: root.expandedLibrary ? "Decks" : "Browser"; onClicked: root.expandedLibrary = !root.expandedLibrary }
                    Button {
                        text: "Settings"
                        onClicked: settings.open()
                        onPressAndHold: Mixxx.PreferencesDialog.show()
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 76; Layout.minimumHeight: 76; Layout.maximumHeight: 76; spacing: 1
                visible: !root.expandedLibrary
                EffectRack { Layout.fillWidth: true; Layout.fillHeight: true; unitNumber: 1; accent: "#d8b17c" }
                EffectRack { Layout.fillWidth: true; Layout.fillHeight: true; unitNumber: 2; accent: "#8db8d7" }
            }
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 54; Layout.minimumHeight: 54; Layout.maximumHeight: 54; spacing: 1
                DeckHeader { Layout.fillWidth: true; Layout.fillHeight: true; group: "[Channel1]"; letter: "A"; accent: "#d8b17c" }
                DeckHeader { Layout.fillWidth: true; Layout.fillHeight: true; group: "[Channel2]"; letter: "B"; accent: "#8db8d7" }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 112; Layout.minimumHeight: 112; Layout.maximumHeight: 112
                visible: !root.expandedLibrary
                color: "#080b0d"; clip: true
                Column {
                    anchors.fill: parent
                    Skin.WaveformDisplay { group: "[Channel1]"; originalTrackContent: true; width: parent.width; height: parent.height / 2 }
                    Skin.WaveformDisplay { group: "[Channel2]"; originalTrackContent: true; width: parent.width; height: parent.height / 2 }
                }
                Rectangle { y: parent.height / 2; width: parent.width; height: 1; color: "#343b40" }
            }
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 200; Layout.minimumHeight: 200; Layout.maximumHeight: 200; spacing: 1
                visible: !root.expandedLibrary
                DeckPanel { Layout.fillWidth: true; Layout.fillHeight: true; group: "[Channel1]"; letter: "A"; accent: "#d8b17c" }
                Mixer { Layout.preferredWidth: 280; Layout.fillHeight: true }
                DeckPanel { Layout.fillWidth: true; Layout.fillHeight: true; group: "[Channel2]"; letter: "B"; accent: "#8db8d7" }
            }
            LibraryPanel {
                id: library
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumHeight: 210
                expanded: root.expandedLibrary
                onExpandRequested: root.expandedLibrary = !root.expandedLibrary
            }
        }
    }
    Skin.Settings {
        id: settings
        parent: Controls.Overlay.overlay
        width: Math.min(root.width, 1400); height: Math.min(root.height, 840)
        x: (root.width - width) / 2; y: (root.height - height) / 2
        modal: true
    }
    Controls.Popup {
        id: tools
        parent: Controls.Overlay.overlay
        x: (root.width - width) / 2; y: (root.height - height) / 2
        width: Math.min(520, root.width - 24); height: 360
        modal: true
        background: Rectangle { color: "#202427"; border.color: "#515960"; radius: 4 }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "Session tools"; color: "#e4e7e9"; font.bold: true; font.pixelSize: 20 }
            Button {
                Layout.fillWidth: true; text: "Recording settings"
                onClicked: { tools.close(); Mixxx.PreferencesDialog.showPage("recording"); }
            }
            ControlButton {
                Layout.fillWidth: true; group: "[Recording]"; key: "toggle_recording"
                text: recording.value === 2 ? "Stop recording" : "Start recording"
            }
            Text { text: "Beatgrid"; color: "#c1c8ce"; font.pixelSize: 14 }
            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: 2
                    ColumnLayout {
                        required property int index
                        Text { text: index === 0 ? "Deck A" : "Deck B"; color: "#c1c8ce" }
                        ControlButton { group: "[Channel" + (index + 1) + "]"; key: "beats_translate_curpos"; text: "Set grid at playhead" }
                        ControlButton { group: "[Channel" + (index + 1) + "]"; key: "bpmlock"; toggleable: true; text: "Lock beatgrid" }
                    }
                }
            }
            Item { Layout.fillHeight: true }
            Button { Layout.alignment: Qt.AlignRight; text: "Close"; onClicked: tools.close() }
        }
    }
}
