import QtQuick
import QtQuick.Layouts
import "../Theme"

Category {
    label: qsTr("Recording")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 22
            text: qsTr("Recording")
        }
        Text {
            Layout.fillWidth: true
            color: Theme.lightGray2
            font.pixelSize: 15
            text: qsTr("Choose where Mixxx saves recordings and which audio format it uses. Opening preferences does not start recording.")
            wrapMode: Text.WordWrap
        }
        NativeSettingsEntry {
            Layout.fillWidth: true
            Layout.preferredHeight: 94
            buttonText: qsTr("Open recording settings")
            description: qsTr("Location and format")
            pageId: "recording"
            title: qsTr("Recording settings")
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
