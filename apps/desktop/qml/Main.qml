// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1600
    height: 940
    minimumWidth: 1080
    minimumHeight: 700
    visible: true
    title: "ardirec — COMTRADE Workstation"
    color: "#eef0f2"

    property real waveformZoom: 1.0
    property real waveformPan: 0.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property var visibleChannels: []
    property var voltageChannels: []
    property var currentChannels: []
    property var otherChannels: []
    property var displayedDigitalChannels: []
    property int measurementChannel: -1
    property int maximumTracks: 24
    property string digitalDisplayMode: "active"
    property string viewMode: "time"
    property string timeDisplayMode: "instantaneous"
    property int activeAnalysisCursor: 1
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28

    readonly property bool hasRecord: documentController.sampleCount > 1 && documentController.analogCount > 0
    readonly property real fullDuration: documentController.durationSeconds
    readonly property real visibleDuration: fullDuration > 0 ? fullDuration / waveformZoom : 0.0
    readonly property real movableDuration: Math.max(0, fullDuration - visibleDuration)
    readonly property real viewStart: documentController.dataStartSeconds + waveformPan * movableDuration
    readonly property real viewEnd: viewStart + visibleDuration

    function clamp(value, lo, hi) { return Math.max(lo, Math.min(hi, value)) }
    function viewLabel() {
        if (viewMode === "phasor") return "PHASOR DIAGRAM"
        if (viewMode === "locus") return "LOCUS / R-X"
        if (viewMode === "harmonics") return "HARMONICS"
        if (viewMode === "table") return "VALUE TABLE"
        return timeDisplayMode === "rms" ? "TIME SIGNALS · RMS" : "TIME SIGNALS"
    }

    function defaultVisibleChannels() {
        let preferred = []
        let voltage = []
        let current = []
        for (let i = 0; i < documentController.analogCount; ++i) {
            const role = documentController.analogRole(i)
            if (role === "Voltage") voltage.push(i)
            else if (role === "Current") current.push(i)
        }
        for (let v of voltage) {
            if (preferred.length >= maximumTracks) break
            preferred.push(v)
        }
        for (let c of current) {
            if (preferred.length >= maximumTracks) break
            preferred.push(c)
        }
        if (preferred.length === 0) {
            for (let i = 0; i < Math.min(6, documentController.analogCount); ++i) preferred.push(i)
        }
        return preferred
    }

    function rebuildAnalogGroups() {
        let voltage = []
        let current = []
        let other = []
        for (let channel of visibleChannels) {
            const role = documentController.analogRole(channel)
            if (role === "Voltage") voltage.push(channel)
            else if (role === "Current") current.push(channel)
            else other.push(channel)
        }
        voltageChannels = voltage
        currentChannels = current
        otherChannels = other
    }

    function rebuildDigitalGroup() {
        let result = []
        for (let i = 0; i < documentController.digitalCount; ++i) {
            if (digitalDisplayMode === "all" || documentController.digitalIsActive(i)) result.push(i)
        }
        if (digitalDisplayMode === "active" && result.length === 0) {
            for (let i = 0; i < documentController.digitalCount; ++i) result.push(i)
        }
        displayedDigitalChannels = result
    }

    function initializeRecord() {
        waveformZoom = 1.0
        waveformPan = 0.0
        visibleChannels = defaultVisibleChannels()
        measurementChannel = visibleChannels.length ? visibleChannels[0] : -1
        rebuildAnalogGroups()
        rebuildDigitalGroup()
        focusTrigger()
        timeSignals.resetScroll()
    }

    function fitRecord() {
        if (!hasRecord) return
        waveformZoom = 1.0
        waveformPan = 0.0
        const trigger = clamp(documentController.triggerOffsetSeconds,
                              documentController.dataStartSeconds,
                              documentController.dataEndSeconds)
        const halfCycle = 0.5 / Math.max(1.0, documentController.nominalFrequency)
        cursorATime = clamp(trigger - halfCycle, documentController.dataStartSeconds, documentController.dataEndSeconds)
        cursorBTime = clamp(trigger + halfCycle, documentController.dataStartSeconds, documentController.dataEndSeconds)
    }

    function focusTrigger() {
        if (!hasRecord || fullDuration <= 0) return
        const frequency = documentController.nominalFrequency > 1 ? documentController.nominalFrequency : 50.0
        const targetWindow = Math.min(fullDuration, Math.max(0.12, 8.0 / frequency))
        waveformZoom = clamp(fullDuration / targetWindow, 1.0, 500.0)
        const newVisible = fullDuration / waveformZoom
        const trigger = clamp(documentController.triggerOffsetSeconds,
                              documentController.dataStartSeconds,
                              documentController.dataEndSeconds)
        const desiredStart = trigger - newVisible * 0.45
        const maxStartOffset = Math.max(0, fullDuration - newVisible)
        waveformPan = maxStartOffset > 0
                      ? clamp((desiredStart - documentController.dataStartSeconds) / maxStartOffset, 0, 1)
                      : 0
        const oneCycle = 1.0 / frequency
        cursorATime = clamp(trigger - oneCycle, documentController.dataStartSeconds, documentController.dataEndSeconds)
        cursorBTime = clamp(trigger + oneCycle, documentController.dataStartSeconds, documentController.dataEndSeconds)
    }

    function zoomAround(factor, anchorFraction) {
        if (!hasRecord || fullDuration <= 0) return
        const fraction = clamp(anchorFraction, 0, 1)
        const anchorTime = viewStart + visibleDuration * fraction
        const newZoom = clamp(waveformZoom * factor, 1.0, 500.0)
        const newVisible = fullDuration / newZoom
        const newMovable = Math.max(0, fullDuration - newVisible)
        const desiredStart = anchorTime - newVisible * fraction
        waveformZoom = newZoom
        waveformPan = newMovable > 0
                      ? clamp((desiredStart - documentController.dataStartSeconds) / newMovable, 0, 1)
                      : 0
    }

    function toggleVisibleChannel(index, enabled) {
        if (index < 0 || index >= documentController.analogCount) return
        let next = visibleChannels.slice()
        const position = next.indexOf(index)
        if (enabled && position < 0) {
            if (next.length >= maximumTracks) return
            next.push(index)
        } else if (!enabled && position >= 0) {
            next.splice(position, 1)
        }
        visibleChannels = next
        rebuildAnalogGroups()
        if (measurementChannel < 0 || visibleChannels.indexOf(measurementChannel) < 0)
            measurementChannel = visibleChannels.length ? visibleChannels[0] : -1
    }

    onDigitalDisplayModeChanged: rebuildDigitalGroup()

    FileDialog {
        id: openDialog
        title: "Open COMTRADE configuration"
        nameFilters: ["COMTRADE configuration (*.cfg *.CFG)"]
        onAccepted: documentController.openCfg(selectedFile)
    }

    Shortcut { sequence: StandardKey.Open; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+0"; onActivated: fitRecord() }
    Shortcut { sequence: "1"; onActivated: if (hasRecord) viewMode = "time" }
    Shortcut { sequence: "2"; onActivated: if (hasRecord) viewMode = "phasor" }
    Shortcut { sequence: "3"; onActivated: if (hasRecord) viewMode = "locus" }
    Shortcut { sequence: "4"; onActivated: if (hasRecord) viewMode = "harmonics" }
    Shortcut { sequence: "5"; onActivated: if (hasRecord) viewMode = "table" }

    Connections {
        target: documentController
        function onDocumentChanged() {
            if (documentController.sampleCount > 1) initializeRecord()
        }
    }

    Drawer {
        id: signalDrawer
        edge: Qt.RightEdge
        width: Math.min(360, window.width * 0.34)
        height: window.height
        modal: false
        interactive: true

        SignalSidebar {
            anchors.fill: parent
            channels: documentController.channels
            analogCount: documentController.analogCount
            visibleChannels: window.visibleChannels
            maximumTracks: window.maximumTracks
            onChannelToggled: (index, enabled) => window.toggleVisibleChannel(index, enabled)
            onCloseRequested: signalDrawer.close()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            Layout.fillWidth: true
            recordTitle: documentController.title
            recordMetadata: documentController.metadata
            currentViewLabel: window.viewLabel()
            hasRecord: window.hasRecord
            onOpenRequested: openDialog.open()
            onSignalsRequested: signalDrawer.open()
            onFitRequested: window.fitRecord()
            onTriggerRequested: window.focusTrigger()
            onZoomInRequested: window.zoomAround(1.4, 0.5)
            onZoomOutRequested: window.zoomAround(1.0 / 1.4, 0.5)
        }

        ViewModeBar {
            Layout.fillWidth: true
            currentView: window.viewMode
            timeDisplayMode: window.timeDisplayMode
            activeAnalysisCursor: window.activeAnalysisCursor
            hasRecord: window.hasRecord
            onViewRequested: viewName => window.viewMode = viewName
            onTimeDisplayModeRequested: mode => window.timeDisplayMode = mode
            onActiveAnalysisCursorRequested: cursorNumber => window.activeAnalysisCursor = cursorNumber
        }

        MeasurementPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            document: documentController
            visibleChannels: window.visibleChannels
            measurementChannel: window.measurementChannel
            cursorATime: window.cursorATime
            cursorBTime: window.cursorBTime
            onMeasurementChannelRequested: channelIndex => window.measurementChannel = channelIndex
        }

        CursorNavigator {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            visible: window.hasRecord
            document: documentController
            viewStart: window.viewStart
            visibleDuration: window.visibleDuration
            cursorATime: window.cursorATime
            cursorBTime: window.cursorBTime
            axisWidth: window.axisWidth
            onCursorARequested: timeSeconds => window.cursorATime = timeSeconds
            onCursorBRequested: timeSeconds => window.cursorBTime = timeSeconds
        }

        Rectangle {
            id: workspace
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#aeb4ba"
            clip: true

            Label {
                anchors.centerIn: parent
                visible: !window.hasRecord
                text: documentController.error.length
                      ? documentController.error
                      : "Open a COMTRADE CFG/DAT record to begin disturbance analysis"
                color: documentController.error.length ? "#a62a2a" : "#666666"
                font.pixelSize: 12
            }

            TimeSignalsView {
                id: timeSignals
                anchors.fill: parent
                visible: window.hasRecord && window.viewMode === "time"
                document: documentController
                analysis: analysisController
                voltageChannels: window.voltageChannels
                currentChannels: window.currentChannels
                otherChannels: window.otherChannels
                displayedDigitalChannels: window.displayedDigitalChannels
                digitalDisplayMode: window.digitalDisplayMode
                displayMode: window.timeDisplayMode
                zoomFactor: window.waveformZoom
                panFraction: window.waveformPan
                viewStart: window.viewStart
                visibleDuration: window.visibleDuration
                cursorATime: window.cursorATime
                cursorBTime: window.cursorBTime
                axisWidth: window.axisWidth
                analogTrackHeight: window.analogTrackHeight
                digitalTrackHeight: window.digitalTrackHeight
                onCursorARequested: timeSeconds => window.cursorATime = timeSeconds
                onCursorBRequested: timeSeconds => window.cursorBTime = timeSeconds
                onPanRequested: value => window.waveformPan = value
                onZoomRequested: (factor, anchorFraction) => window.zoomAround(factor, anchorFraction)
                onDigitalDisplayModeRequested: mode => window.digitalDisplayMode = mode
            }

            PhasorView {
                anchors.fill: parent
                visible: window.hasRecord && window.viewMode === "phasor"
                document: documentController
                analysis: analysisController
                cursorATime: window.cursorATime
                cursorBTime: window.cursorBTime
                activeCursor: window.activeAnalysisCursor
            }

            LocusView {
                anchors.fill: parent
                visible: window.hasRecord && window.viewMode === "locus"
                document: documentController
                analysis: analysisController
                viewStart: window.viewStart
                visibleDuration: window.visibleDuration
                cursorATime: window.cursorATime
                cursorBTime: window.cursorBTime
            }

            HarmonicsView {
                anchors.fill: parent
                visible: window.hasRecord && window.viewMode === "harmonics"
                document: documentController
                analysis: analysisController
                channelIndex: window.measurementChannel
                cursorATime: window.cursorATime
                cursorBTime: window.cursorBTime
                activeCursor: window.activeAnalysisCursor
            }

            ValueTableView {
                anchors.fill: parent
                visible: window.hasRecord && window.viewMode === "table"
                document: documentController
                analysis: analysisController
                visibleChannels: window.visibleChannels
                cursorATime: window.cursorATime
                cursorBTime: window.cursorBTime
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: "#ededed"
            border.color: "#bcbcbc"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12

                Label {
                    text: documentController.error.length ? documentController.error : documentController.recordHealth
                    color: documentController.error.length ? "#a62a2a" : "#555555"
                    font.pixelSize: 8
                }
                Rectangle { width: 1; height: 14; color: "#c0c0c0" }
                Label {
                    visible: window.hasRecord
                    text: "View " + ((window.viewStart - documentController.triggerOffsetSeconds) * 1000.0).toFixed(2)
                          + " … " + ((window.viewEnd - documentController.triggerOffsetSeconds) * 1000.0).toFixed(2)
                          + " ms  ·  zoom " + window.waveformZoom.toFixed(2) + "×"
                    color: "#5c5c5c"
                    font.pixelSize: 8
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: window.viewMode === "time"
                          ? "Wheel scrolls signals · Ctrl+wheel zooms time · cursor ↔ snaps to digital edges"
                          : "Drag C1/C2 on the common time ruler · every analysis view follows the cursor context"
                    color: "#666666"
                    font.pixelSize: 8
                }
                Label {
                    text: "ardirec " + Qt.application.version
                    color: "#777777"
                    font.pixelSize: 8
                }
            }
        }
    }
}
