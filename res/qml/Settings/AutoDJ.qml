import QtQuick
import QtQuick.Layouts
import "../Theme"

Category {
    label: qsTr("AutoDJ")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 22
            text: qsTr("AutoDJ")
        }
        Text {
            Layout.fillWidth: true
            color: Theme.lightGray2
            font.pixelSize: 15
            text: qsTr("Set how Mixxx transitions between tracks in the AutoDJ queue.")
            wrapMode: Text.WordWrap
        }
        NativeSettingsEntry {
            Layout.fillWidth: true
            Layout.preferredHeight: 94
            buttonText: qsTr("Open AutoDJ settings")
            description: qsTr("Transitions and queue behavior")
            pageId: "autodj"
            title: qsTr("AutoDJ settings")
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
