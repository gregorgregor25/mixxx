import QtQuick
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Setting
import "../Theme"

Mixxx.SettingParameter {
    id: root

    required property string pageId
    required property string title
    required property string description
    required property string buttonText
    property bool featureAvailable: true
    property string unavailableText: qsTr("This settings page is unavailable in this Android build.")
    property string statusMessage: ""
    readonly property bool pageAvailable: featureAvailable && Mixxx.PreferencesDialog.hasPage(pageId)

    label: title

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    color: Theme.white
                    font.bold: true
                    font.pixelSize: 16
                    text: root.title
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    color: Theme.lightGray2
                    font.pixelSize: 14
                    text: root.description
                    wrapMode: Text.WordWrap
                }
            }
            Setting.FormButton {
                Layout.preferredHeight: 48
                Layout.preferredWidth: 190
                Accessible.name: text
                enabled: root.pageAvailable
                opacity: enabled ? 1 : 0.5
                text: root.buttonText

                onClicked: {
                    root.statusMessage = Mixxx.PreferencesDialog.showPage(root.pageId)
                            ? "" : qsTr("This settings page could not be opened.");
                }
            }
        }
        Text {
            Layout.fillWidth: true
            color: Theme.lightGray2
            font.pixelSize: 13
            text: root.statusMessage || (root.pageAvailable ? "" : root.unavailableText)
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }
}
