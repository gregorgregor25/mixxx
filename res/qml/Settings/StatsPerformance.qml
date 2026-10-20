import QtQuick
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Setting
import "../Theme"

Category {
    label: qsTr("Stats & Performance")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 22
            text: qsTr("Stats & Performance")
        }
        Text {
            Layout.fillWidth: true
            color: Theme.lightGray2
            font.pixelSize: 15
            text: qsTr("Choose a diagnostic statistics mode for this session. These controls do not change your saved audio setup.")
            wrapMode: Text.WordWrap
        }
        Mixxx.SettingParameter {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            label: qsTr("Statistics mode")

            RowLayout {
                anchors.fill: parent
                spacing: 12

                Setting.FormButton {
                    Layout.preferredHeight: 48
                    Layout.preferredWidth: 140
                    Accessible.name: qsTr("Statistics off")
                    highlight: !Mixxx.Application.baseStatsEnabled && !Mixxx.Application.experimentStatsEnabled
                    text: qsTr("Off")

                    onClicked: Mixxx.Application.setBaseStatsEnabled(false)
                }
                Setting.FormButton {
                    Layout.preferredHeight: 48
                    Layout.preferredWidth: 140
                    Accessible.name: qsTr("Base statistics")
                    highlight: Mixxx.Application.baseStatsEnabled
                    text: qsTr("Base")

                    onClicked: Mixxx.Application.setBaseStatsEnabled(true)
                }
                Setting.FormButton {
                    Layout.preferredHeight: 48
                    Layout.preferredWidth: 140
                    Accessible.name: qsTr("Experiment statistics")
                    highlight: Mixxx.Application.experimentStatsEnabled
                    text: qsTr("Experiment")

                    onClicked: Mixxx.Application.setExperimentStatsEnabled(true)
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
        Text {
            Layout.fillWidth: true
            color: Theme.lightGray2
            font.pixelSize: 14
            text: qsTr("These diagnostic buckets compare Mixxx's performance during a run. They do not measure speaker or headphone timing.")
            wrapMode: Text.WordWrap
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
