import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import QtQml.Models
import Mixxx 1.0 as Mixxx
import ".." as Skin

Rectangle {
    id: root

    property bool expanded: false
    property string selectedSource: qsTr("Track Collection")
    property bool importing: false
    property var importQueue: []
    property var importErrors: []
    property string importStatus: ""
    property int importedTracks: 0
    readonly property var sidebar: librarySources.sidebar()
    readonly property string analysisStatus: {
        const total = Mixxx.Library.analysisTotalTracks;
        const completed = Mixxx.Library.analysisCompletedTracks;
        if (total <= 0)
            return "";
        if (Mixxx.Library.analysisActive)
            return qsTr("Analysing: %1 of %2 complete · current track %3%")
                    .arg(completed).arg(total)
                    .arg(Math.round(Mixxx.Library.analysisCurrentTrackProgress * 100));
        if (completed >= total)
            return qsTr("Analysis complete: %1 tracks.").arg(completed);
        return qsTr("Analysis ended: %1 of %2 complete.").arg(completed).arg(total);
    }
    signal expandRequested()

    function focusSources() {
        if (!sourceColumn.visible)
            return;
        dismissSearch();
        libraryFocus.value = Skin.FocusedWidgetControl.WidgetKind.Sidebar;
        sourceTree.forceActiveFocus();
        if (sourceTree.currentRow < 0)
            sourceTree.selectRow(0);
    }

    function openSelectedSource() {
        const row = sourceTree.currentRow;
        if (row < 0)
            return;
        const entry = sourceTree.itemAtCell(0, row);
        if (entry && (entry.hasChildren || (entry.depth === 0 && row !== 0))) {
            sourceTree.toggleExpanded(row);
            return;
        }
        if (sourceTree.depth(row) === 0 && row !== 0) {
            sourceTree.toggleExpanded(row);
            return;
        }
        root.sidebar.activate(sourceTree.index(row, 0));
        if (entry)
            root.selectedSource = entry.label;
        libraryFocus.value = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
        trackList.focusSelection();
    }

    Skin.FocusedWidgetControl {
        id: libraryFocus
        onValueChanged: function(value) {
            if (value === Skin.FocusedWidgetControl.WidgetKind.Sidebar)
                sourceTree.forceActiveFocus();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusBackward"
        onValueChanged: function(value) {
            if (value > 0)
                root.focusSources();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusForward"
        onValueChanged: function(value) {
            if (value > 0 && libraryFocus.value === Skin.FocusedWidgetControl.WidgetKind.Sidebar)
                root.openSelectedSource();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveVertical"
        onValueChanged: function(value) {
            if (value !== 0 && libraryFocus.value === Skin.FocusedWidgetControl.WidgetKind.Sidebar)
                sourceTree.moveSelectionVertical(value);
        }
    }

    function openImport() {
        importFolders.clear();
        const folders = Mixxx.Library.copiedMusicFolders();
        for (const folder of folders)
            importFolders.append({ name: folder.name, directory: folder.directory.toString(), chosen: false });
        importDialog.open();
    }

    function importNext() {
        if (importQueue.length === 0) {
            importing = false;
            importStatus = qsTr("%1 tracks queued for analysis.").arg(importedTracks);
            if (importErrors.length > 0)
                importStatus += " " + importErrors.join("; ");
            return;
        }
        const next = importQueue.shift();
        importStatus = qsTr("Importing %1…").arg(next.name);
        Mixxx.Library.importFolderPlaylist(next.directory, next.name);
    }

    Connections {
        target: Mixxx.Library
        function onFolderPlaylistImportFinished(name, supportedFiles, addedToPlaylist, queuedForAnalysis, error) {
            if (error.length > 0) {
                root.importErrors.push(name + ": " + error);
            }
            root.importedTracks += queuedForAnalysis;
            root.importStatus = qsTr("%1 tracks queued for analysis.").arg(root.importedTracks);
            sourceTree.expandRecursively();
            root.importNext();
        }
    }

    ListModel { id: importFolders }
    Popup {
        id: importDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(580, parent.width - 40)
        height: Math.min(520, parent.height - 40)
        padding: 16
        modal: true
        background: Rectangle { color: "#202427"; border.color: "#515960"; radius: 4 }
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: qsTr("Import copied music folders"); color: "#f1f2f2"; font.pixelSize: 20; font.bold: true }
            Text {
                Layout.fillWidth: true
                text: qsTr("Each selected folder becomes a playlist. Mixxx will analyse the tracks in the background.")
                color: "#c3c9ce"; wrapMode: Text.WordWrap; font.pixelSize: 14
            }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; model: importFolders
                delegate: CheckDelegate {
                    required property int index
                    required property string name
                    required property bool chosen
                    width: ListView.view.width; height: 46
                    text: name; checked: chosen
                    contentItem: Text {
                        text: parent.text
                        color: "#f1f2f2"
                        font.pixelSize: 16
                        verticalAlignment: Text.AlignVCenter
                        rightPadding: 40
                        elide: Text.ElideRight
                    }
                    onToggled: importFolders.setProperty(index, "chosen", checked)
                }
                Text {
                    anchors.centerIn: parent
                    visible: importFolders.count === 0
                    text: qsTr("No copied music folders found.")
                    color: "#c3c9ce"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Cancel"); onClicked: importDialog.close() }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Import selected")
                    onClicked: {
                        const folders = [];
                        for (let i = 0; i < importFolders.count; ++i) {
                            const entry = importFolders.get(i);
                            if (entry.chosen)
                                folders.push({ name: entry.name, directory: entry.directory });
                        }
                        if (folders.length === 0)
                            return;
                        root.importQueue = folders;
                        root.importErrors = [];
                        root.importedTracks = 0;
                        root.importing = true;
                        importDialog.close();
                        root.importNext();
                    }
                }
            }
        }
    }

    function applySearch() {
        if (sidebar && sidebar.tracklist)
            sidebar.tracklist.search(searchField.text);
    }

    function searchContains(item, x, y) {
        const position = searchField.mapFromItem(item, x, y);
        return position.x >= 0 && position.y >= 0 &&
                position.x < searchField.width && position.y < searchField.height;
    }

    function dismissSearch() {
        if (!searchField.editingRequested)
            return;
        searchField.editingRequested = false;
        searchField.focus = false;
        Qt.inputMethod.hide();
    }

    color: "#17191c"
    clip: true

    Component.onCompleted: {
        if (sidebar)
            sidebar.activate(sidebar.index(0, 0));
    }

    LibrarySourceTree {
        id: librarySources
    }

    Connections {
        target: root.sidebar
        function onTracklistChanged() { root.applySearch(); }
    }

    Rectangle {
        id: toolbar
        height: 40
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: "#222529"
        border.color: "#414449"

        Text {
            id: collectionTitle
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            color: "#f1f2f2"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            text: root.selectedSource
            width: 195
            elide: Text.ElideRight
        }

        TextField {
            id: searchField
            property bool editingRequested: false
            width: Math.max(200, Math.min(400, toolbar.width * 0.32))
            height: 32
            anchors.left: parent.width < 700 ? collectionTitle.right : parent.left
            anchors.leftMargin: parent.width < 700 ? 12 : 225
            anchors.verticalCenter: parent.verticalCenter
            color: "#f1f2f2"
            font.pixelSize: 14
            placeholderText: qsTr("Search tracks")
            placeholderTextColor: "#abb3b7"
            Accessible.name: qsTr("Search tracks")
            activeFocusOnTab: false
            readOnly: !editingRequested
            selectByMouse: true
            onActiveFocusChanged: {
                if (!activeFocus)
                    editingRequested = false;
            }
            onAccepted: root.dismissSearch()
            onTextChanged: root.applySearch()

            Connections {
                target: Qt.inputMethod
                function onVisibleChanged() {
                    if (!Qt.inputMethod.visible)
                        root.dismissSearch();
                }
            }

            MouseArea {
                anchors.fill: parent
                visible: !searchField.editingRequested
                cursorShape: Qt.IBeamCursor
                onClicked: {
                    searchField.editingRequested = true;
                    searchField.forceActiveFocus(Qt.MouseFocusReason);
                    Qt.inputMethod.show();
                }
            }

            background: Rectangle {
                radius: 3
                color: "#0c0e10"
                border.color: searchField.activeFocus ? "#a9b8d0" : "#4c5053"
                border.width: searchField.activeFocus ? 2 : 1
            }
        }

        StudioButton {
            id: expandButton
            width: 84
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.expanded ? qsTr("Deck view") : qsTr("Expand")
            onClicked: root.expandRequested()
        }

        StudioButton {
            id: loadBButton
            width: 68
            anchors.right: expandButton.left
            anchors.rightMargin: 7
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Load B")
            onClicked: trackList.loadSelectedTrack("[Channel2]", false)
        }

        StudioButton {
            width: 68
            anchors.right: loadBButton.left
            anchors.rightMargin: 7
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Load A")
            onClicked: trackList.loadSelectedTrack("[Channel1]", false)
        }
    }

    Rectangle {
        id: sourceColumn
        width: root.width < 700 ? 0 : 198
        anchors.left: parent.left
        anchors.top: toolbar.bottom
        anchors.bottom: footer.top
        color: "#1c1e21"
        visible: width > 0

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: "#3c4044"
        }

        Text {
            anchors.top: parent.top
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.leftMargin: 12
            color: "#b5bdc1"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            text: qsTr("YOUR MUSIC")
        }

        TreeView {
            id: sourceTree
            function selectRow(row) {
                if (rows <= 0)
                    return;
                const visibleRow = Math.max(0, Math.min(Math.trunc(row), rows - 1));
                const itemIndex = index(visibleRow, 0);
                selectionModel.select(itemIndex, ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear);
                selectionModel.setCurrentIndex(itemIndex, ItemSelectionModel.NoUpdate);
            }

            function moveSelectionVertical(offset) {
                if (offset === 0 || rows <= 0)
                    return;
                const current = currentRow < 0 ? (offset > 0 ? -1 : 0) : currentRow;
                selectRow(current + offset);
            }

            anchors.top: parent.top
            anchors.topMargin: 39
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.bottom: importButton.top
            anchors.bottomMargin: 6
            clip: true
            model: root.sidebar
            selectionModel: ItemSelectionModel {
                model: root.sidebar
            }
            onCurrentRowChanged: {
                if (currentRow >= 0)
                    positionViewAtRow(currentRow, TableView.Contain);
            }
            Component.onCompleted: Qt.callLater(function() {
                sourceTree.expandRecursively();
                sourceTree.selectRow(0);
            })
            delegate: Rectangle {
                required property int row
                required property int column
                required property int depth
                required property bool expanded
                required property bool hasChildren
                required property string label
                implicitWidth: sourceTree.width
                implicitHeight: 35
                color: libraryFocus.value === Skin.FocusedWidgetControl.WidgetKind.Sidebar && sourceTree.currentRow === row ?
                        "#48545d" : root.selectedSource === label ? "#35393e" : "transparent"
                radius: 3
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 9 + depth * 10
                    anchors.rightMargin: 5
                    verticalAlignment: Text.AlignVCenter
                    text: label
                    color: "#f1f2f2"; font.pixelSize: 13
                    font.weight: depth === 0 ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        sourceTree.selectRow(row);
                        if (hasChildren || (depth === 0 && row !== 0)) {
                            sourceTree.toggleExpanded(row);
                        } else {
                            root.sidebar.activate(sourceTree.index(row, column));
                            root.selectedSource = label;
                            libraryFocus.value = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                            trackList.focusSelection();
                        }
                    }
                }
            }
        }
        StudioButton {
            id: importButton
            anchors.left: parent.left; anchors.leftMargin: 8
            anchors.right: parent.right; anchors.rightMargin: 9
            anchors.bottom: parent.bottom; anchors.bottomMargin: 6
            text: root.importing ? qsTr("Importing…") : qsTr("Import folders")
            enabled: !root.importing
            onClicked: root.openImport()
        }
    }

    LibraryTrackList {
        id: trackList
        anchors.left: sourceColumn.right
        anchors.right: parent.right
        anchors.top: toolbar.bottom
        anchors.bottom: footer.top
        model: root.sidebar ? root.sidebar.tracklist : null
    }

    Rectangle {
        id: footer
        height: 25
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "#202226"
        border.color: "#373b40"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            color: "#bac1c4"
            font.pixelSize: 11
            anchors.right: parent.right
            anchors.rightMargin: 8
            elide: Text.ElideRight
            text: {
                let status = root.importing ? root.importStatus :
                        root.analysisStatus.length > 0 ? root.analysisStatus :
                        root.importStatus.length > 0 ? root.importStatus :
                        qsTr("Select a track, then load A or B. Double tap to load the next available deck.");
                if (!root.importing && root.analysisStatus.length > 0 && root.importErrors.length > 0)
                    status += " " + root.importErrors.join("; ");
                return status;
            }
        }
    }

    component StudioButton: Button {
        id: studioButton
        height: 32
        padding: 4

        contentItem: Text {
            color: "#f1f2f2"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            text: studioButton.text
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 3
            border.color: studioButton.down ? "#24282b" : (studioButton.activeFocus ? "#bac9d7" : "#4d5156")
            gradient: Gradient {
                GradientStop { position: 0; color: studioButton.down ? "#1e2024" : "#44474b" }
                GradientStop { position: 0.5; color: studioButton.down ? "#292c30" : "#303337" }
                GradientStop { position: 1; color: studioButton.down ? "#303337" : "#222428" }
            }
        }
    }
}
