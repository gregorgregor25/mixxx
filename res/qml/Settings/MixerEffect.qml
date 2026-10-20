import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme"

Category {
    label: qsTr("Mixer & Effects")

    ScrollView {
        id: settingsScroll

        anchors.fill: parent
        anchors.margins: 24
        clip: true

        ColumnLayout {
            spacing: 18
            width: settingsScroll.availableWidth

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.bold: true
                font.pixelSize: 22
                text: qsTr("Mixer & Effects")
            }
            Flx6FilterSettings {
                Layout.fillWidth: true
            }
            NativeSettingsEntry {
                Layout.fillWidth: true
                Layout.preferredHeight: 94
                buttonText: qsTr("Open mixer settings")
                description: qsTr("Mixer and crossfader behavior")
                pageId: "mixer"
                title: qsTr("Mixer")
            }
            NativeSettingsEntry {
                Layout.fillWidth: true
                Layout.preferredHeight: 94
                buttonText: qsTr("Open effects settings")
                description: qsTr("Effects and filter behavior")
                pageId: "effects"
                title: qsTr("Effects")
            }
        }
    }
}
