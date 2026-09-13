// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Item {
    id: root

    property bool hasRecord: false
    property string viewMode: "time"
    property var document
    property var analysis
    property var locusAnalysis
    property var harmonicSnapshot
    property var tableSnapshot
    property real viewStart: 0.0
    property real visibleDuration: 0.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property var voltageChannels: []
    property var currentChannels: []
    property var residualChannels: []
    property var harmonicChannels: []
    property var tableChannels: []
    property int selectedChannel: -1
    property string valueRepresentation: "secondary"

    signal signalActivated(int channelIndex)

    // A heavy engineering view is created only after its first explicit use.
    // Once created it stays alive for the current record, so returning to the
    // view is a visibility toggle instead of QML object reconstruction.
    property bool phasorWarm: false
    property bool locusWarm: false
    property bool harmonicsWarm: false
    property bool tableWarm: false

    function warmCurrentView() {
        if (!hasRecord) return
        if (viewMode === "phasor") phasorWarm = true
        else if (viewMode === "locus") locusWarm = true
        else if (viewMode === "harmonics") harmonicsWarm = true
        else if (viewMode === "table") tableWarm = true
    }

    function resetForRecord() {
        phasorWarm = false
        locusWarm = false
        harmonicsWarm = false
        tableWarm = false
        warmCurrentView()
    }

    readonly property bool activeViewLoading:
        (viewMode === "phasor" && phasorLoader.status === Loader.Loading)
        || (viewMode === "locus" && locusLoader.status === Loader.Loading)
        || (viewMode === "harmonics" && harmonicsLoader.status === Loader.Loading)
        || (viewMode === "table" && tableLoader.status === Loader.Loading)

    onViewModeChanged: warmCurrentView()
    onHasRecordChanged: if (hasRecord) warmCurrentView()
    Component.onCompleted: warmCurrentView()

    Loader {
        id: phasorLoader
        anchors.fill: parent
        active: root.hasRecord && root.phasorWarm
        asynchronous: true
        visible: status === Loader.Ready && root.viewMode === "phasor"
        sourceComponent: Component {
            PhasorView {
                document: root.document
                analysis: root.viewMode === "phasor" ? root.analysis : null
                cursorATime: root.cursorATime
                cursorBTime: root.cursorBTime
                voltageChannels: root.voltageChannels
                currentChannels: root.currentChannels
                residualChannels: root.residualChannels
            }
        }
    }

    Loader {
        id: locusLoader
        anchors.fill: parent
        active: root.hasRecord && root.locusWarm
        asynchronous: true
        visible: status === Loader.Ready && root.viewMode === "locus"
        sourceComponent: Component {
            LocusView {
                document: root.document
                analysis: root.viewMode === "locus" ? root.locusAnalysis : null
                viewStart: root.viewStart
                visibleDuration: root.visibleDuration
                cursorATime: root.cursorATime
                cursorBTime: root.cursorBTime
            }
        }
    }

    Loader {
        id: harmonicsLoader
        anchors.fill: parent
        active: root.hasRecord && root.harmonicsWarm
        asynchronous: true
        visible: status === Loader.Ready && root.viewMode === "harmonics"
        sourceComponent: Component {
            HarmonicsView {
                document: root.document
                analysis: root.viewMode === "harmonics" ? root.analysis : null
                snapshot: root.viewMode === "harmonics" ? root.harmonicSnapshot : null
                visibleChannels: root.harmonicChannels
                cursorTime: root.cursorATime
            }
        }
    }

    Loader {
        id: tableLoader
        anchors.fill: parent
        active: root.hasRecord && root.tableWarm
        asynchronous: true
        visible: status === Loader.Ready && root.viewMode === "table"
        sourceComponent: Component {
            ValueTableView {
                document: root.document
                analysis: root.viewMode === "table" ? root.analysis : null
                snapshot: root.viewMode === "table" ? root.tableSnapshot : null
                visibleChannels: root.tableChannels
                cursorTime: root.cursorATime
                valueRepresentation: root.valueRepresentation
                selectedChannel: root.selectedChannel
                onSignalActivated: channelIndex => root.signalActivated(channelIndex)
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 250
        height: 66
        radius: 4
        visible: root.hasRecord && root.activeViewLoading
        color: "#f8f9fa"
        border.color: "#c9cfd4"
        z: 20

        Row {
            anchors.centerIn: parent
            spacing: 12
            BusyIndicator { width: 24; height: 24; running: parent.parent.visible }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "Preparing " + root.viewMode.toUpperCase() + " view…"
                color: "#4d5962"
                font.pixelSize: 11
            }
        }
    }
}
