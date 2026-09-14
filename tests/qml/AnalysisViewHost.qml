// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    objectName: "r4AnalysisHost"

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

    readonly property bool dualCursorView: viewType === "time" || viewType === "phasor" || viewType === "locus"
    readonly property bool singleCursorView: viewType === "harmonics" || viewType === "table"
    readonly property bool localCursorVisible: hasRecord && live && (dualCursorView || singleCursorView)
    readonly property int localCursorCount: dualCursorView ? 2 : (singleCursorView ? 1 : 0)
    readonly property int presentedCursorCount: localCursorVisible ? localCursorCount : 0
    readonly property bool presentsCursorA: localCursorCount >= 1
    readonly property bool presentsCursorB: localCursorCount === 2
    readonly property bool heavyActive: hasRecord && live && requestOwner

    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)
    signal panRequested(real panFraction)
    signal zoomRequested(real factor, real anchorFraction)
    signal digitalDisplayModeRequested(string mode)
    signal signalActivated(int channelIndex)

    function requestCursorA(timeSeconds) { cursorARequested(timeSeconds) }
    function requestCursorB(timeSeconds) { cursorBRequested(timeSeconds) }
}
