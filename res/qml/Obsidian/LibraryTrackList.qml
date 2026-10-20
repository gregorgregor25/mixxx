import ".." as Skin
import "../Library" as LibraryComponent
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import "../Theme"

Rectangle {
    id: root

    required property var model

    function loadSelectedTrack(group, play) {
        view.loadSelectedTrack(group, play);
    }

    function focusSelection() {
        view.forceActiveFocus();
    }

    color: "#17191c"

    LibraryComponent.Control {
        id: libraryControl

        onFocusWidgetChanged: {
            switch (focusWidget) {
            case Skin.FocusedWidgetControl.WidgetKind.LibraryView:
                view.forceActiveFocus();
                break;
            }
        }
        onLoadSelectedTrack: (group, play) => {
            view.loadSelectedTrack(group, play);
        }
        onLoadSelectedTrackIntoNextAvailableDeck: play => {
            view.loadSelectedTrackIntoNextAvailableDeck(play);
        }
        onMoveVertical: offset => {
            view.selectionModel.moveSelectionVertical(offset);
        }
    }
    HorizontalHeaderView {
        id: horizontalHeader

        property int sortingColumn: -1
        property var sortingOrder: Qt.DescendingOrder

        anchors.left: parent.left
        anchors.margins: 0
        anchors.right: parent.right
        anchors.top: parent.top
        syncView: view

        delegate: Item {
            id: column

            required property string display
            required property int index

            implicitHeight: 28
            implicitWidth: columnName.contentWidth + 5

            MouseArea {
                id: columnMouseHandler

                acceptedButtons: Qt.LeftButton
                anchors.fill: parent

                onClicked: {
                    if (horizontalHeader.sortingColumn == index) {
                        horizontalHeader.sortingOrder = horizontalHeader.sortingOrder == Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder;
                    } else {
                        horizontalHeader.sortingColumn = index;
                        horizontalHeader.sortingOrder = Qt.AscendingOrder;
                    }
                    view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
                }
            }
            Text {
                id: columnName

                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 36
                color: "#c6ccce"
                elide: Text.ElideRight
                font.capitalization: Font.Capitalize
                font.family: Theme.fontFamily
                font.pixelSize: 11
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignLeft
                text: display
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                color: "#a9b8d0"
                font.pixelSize: 9
                font.weight: Font.Bold
                text: horizontalHeader.sortingOrder == Qt.AscendingOrder ? "ASC" : "DESC"
                visible: horizontalHeader.sortingColumn == index
            }
            Rectangle {
                id: columnResizer

                color: "#414449"
                width: 1

                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    top: parent.top
                }
                MouseArea {
                    id: columnResizeHandler

                    property int sizeOffset: 0

                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    preventStealing: true

                    onMouseXChanged: {
                        if (drag.active) {
                            column.width += mouseX;
                            sizeOffset += mouseX;
                        }
                    }

                    drag {
                        axis: Drag.XAxis
                        target: parent
                        threshold: 2

                        onActiveChanged: {
                            if (!drag.active && columnResizeHandler.sizeOffset !== 0) {
                                view.model.columns[index].preferredWidth = column.width;
                                columnResizeHandler.sizeOffset = 0;
                                view.updateColumnSize();
                                view.forceLayout();
                            }
                        }
                    }
                }
            }
        }
    }
    TableView {
        id: view

        property int dynamicColumnCount: 0
        property int usedWidth: 0

        function loadSelectedTrack(group, play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.getPlayer(group).loadTrackFromLocationUrl(urls[0], play);
        }
        function loadSelectedTrackIntoNextAvailableDeck(play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(urls[0], play);
        }
        function updateColumnSize() {
            const oldUsedWidth = usedWidth;
            const oldDynamicColumnCount = dynamicColumnCount;
            usedWidth = 0;
            dynamicColumnCount = 0;
            if (model == null) {
                return;
            }
            for (let c = 0; c < model.columns.length; c++) {
                if (model.columns[c].hidden || model.columns[c].autoHideWidth > view.width) {
                    continue;
                } else if (model.columns[c].preferredWidth > 0) {
                    usedWidth += model.columns[c].preferredWidth;
                } else {
                    dynamicColumnCount += model.columns[c].fillSpan || 1;
                }
            }
            return oldDynamicColumnCount != dynamicColumnCount || oldUsedWidth != usedWidth;
        }

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 0
        anchors.right: parent.right
        anchors.top: horizontalHeader.bottom
        clip: true
        columnWidthProvider: function (column) {
            const columnDef = view.model.columns[column];
            if (columnDef.hidden) {
                return 0;
            }
            if (columnDef.autoHideWidth > 0 && columnDef.autoHideWidth > view.width) {
                return 0;
            }
            if (columnDef.preferredWidth >= 0) {
                return columnDef.preferredWidth;
            }
            const span = columnDef.fillSpan || 1;
            return span * (view.width - view.usedWidth) / view.dynamicColumnCount;
        }
        keyNavigationEnabled: false
        model: root.model
        pointerNavigationEnabled: false
        reuseItems: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
        }
        delegate: Item {
            id: item

            required property url cover_art
            required property color decoration
            required property var display
            required property string file_url
            required property int row
            required property bool selected
            required property var track

            implicitHeight: 36

            Loader {
                id: loader

                property var capabilities: root.model ? root.model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
                property url cover_art: item.cover_art
                property color decoration: item.decoration
                property var display: item.display
                property url file_url: item.file_url
                property int row: item.row
                property bool selected: item.selected
                property var tableView: view
                property var track: item.track

                anchors.fill: parent
                focus: true
                sourceComponent: delegate

                onLoaded:
                // Workaround needed for WaveformOverview column to load the data
                //     if (track)
                //         Mixxx.Library.analyze(track)
                {}
            }
            // Workaround needed for WaveformOverview column to load the data
            // TableView.onReused: {
            //     if (track)
            //         Mixxx.Library.analyze(track)
            // }
        }
        selectionModel: ItemSelectionModel {
            function moveSelectionVertical(value) {
                if (value == 0)
                    return;

                const current = this.currentIndex;
                const oldRow = current.row >= 0 ? current.row : (value > 0 ? -1 : 0);
                this.selectRow(oldRow + value);
            }
            function selectRow(row) {
                const rowCount = this.model.rowCount();
                if (rowCount == 0) {
                    this.clear();
                    return;
                }
                const newRow = Math.max(0, Math.min(Math.trunc(row), rowCount - 1));
                if (this.currentIndex.row === newRow)
                    return;
                const index = this.model.index(newRow, 0);
                this.select(index, ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear);
                this.setCurrentIndex(index, ItemSelectionModel.NoUpdate);
                view.positionViewAtRow(newRow, TableView.Contain);
            }
            function selectedTrackUrls() {
                return this.selectedIndexes.map(index => {
                    return this.model.getUrl(index.row);
                });
            }

            model: view.model
        }

        Component.onCompleted: this.updateColumnSize()
        Keys.onDownPressed: this.selectionModel.moveSelectionVertical(1)
        Keys.onEnterPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onReturnPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onUpPressed: this.selectionModel.moveSelectionVertical(-1)
        onModelChanged: {
            this.updateColumnSize();
            Qt.callLater(function() {
                if (view.model && view.selectionModel && view.model.rowCount() > 0)
                    view.selectionModel.selectRow(0);
            });
        }
        onWidthChanged: {
            if (view.updateColumnSize()) {
                // forceLayout is costly - only invoke if there was a change in the column layouts
                view.forceLayout();
            }
        }
    }
}
