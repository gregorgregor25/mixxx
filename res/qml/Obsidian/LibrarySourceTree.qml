import Mixxx 1.0 as Mixxx
import QtQml
import QtQuick
import "../Library" as LibraryComponent

Mixxx.LibrarySourceTree {
    id: root

    defaultColumns: [
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Title
            fillSpan: 3
            label: qsTr("Title")
            delegate: TrackCell { showLoadButtons: true }
        },
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Artist
            fillSpan: 2
            label: qsTr("Artist")
            delegate: TrackCell { }
        },
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Bpm
            label: qsTr("BPM")
            preferredWidth: 78
            delegate: TrackCell { }
        },
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Key
            label: qsTr("Key")
            preferredWidth: 72
            delegate: TrackCell { }
        },
        Mixxx.TrackListColumn {
            autoHideWidth: 900
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Album
            fillSpan: 1
            label: qsTr("Album")
            delegate: TrackCell { }
        }
    ]

    Mixxx.LibraryAllTrackSource {
        columns: root.defaultColumns
        label: qsTr("Track Collection")
    }

    Mixxx.LibraryPlaylistSource {
        columns: root.defaultColumns
        label: qsTr("Playlists")
    }

    component TrackCell: LibraryComponent.Cell {
        id: cell

        readonly property var caps: capabilities
        property bool showLoadButtons: false
        color: selected ? "#373b3e" : (row % 2 === 0 ? "#17191c" : "#1e2023")
        Binding on Drag.active {
            delayed: true
            value: dragArea.drag.active
        }

        LibraryComponent.Track {
            id: dragArea
            anchors.fill: parent
            capabilities: cell.caps

            drag.onGrabChanged: (transition, eventPoint) => {
                if (transition !== PointerDevice.GrabPassive && transition !== PointerDevice.GrabExclusive)
                    return;
                cell.dragImage.grabToImage(result => {
                    cell.Drag.imageSource = result.url;
                }, Qt.size(cell.dragImage.width, cell.dragImage.height));
            }

            tap.onDoubleTapped: {
                tableView.selectionModel.selectRow(row);
                tableView.loadSelectedTrackIntoNextAvailableDeck(false);
            }
            tap.onTapped: (eventPoint, button) => {
                if (button === Qt.LeftButton)
                    tableView.selectionModel.selectRow(row);
            }
        }

        Rectangle {
            width: 3
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: "#a9b8d0"
            visible: selected
        }

        Text {
            id: value
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: cell.showLoadButtons ? 80 : 8
            color: selected ? "#f5f6f6" : "#d7dcdd"
            elide: Text.ElideRight
            font.pixelSize: 14
            font.weight: selected ? Font.DemiBold : Font.Normal
            text: display ?? ""
            verticalAlignment: Text.AlignVCenter
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            visible: cell.showLoadButtons

            Repeater {
                model: 2
                delegate: Rectangle {
                    required property int index
                    width: 32
                    height: 30
                    radius: 3
                    border.color: "#5d6267"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#484b4f" }
                        GradientStop { position: 1; color: "#24272b" }
                    }

                    Text {
                        anchors.centerIn: parent
                        color: index === 0 ? "#d9b789" : "#91b4ce"
                        font.pixelSize: 12
                        font.weight: Font.Bold
                        text: index === 0 ? "A" : "B"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            tableView.selectionModel.selectRow(row);
                            tableView.loadSelectedTrack(index === 0 ? "[Channel1]" : "[Channel2]", false);
                        }
                    }
                }
            }
        }
    }
}
