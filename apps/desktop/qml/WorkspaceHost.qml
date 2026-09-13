// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property bool hasRecord: false
    property string layoutMode: "single"
    property int activePaneIndex: 0
    property string pane0View: "time"
    property string pane1View: "phasor"
    property string pane2View: "locus"
    property string pane3View: "table"

    property var document
    property var analysis
    property var locusAnalysis
    property var harmonicSnapshot
    property var tableSnapshot
    property var voltageChannels: []
    property var currentChannels: []
    property var otherChannels: []
    property var displayedDigitalChannels: []
    property string digitalDisplayMode: "active"
    property string displayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property real zoomFactor: 1.0
    property real panFraction: 0.0
    property real viewStart: 0.0
    property real visibleDuration: 0.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28
    property var phasorVoltageChannels: []
    property var phasorCurrentChannels: []
    property var residualChannels: []
    property var harmonicChannels: []
    property var tableChannels: []
    property int selectedChannel: -1

    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)
    signal panRequested(real panFraction)
    signal zoomRequested(real factor, real anchorFraction)
    signal digitalDisplayModeRequested(string mode)
    signal signalActivated(int channelIndex)

    readonly property string activeView: paneView(activePaneIndex)
    readonly property int visiblePaneCount: layoutMode === "single" ? 1
                                            : (layoutMode === "split-h" || layoutMode === "split-v" ? 2 : 4)
    readonly property string layoutLabel: layoutMode === "split-h" ? "SPLIT H"
                                          : layoutMode === "split-v" ? "SPLIT V"
                                          : layoutMode === "grid" ? "2×2 GRID"
                                          : layoutMode === "report" ? "REPORT"
                                          : "SINGLE"

    function paneView(index) {
        if (index === 1) return pane1View
        if (index === 2) return pane2View
        if (index === 3) return pane3View
        return pane0View
    }

    function paneVisible(index) {
        return index >= 0 && index < visiblePaneCount
    }

    function setPaneView(index, viewName) {
        const valid = ["time", "phasor", "locus", "harmonics", "table"]
        if (valid.indexOf(viewName) < 0) return
        activePaneIndex = Math.max(0, Math.min(visiblePaneCount - 1, index))
        if (index === 1) pane1View = viewName
        else if (index === 2) pane2View = viewName
        else if (index === 3) pane3View = viewName
        else pane0View = viewName
    }

    function setActiveView(viewName) {
        setPaneView(activePaneIndex, viewName)
    }

    function setLayout(mode) {
        if (["single", "split-h", "split-v", "grid", "report"].indexOf(mode) < 0) return
        const previousView = activeView
        layoutMode = mode
        if (mode === "single") {
            pane0View = previousView
            activePaneIndex = 0
        } else if (mode === "split-h" || mode === "split-v") {
            pane0View = previousView
            if (pane1View === pane0View) pane1View = pane0View === "phasor" ? "time" : "phasor"
            activePaneIndex = Math.min(activePaneIndex, 1)
        } else if (mode === "grid") {
            pane0View = "time"
            pane1View = "phasor"
            pane2View = "locus"
            pane3View = "harmonics"
            activePaneIndex = 0
        } else if (mode === "report") {
            pane0View = "time"
            pane1View = "phasor"
            pane2View = "locus"
            pane3View = "table"
            activePaneIndex = 0
        }
    }

    function resetForRecord() {
        pane0.resetForRecord()
        pane1.resetForRecord()
        pane2.resetForRecord()
        pane3.resetForRecord()
    }

    function resetTimeScrolls() {
        pane0.resetTimeScroll()
        pane1.resetTimeScroll()
        pane2.resetTimeScroll()
        pane3.resetTimeScroll()
    }

    GridLayout {
        anchors.fill: parent
        columns: root.layoutMode === "split-h" || root.layoutMode === "grid" || root.layoutMode === "report" ? 2 : 1
        rowSpacing: 2
        columnSpacing: 2

        WorkspacePane {
            id: pane0
            visible: root.paneVisible(0)
            Layout.fillWidth: true
            Layout.fillHeight: true
            paneIndex: 0
            activePane: root.activePaneIndex === 0
            hasRecord: root.hasRecord
            viewMode: root.pane0View
            document: root.document
            analysis: root.analysis
            locusAnalysis: root.locusAnalysis
            harmonicSnapshot: root.harmonicSnapshot
            tableSnapshot: root.tableSnapshot
            voltageChannels: root.voltageChannels
            currentChannels: root.currentChannels
            otherChannels: root.otherChannels
            displayedDigitalChannels: root.displayedDigitalChannels
            digitalDisplayMode: root.digitalDisplayMode
            displayMode: root.displayMode
            valueRepresentation: root.valueRepresentation
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
            viewStart: root.viewStart
            visibleDuration: root.visibleDuration
            cursorATime: root.cursorATime
            cursorBTime: root.cursorBTime
            axisWidth: root.axisWidth
            analogTrackHeight: root.analogTrackHeight
            digitalTrackHeight: root.digitalTrackHeight
            phasorVoltageChannels: root.phasorVoltageChannels
            phasorCurrentChannels: root.phasorCurrentChannels
            residualChannels: root.residualChannels
            harmonicChannels: root.harmonicChannels
            tableChannels: root.tableChannels
            selectedChannel: root.selectedChannel
            onActivated: root.activePaneIndex = 0
            onViewModeRequested: viewName => root.setPaneView(0, viewName)
            onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)
            onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)
            onPanRequested: value => root.panRequested(value)
            onZoomRequested: (factor, anchorFraction) => root.zoomRequested(factor, anchorFraction)
            onDigitalDisplayModeRequested: mode => root.digitalDisplayModeRequested(mode)
            onSignalActivated: channelIndex => root.signalActivated(channelIndex)
        }

        WorkspacePane {
            id: pane1
            visible: root.paneVisible(1)
            Layout.fillWidth: true
            Layout.fillHeight: true
            paneIndex: 1
            activePane: root.activePaneIndex === 1
            hasRecord: root.hasRecord
            viewMode: root.pane1View
            document: root.document
            analysis: root.analysis
            locusAnalysis: root.locusAnalysis
            harmonicSnapshot: root.harmonicSnapshot
            tableSnapshot: root.tableSnapshot
            voltageChannels: root.voltageChannels
            currentChannels: root.currentChannels
            otherChannels: root.otherChannels
            displayedDigitalChannels: root.displayedDigitalChannels
            digitalDisplayMode: root.digitalDisplayMode
            displayMode: root.displayMode
            valueRepresentation: root.valueRepresentation
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
            viewStart: root.viewStart
            visibleDuration: root.visibleDuration
            cursorATime: root.cursorATime
            cursorBTime: root.cursorBTime
            axisWidth: root.axisWidth
            analogTrackHeight: root.analogTrackHeight
            digitalTrackHeight: root.digitalTrackHeight
            phasorVoltageChannels: root.phasorVoltageChannels
            phasorCurrentChannels: root.phasorCurrentChannels
            residualChannels: root.residualChannels
            harmonicChannels: root.harmonicChannels
            tableChannels: root.tableChannels
            selectedChannel: root.selectedChannel
            onActivated: root.activePaneIndex = 1
            onViewModeRequested: viewName => root.setPaneView(1, viewName)
            onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)
            onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)
            onPanRequested: value => root.panRequested(value)
            onZoomRequested: (factor, anchorFraction) => root.zoomRequested(factor, anchorFraction)
            onDigitalDisplayModeRequested: mode => root.digitalDisplayModeRequested(mode)
            onSignalActivated: channelIndex => root.signalActivated(channelIndex)
        }

        WorkspacePane {
            id: pane2
            visible: root.paneVisible(2)
            Layout.fillWidth: true
            Layout.fillHeight: true
            paneIndex: 2
            activePane: root.activePaneIndex === 2
            hasRecord: root.hasRecord
            viewMode: root.pane2View
            document: root.document
            analysis: root.analysis
            locusAnalysis: root.locusAnalysis
            harmonicSnapshot: root.harmonicSnapshot
            tableSnapshot: root.tableSnapshot
            voltageChannels: root.voltageChannels
            currentChannels: root.currentChannels
            otherChannels: root.otherChannels
            displayedDigitalChannels: root.displayedDigitalChannels
            digitalDisplayMode: root.digitalDisplayMode
            displayMode: root.displayMode
            valueRepresentation: root.valueRepresentation
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
            viewStart: root.viewStart
            visibleDuration: root.visibleDuration
            cursorATime: root.cursorATime
            cursorBTime: root.cursorBTime
            axisWidth: root.axisWidth
            analogTrackHeight: root.analogTrackHeight
            digitalTrackHeight: root.digitalTrackHeight
            phasorVoltageChannels: root.phasorVoltageChannels
            phasorCurrentChannels: root.phasorCurrentChannels
            residualChannels: root.residualChannels
            harmonicChannels: root.harmonicChannels
            tableChannels: root.tableChannels
            selectedChannel: root.selectedChannel
            onActivated: root.activePaneIndex = 2
            onViewModeRequested: viewName => root.setPaneView(2, viewName)
            onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)
            onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)
            onPanRequested: value => root.panRequested(value)
            onZoomRequested: (factor, anchorFraction) => root.zoomRequested(factor, anchorFraction)
            onDigitalDisplayModeRequested: mode => root.digitalDisplayModeRequested(mode)
            onSignalActivated: channelIndex => root.signalActivated(channelIndex)
        }

        WorkspacePane {
            id: pane3
            visible: root.paneVisible(3)
            Layout.fillWidth: true
            Layout.fillHeight: true
            paneIndex: 3
            activePane: root.activePaneIndex === 3
            hasRecord: root.hasRecord
            viewMode: root.pane3View
            document: root.document
            analysis: root.analysis
            locusAnalysis: root.locusAnalysis
            harmonicSnapshot: root.harmonicSnapshot
            tableSnapshot: root.tableSnapshot
            voltageChannels: root.voltageChannels
            currentChannels: root.currentChannels
            otherChannels: root.otherChannels
            displayedDigitalChannels: root.displayedDigitalChannels
            digitalDisplayMode: root.digitalDisplayMode
            displayMode: root.displayMode
            valueRepresentation: root.valueRepresentation
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
            viewStart: root.viewStart
            visibleDuration: root.visibleDuration
            cursorATime: root.cursorATime
            cursorBTime: root.cursorBTime
            axisWidth: root.axisWidth
            analogTrackHeight: root.analogTrackHeight
            digitalTrackHeight: root.digitalTrackHeight
            phasorVoltageChannels: root.phasorVoltageChannels
            phasorCurrentChannels: root.phasorCurrentChannels
            residualChannels: root.residualChannels
            harmonicChannels: root.harmonicChannels
            tableChannels: root.tableChannels
            selectedChannel: root.selectedChannel
            onActivated: root.activePaneIndex = 3
            onViewModeRequested: viewName => root.setPaneView(3, viewName)
            onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)
            onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)
            onPanRequested: value => root.panRequested(value)
            onZoomRequested: (factor, anchorFraction) => root.zoomRequested(factor, anchorFraction)
            onDigitalDisplayModeRequested: mode => root.digitalDisplayModeRequested(mode)
            onSignalActivated: channelIndex => root.signalActivated(channelIndex)
        }
    }
}
