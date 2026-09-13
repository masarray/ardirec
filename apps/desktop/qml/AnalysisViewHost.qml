// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Item {
    id: root

    property bool hasRecord: false
    property bool live: true
    property bool requestOwner: true
    property string viewType: "time"
    property var document
    property var analysis
    property var locusAnalysis
    property var harmonicSnapshot
    property var tableSnapshot
    property real zoomFactor: 1.0
    property real panFraction: 0.0
    property real viewStart: 0.0
    property real visibleDuration: 0.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property var voltageChannels: []
    property var currentChannels: []
    property var otherChannels: []
    property var displayedDigitalChannels: []
    property string digitalDisplayMode: "active"
    property string timeDisplayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property var phasorVoltageChannels: []
    property var phasorCurrentChannels: []
    property var residualChannels: []
    property var harmonicChannels: []
    property var tableChannels: []
    property int selectedChannel: -1
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28

    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)
    signal panRequested(real panFraction)
    signal zoomRequested(real factor, real anchorFraction)
    signal digitalDisplayModeRequested(string mode)
    signal signalActivated(int channelIndex)

    // Retain the QML view instance so child-local visual state survives minimize,
    // but freeze/remove expensive inputs while the child is minimized. Closing a
    // child destroys this host through the workspace delegate lifetime.
    Loader {
        id: viewLoader
        anchors.fill: parent
        active: root.hasRecord
        asynchronous: root.viewType !== "time"
        visible: root.live && status === Loader.Ready
        sourceComponent: root.viewType === "time" ? timeComponent
                       : root.viewType === "phasor" ? phasorComponent
                       : root.viewType === "locus" ? locusComponent
                       : root.viewType === "harmonics" ? harmonicsComponent
                       : tableComponent
    }

    Component {
        id: timeComponent
        TimeSignalsView {
            visible: root.live
            document: root.document
            analysis: root.live ? root.analysis : null
            voltageChannels: root.live ? root.voltageChannels : []
            currentChannels: root.live ? root.currentChannels : []
            otherChannels: root.live ? root.otherChannels : []
            displayedDigitalChannels: root.live ? root.displayedDigitalChannels : []
            digitalDisplayMode: root.digitalDisplayMode
            displayMode: root.timeDisplayMode
            valueRepresentation: root.valueRepresentation
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
            viewStart: root.live ? root.viewStart : 0.0
            visibleDuration: root.live ? root.visibleDuration : 0.0
            cursorATime: root.live ? root.cursorATime : 0.0
            cursorBTime: root.live ? root.cursorBTime : 0.0
            axisWidth: root.axisWidth
            analogTrackHeight: root.analogTrackHeight
            digitalTrackHeight: root.digitalTrackHeight
            onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)
            onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)
            onPanRequested: value => root.panRequested(value)
            onZoomRequested: (factor, anchorFraction) => root.zoomRequested(factor, anchorFraction)
            onDigitalDisplayModeRequested: mode => root.digitalDisplayModeRequested(mode)
        }
    }

    Component {
        id: phasorComponent
        PhasorView {
            visible: root.live
            requestOwner: root.requestOwner && root.live
            document: root.document
            analysis: root.live ? root.analysis : null
            cursorATime: root.live ? root.cursorATime : 0.0
            cursorBTime: root.live ? root.cursorBTime : 0.0
            voltageChannels: root.phasorVoltageChannels
            currentChannels: root.phasorCurrentChannels
            residualChannels: root.residualChannels
        }
    }

    Component {
        id: locusComponent
        LocusView {
            visible: root.live
            document: root.document
            analysis: root.live ? root.locusAnalysis : null
            viewStart: root.live ? root.viewStart : 0.0
            visibleDuration: root.live ? root.visibleDuration : 0.0
            cursorATime: root.live ? root.cursorATime : 0.0
            cursorBTime: root.live ? root.cursorBTime : 0.0
        }
    }

    Component {
        id: harmonicsComponent
        HarmonicsView {
            visible: root.live
            document: root.document
            analysis: root.live ? root.analysis : null
            snapshot: root.live ? root.harmonicSnapshot : null
            visibleChannels: root.live ? root.harmonicChannels : []
            cursorTime: root.live ? root.cursorATime : 0.0
        }
    }

    Component {
        id: tableComponent
        ValueTableView {
            visible: root.live
            document: root.document
            analysis: root.live ? root.analysis : null
            snapshot: root.live ? root.tableSnapshot : null
            visibleChannels: root.live ? root.tableChannels : []
            cursorTime: root.live ? root.cursorATime : 0.0
            valueRepresentation: root.valueRepresentation
            selectedChannel: root.selectedChannel
            onSignalActivated: channelIndex => root.signalActivated(channelIndex)
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 240
        height: 58
        radius: 3
        visible: root.live && root.hasRecord && viewLoader.status === Loader.Loading
        color: "#f8f9fa"
        border.color: "#c9cfd4"
        z: 20

        Row {
            anchors.centerIn: parent
            spacing: 10
            BusyIndicator { width: 22; height: 22; running: parent.parent.visible }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "Preparing " + root.viewType.toUpperCase() + "…"
                color: "#4d5962"
                font.pixelSize: 10
            }
        }
    }
}
