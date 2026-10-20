import QtQuick
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../Theme"

Category {
    label: qsTr("Broadcast")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 22
            text: qsTr("Broadcast")
        }
        Text {
            Layout.fillWidth: true
            color: Theme.lightGray2
            font.pixelSize: 15
            text: qsTr("Configure live broadcasting when it is supported by this Mixxx build. Opening preferences does not start a broadcast.")
            wrapMode: Text.WordWrap
        }
        NativeSettingsEntry {
            Layout.fillWidth: true
            Layout.preferredHeight: 94
            buttonText: qsTr("Open broadcast settings")
            description: qsTr("Servers and audio format")
            featureAvailable: Mixxx.Application.liveBroadcastingAvailable
            pageId: "broadcast"
            title: qsTr("Broadcast settings")
            unavailableText: qsTr("Live broadcasting is unavailable in this Android build.")
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
