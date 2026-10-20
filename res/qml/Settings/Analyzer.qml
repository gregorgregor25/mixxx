import QtQuick
import QtQuick.Layouts
import "../Theme"

Category {
    label: qsTr("Analyzer")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 22
            text: qsTr("Track Analysis")
        }
        Text {
            Layout.fillWidth: true
            color: Theme.lightGray2
            font.pixelSize: 15
            text: qsTr("Choose how Mixxx analyzes beats, musical key, and track loudness.")
            wrapMode: Text.WordWrap
        }
        NativeSettingsEntry {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            buttonText: qsTr("Open beat settings")
            description: qsTr("BPM and beat grid")
            pageId: "beats"
            title: qsTr("Beat analysis")
        }
        NativeSettingsEntry {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            buttonText: qsTr("Open key settings")
            description: qsTr("Detect musical key")
            pageId: "key"
            title: qsTr("Key analysis")
        }
        NativeSettingsEntry {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            buttonText: qsTr("Open loudness settings")
            description: qsTr("ReplayGain normalization")
            pageId: "replaygain"
            title: qsTr("Loudness analysis")
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
