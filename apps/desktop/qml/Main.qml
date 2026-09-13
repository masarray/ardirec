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
    property var configuredDigitalChannels: []
    property var displayedDigitalChannels: []
    property var phasorVoltageChannels: []
    property var phasorCurrentChannels: []
    property var phasorResidualChannels: []
    property var harmonicChannels: []
    property var tableChannels: []
    property int measurementChannel: -1
    property int maximumTracks: 24
    property string digitalDisplayMode: "active"
    property string viewMode: "time"
    property string timeDisplayMode: "instantaneous"
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28

    readonly property bool hasRecord: documentController.sampleCount > 1 && documentController.analogCount > 0
    readonly property real fullDuration: documentController.durationSeconds
    readonly property real visibleDuration: fullDuration > 0 ? fullDuration / waveformZoom : 0.0
    readonly property real movableDuration: Math.max(0, fullDuration - visibleDuration)
    readonly property real viewStart: documentController.dataStartSeconds + waveformPan * movableDuration
    readonly property real viewEnd: viewStart + visibleDuration

    menuBar: WorkstationMenuBar { actions: appActions }

    AppActions {
        id: appActions
        hasRecord: window.hasRecord
        diagnosticCount: documentController.diagnosticCount
        currentView: window.viewMode
        timeDisplayMode: window.timeDisplayMode
        valueRepresentation: documentController.valueRepresentation
        fullScreen: window.visibility === Window.FullScreen

        onOpenRequested: openDialog.open()
        onOpenRecentRequested: url => {
            appActions.noteRecentFile(url)
            documentController.openCfg(url)
        }
        onPropertiesRequested: topBar.showProperties()
        onDiagnosticsRequested: topBar.showDiagnostics()
        onSignalsRequested: signalDrawer.open()
        onFitRequested: window.fitRecord()
        onTriggerRequested: window.focusTrigger()
        onZoomInRequested: window.zoomAround(1.4, 0.5)
        onZoomOutRequested: window.zoomAround(1.0 / 1.4, 0.5)
        onViewRequested: viewName => window.viewMode = viewName
        onWaveformModeRequested: mode => window.timeDisplayMode = mode
        onValueRepresentationRequested: representation => documentController.setValueRepresentation(representation)
        onFullScreenRequested: {
            if (window.visibility === Window.FullScreen) window.showNormal()
            else window.showFullScreen()
        }
        onAboutRequested: topBar.showAbout()
    }

    function clamp(value, lo, hi) { return Math.max(lo, Math.min(hi, value)) }
    function viewLabel() {
        if (viewMode === "phasor") return "PHASOR · C1/C2"
        if (viewMode === "locus") return "LOCUS / R-X"
        if (viewMode === "harmonics") return "HARMONICS"
        if (viewMode === "table") return "ENGINEERING TABLE"
        return timeDisplayMode === "rms" ? "TIME SIGNALS · RMS" : "TIME SIGNALS · INSTANT"
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

    function allDigitalChannels() {
        let result = []
        for (let i = 0; i < documentController.digitalCount; ++i) result.push(i)
        return result
    }

    function defaultPhasorGroup(role) {
        let result = []
        for (let i = 0; i < documentController.analogCount; ++i)
            if (documentController.analogRole(i) === role) result.push(i)
        return result
    }

    function signalLooksResidual(index) {
        const phase = documentController.channelPhase(index)
        const name = documentController.channelName(index).trimmed().toUpperCase().replace(/[^A-Z0-9]/g, "")
        return phase === "E" || name.indexOf("3I0") >= 0 || name.indexOf("3U0") >= 0
                || name.indexOf("3V0") >= 0 || name === "I0" || name === "U0" || name === "V0"
                || name.endsWith("IN") || name.endsWith("UN") || name.endsWith("VN")
    }

    function defaultResidualGroup() {
        let result = []
        for (let i = 0; i < documentController.analogCount; ++i)
            if (signalLooksResidual(i)) result.push(i)
        return result
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
        const configured = configuredDigitalChannels || []
        for (let channel of configured) {
            if (channel < 0 || channel >= documentController.digitalCount) continue
            if (digitalDisplayMode === "all" || documentController.digitalIsActive(channel)) result.push(channel)
        }
        if (digitalDisplayMode === "active" && result.length === 0 && configured.length > 0)
            result = configured.slice()
        displayedDigitalChannels = result
    }

    function initializeRecord() {
        waveformZoom = 1.0
        waveformPan = 0.0
        visibleChannels = defaultVisibleChannels()
        configuredDigitalChannels = allDigitalChannels()
        phasorVoltageChannels = defaultPhasorGroup("Voltage")
        phasorCurrentChannels = defaultPhasorGroup("Current")
        phasorResidualChannels = defaultResidualGroup()
        harmonicChannels = visibleChannels.slice()
        tableChannels = visibleChannels.slice()
        measurementChannel = visibleChannels.length ? visibleChannels[0] : -1
        rebuildAnalogGroups()
        rebuildDigitalGroup()
        locusAnalysisProxy.invalidate()
        deferredViews.resetForRecord()
        focusTrigger()
        timeSignals.resetScroll()
    }

    function applySignalConfiguration(timeChannels, digitalChannels, voltageGroup, currentGroup,
                                      residualGroup, harmonicsGroup, tableGroup) {
        visibleChannels = timeChannels.slice(0, maximumTracks)
        configuredDigitalChannels = digitalChannels.slice()
        phasorVoltageChannels = voltageGroup.slice()
        phasorCurrentChannels = currentGroup.slice()
        phasorResidualChannels = residualGroup.slice()
        harmonicChannels = harmonicsGroup.slice()
        tableChannels = tableGroup.slice()
        rebuildAnalogGroups()
        rebuildDigitalGroup()
        if (measurementChannel < 0 || (tableChannels.indexOf(measurementChannel) < 0 && visibleChannels.indexOf(measurementChannel) < 0))
            measurementChannel = tableChannels.length ? tableChannels[0] : (visibleChannels.length ? visibleChannels[0] : -1)
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

    onDigitalDisplayModeChanged: rebuildDigitalGroup()

    FileDialog {
        id: openDialog
        title: "Open COMTRADE configuration"
        nameFilters: ["COMTRADE configuration (*.cfg *.CFG)"]
        onAccepted: {
            appActions.noteRecentFile(selectedFile.toString())
            documentController.openCfg(selectedFile)
        }
    }

    Connections {
        target: documentController
        function onDocumentChanged() {
            if (documentController.sampleCount > 1) initializeRecord()
        }
    }

    Drawer {
        id: signalDrawer
        edge: Qt.RightEdge
        width: Math.min(1220, window.width * 0.92)
        height: window.height
        modal: true
        interactive: true
        onOpened: signalMatrix.reloadConfiguration()

        SignalSidebar {
            id: signalMatrix
            anchors.fill: parent
            document: documentController
            analysis: analysisController
            analogCount: documentController.analogCount
            digitalCount: documentController.digitalCount
            visibleChannels: window.visibleChannels
            configuredDigitalChannels: window.configuredDigitalChannels
            phasorVoltageChannels: window.phasorVoltageChannels
            phasorCurrentChannels: window.phasorCurrentChannels
            phasorResidualChannels: window.phasorResidualChannels
            harmonicChannels: window.harmonicChannels
            tableChannels: window.tableChannels
            maximumTracks: window.maximumTracks
            onConfigurationApplied: (timeChannels, digitalChannels, voltageGroup, currentGroup, residualGroup, harmonicsGroup, tableGroup) =>
                                        window.applySignalConfiguration(timeChannels, digitalChannels, voltageGroup, currentGroup,
                                                                        residualGroup, harmonicsGroup, tableGroup)
            onCloseRequested: signalDrawer.close()
        }
    }

    LocusAnalysisProxy {
        id: locusAnalysisProxy
        source: analysisController
        document: documentController
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar
            Layout.fillWidth: true
            document: documentController
            actions: appActions
            recordTitle: documentController.title
            currentViewLabel: window.viewLabel()
            hasRecord: window.hasRecord
        }

        ViewModeBar {
            Layout.fillWidth: true
            actions: appActions
            currentView: window.viewMode
            timeDisplayMode: window.timeDisplayMode
            valueRepresentation: documentController.valueRepresentation
            ratioSummary: documentController.transformerRatioSummary
            transformerRatiosAvailable: documentController.transformerRatiosAvailable
            hasRecord: window.hasRecord
        }

        CursorNavigator {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 32 : 0
            visible: window.hasRecord && window.viewMode !== "harmonics" && window.viewMode !== "table"
            document: documentController
            viewStart: window.viewStart
            visibleDuration: window.visibleDuration
            cursorATime: window.cursorATime
            cursorBTime: window.cursorBTime
            axisWidth: window.axisWidth
            onCursorARequested: timeSeconds => window.cursorATime = timeSeconds
            onCursorBRequested: timeSeconds => window.cursorBTime = timeSeconds
        }

        HarmonicCursorNavigator {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 36 : 0
            visible: window.hasRecord && (window.viewMode === "harmonics" || window.viewMode === "table")
            document: documentController
            viewStart: window.viewStart
            visibleDuration: window.visibleDuration
            cursorTime: window.cursorATime
            axisWidth: window.axisWidth
            labelText: window.viewMode === "table" ? "TABLE CURSOR" : "HARMONIC CURSOR"
            detailText: window.viewMode === "table" ? "1-cycle engineering snapshot" : "1-cycle trailing DFT"
            onCursorRequested: timeSeconds => window.cursorATime = timeSeconds
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
                visible: !window.hasRecord && !documentController.loading
                text: documentController.error.length
                      ? documentController.error
                      : "Open a COMTRADE CFG/DAT record to begin disturbance analysis"
                color: documentController.error.length ? "#a62a2a" : "#666666"
                font.pixelSize: 12
            }

            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: !window.hasRecord && documentController.loading
                BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; width: 34; height: 34; running: parent.visible }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: documentController.loadingStatus.length ? documentController.loadingStatus : "Loading COMTRADE…"
                    color: "#4d5962"
                    font.pixelSize: 11
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "The workstation remains responsive while the record is indexed."
                    color: "#7a838a"
                    font.pixelSize: 9
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 8
                width: loadingRow.implicitWidth + 20
                height: 32
                radius: 3
                visible: window.hasRecord && documentController.loading
                color: "#f8f9fa"
                border.color: "#c9cfd4"
                z: 40
                Row {
                    id: loadingRow
                    anchors.centerIn: parent
                    spacing: 7
                    BusyIndicator { width: 18; height: 18; running: parent.parent.visible }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: documentController.loadingStatus
                        color: "#56616a"
                        font.pixelSize: 9
                    }
                }
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
                valueRepresentation: documentController.valueRepresentation
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

            DeferredEngineeringViews {
                id: deferredViews
                anchors.fill: parent
                hasRecord: window.hasRecord
                viewMode: window.viewMode
                document: documentController
                analysis: analysisController
                locusAnalysis: locusAnalysisProxy
                harmonicSnapshot: harmonicSnapshotController
                tableSnapshot: tableSnapshotController
                viewStart: window.viewStart
                visibleDuration: window.visibleDuration
                cursorATime: window.cursorATime
                cursorBTime: window.cursorBTime
                voltageChannels: window.phasorVoltageChannels
                currentChannels: window.phasorCurrentChannels
                residualChannels: window.phasorResidualChannels
                harmonicChannels: window.harmonicChannels
                tableChannels: window.tableChannels
                selectedChannel: window.measurementChannel
                valueRepresentation: documentController.valueRepresentation
                onSignalActivated: channelIndex => {
                    window.measurementChannel = channelIndex
                    documentController.selectChannel(channelIndex)
                }
            }
        }

        WorkstationStatusBar {
            Layout.fillWidth: true
            document: documentController
            hasRecord: window.hasRecord
            viewStart: window.viewStart
            viewEnd: window.viewEnd
            zoomFactor: window.waveformZoom
        }
    }
}
