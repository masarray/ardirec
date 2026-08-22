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
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28
    property real cursorHoverRadius: 8
    property real cursorSnapRadius: 12
    property var tracePalette: ["#a5751b", "#315b8a", "#2b8b82", "#8a4f63", "#556f32", "#70528d", "#a05935", "#4f6f79"]

    readonly property bool hasRecord: documentController.sampleCount > 1 && documentController.analogCount > 0
    readonly property real fullDuration: documentController.durationSeconds
    readonly property real visibleDuration: fullDuration > 0 ? fullDuration / waveformZoom : 0.0
    readonly property real movableDuration: Math.max(0, fullDuration - visibleDuration)
    readonly property real viewStart: documentController.dataStartSeconds + waveformPan * movableDuration
    readonly property real viewEnd: viewStart + visibleDuration

    function clamp(value, lo, hi) {
        return Math.max(lo, Math.min(hi, value))
    }

    function timeAtFraction(fraction) {
        return viewStart + clamp(fraction, 0, 1) * visibleDuration
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
        signalFlick.contentY = 0
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
            hasRecord: window.hasRecord
            onOpenRequested: openDialog.open()
            onSignalsRequested: signalDrawer.open()
            onFitRequested: window.fitRecord()
            onTriggerRequested: window.focusTrigger()
            onZoomInRequested: window.zoomAround(1.4, 0.5)
            onZoomOutRequested: window.zoomAround(1.0 / 1.4, 0.5)
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

        Rectangle {
            id: analysisArea
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
                      : "Open a COMTRADE CFG/DAT record to begin waveform analysis"
                color: documentController.error.length ? "#a62a2a" : "#666666"
                font.pixelSize: 12
            }

            Column {
                anchors.fill: parent
                visible: window.hasRecord
                spacing: 0

                TimeRuler {
                    id: timeRuler
                    width: parent.width
                    height: 30
                    document: documentController
                    viewStart: window.viewStart
                    visibleDuration: window.visibleDuration
                    cursorATime: window.cursorATime
                    cursorBTime: window.cursorBTime
                    axisWidth: window.axisWidth
                }

                Flickable {
                    id: signalFlick
                    width: parent.width
                    height: parent.height - timeRuler.height
                    clip: true
                    contentWidth: width
                    contentHeight: signalStack.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: false

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        width: 12
                    }

                    Column {
                        id: signalStack
                        width: signalFlick.width
                        spacing: 0

                        EventStrip {
                            width: parent.width
                            height: 46
                            document: documentController
                            viewStart: window.viewStart
                            visibleDuration: window.visibleDuration
                            cursorATime: window.cursorATime
                            cursorBTime: window.cursorBTime
                            axisWidth: window.axisWidth
                        }

                        SectionHeader {
                            width: parent.width
                            height: window.voltageChannels.length ? 25 : 0
                            visible: window.voltageChannels.length > 0
                            title: "VOLTAGE"
                            count: window.voltageChannels.length
                            detail: "instantaneous"
                            accent: "#a5751b"
                        }
                        Repeater {
                            model: window.voltageChannels
                            TrackView {
                                required property int index
                                required property int modelData
                                width: signalStack.width
                                height: window.analogTrackHeight
                                document: documentController
                                channelIndex: modelData
                                zoomFactor: window.waveformZoom
                                panFraction: window.waveformPan
                                viewStart: window.viewStart
                                visibleDuration: window.visibleDuration
                                cursorATime: window.cursorATime
                                cursorBTime: window.cursorBTime
                                axisWidth: window.axisWidth
                                traceColor: window.tracePalette[index % 3]
                            }
                        }

                        SectionHeader {
                            width: parent.width
                            height: window.currentChannels.length ? 25 : 0
                            visible: window.currentChannels.length > 0
                            title: "CURRENT"
                            count: window.currentChannels.length
                            detail: "instantaneous"
                            accent: "#315b8a"
                        }
                        Repeater {
                            model: window.currentChannels
                            TrackView {
                                required property int index
                                required property int modelData
                                width: signalStack.width
                                height: window.analogTrackHeight
                                document: documentController
                                channelIndex: modelData
                                zoomFactor: window.waveformZoom
                                panFraction: window.waveformPan
                                viewStart: window.viewStart
                                visibleDuration: window.visibleDuration
                                cursorATime: window.cursorATime
                                cursorBTime: window.cursorBTime
                                axisWidth: window.axisWidth
                                traceColor: window.tracePalette[index % 3]
                            }
                        }

                        SectionHeader {
                            width: parent.width
                            height: window.otherChannels.length ? 25 : 0
                            visible: window.otherChannels.length > 0
                            title: "OTHER ANALOG"
                            count: window.otherChannels.length
                            detail: "recorded channels"
                            accent: "#70528d"
                        }
                        Repeater {
                            model: window.otherChannels
                            TrackView {
                                required property int index
                                required property int modelData
                                width: signalStack.width
                                height: window.analogTrackHeight
                                document: documentController
                                channelIndex: modelData
                                zoomFactor: window.waveformZoom
                                panFraction: window.waveformPan
                                viewStart: window.viewStart
                                visibleDuration: window.visibleDuration
                                cursorATime: window.cursorATime
                                cursorBTime: window.cursorBTime
                                axisWidth: window.axisWidth
                                traceColor: window.tracePalette[(index + 5) % window.tracePalette.length]
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: documentController.digitalCount > 0 ? 30 : 0
                            visible: documentController.digitalCount > 0
                            color: "#eceff2"
                            border.color: "#c8cdd2"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 12
                                spacing: 7

                                Rectangle { width: 3; height: 13; radius: 1; color: "#d5942b" }
                                Label {
                                    text: "DIGITAL EVENTS"
                                    color: "#30363c"
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 0.7
                                }
                                Label {
                                    text: window.displayedDigitalChannels.length + " / " + documentController.digitalCount
                                    color: "#6c757d"
                                    font.pixelSize: 8
                                }
                                Label {
                                    text: documentController.activeDigitalCount + " active"
                                    color: "#7b8187"
                                    font.pixelSize: 8
                                }
                                Item { Layout.fillWidth: true }
                                ToolButton {
                                    text: "Active"
                                    checkable: true
                                    checked: window.digitalDisplayMode === "active"
                                    font.pixelSize: 8
                                    onClicked: window.digitalDisplayMode = "active"
                                }
                                ToolButton {
                                    text: "All"
                                    checkable: true
                                    checked: window.digitalDisplayMode === "all"
                                    font.pixelSize: 8
                                    onClicked: window.digitalDisplayMode = "all"
                                }
                            }
                        }

                        Repeater {
                            model: window.displayedDigitalChannels
                            DigitalTrackView {
                                required property int modelData
                                width: signalStack.width
                                height: window.digitalTrackHeight
                                document: documentController
                                channelIndex: modelData
                                zoomFactor: window.waveformZoom
                                panFraction: window.waveformPan
                                viewStart: window.viewStart
                                visibleDuration: window.visibleDuration
                                cursorATime: window.cursorATime
                                cursorBTime: window.cursorBTime
                                axisWidth: window.axisWidth
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 14
                            color: "#f4f5f6"
                        }
                    }
                }
            }

            MouseArea {
                id: analysisMouse
                visible: window.hasRecord && window.visibleChannels.length > 0
                x: window.axisWidth
                y: 30
                width: Math.max(1, analysisArea.width - window.axisWidth - 14)
                height: Math.max(1, analysisArea.height - 30)
                z: 50
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                preventStealing: true

                property bool movingCursorA: false
                property bool movingCursorB: false
                property bool panning: false
                property bool hoverCursorA: false
                property bool hoverCursorB: false
                property real pressX: 0
                property real panStartTime: 0

                cursorShape: movingCursorA || movingCursorB || hoverCursorA || hoverCursorB
                             ? Qt.SizeHorCursor
                             : (panning ? Qt.ClosedHandCursor : Qt.ArrowCursor)

                function cursorPixel(timeSeconds) {
                    return (timeSeconds - window.viewStart)
                           / Math.max(1e-12, window.visibleDuration) * width
                }

                function updateCursorHover(mouseX) {
                    const ax = cursorPixel(window.cursorATime)
                    const bx = cursorPixel(window.cursorBTime)
                    hoverCursorA = window.cursorATime >= window.viewStart
                                   && window.cursorATime <= window.viewEnd
                                   && Math.abs(mouseX - ax) <= window.cursorHoverRadius
                    hoverCursorB = window.cursorBTime >= window.viewStart
                                   && window.cursorBTime <= window.viewEnd
                                   && Math.abs(mouseX - bx) <= window.cursorHoverRadius
                }

                function snapCursorTime(targetTime) {
                    const clamped = window.clamp(targetTime,
                                                 documentController.dataStartSeconds,
                                                 documentController.dataEndSeconds)
                    if (documentController.digitalCount <= 0 || window.visibleDuration <= 0) return clamped
                    const thresholdSeconds = window.visibleDuration
                                             * window.cursorSnapRadius / Math.max(1, width)
                    return documentController.snapToDigitalEdge(clamped, thresholdSeconds)
                }

                onPressed: mouse => {
                    const fraction = window.clamp(mouse.x / Math.max(1, width), 0, 1)
                    const targetTime = window.timeAtFraction(fraction)
                    movingCursorA = false
                    movingCursorB = false
                    panning = false
                    updateCursorHover(mouse.x)

                    if (mouse.button === Qt.RightButton) {
                        window.cursorBTime = snapCursorTime(targetTime)
                        movingCursorB = true
                        hoverCursorB = true
                        return
                    }

                    const ax = cursorPixel(window.cursorATime)
                    const bx = cursorPixel(window.cursorBTime)
                    const distanceA = Math.abs(mouse.x - ax)
                    const distanceB = Math.abs(mouse.x - bx)
                    if (distanceA <= window.cursorHoverRadius || distanceB <= window.cursorHoverRadius) {
                        if (distanceA <= distanceB) movingCursorA = true
                        else movingCursorB = true
                    } else {
                        panning = true
                        pressX = mouse.x
                        panStartTime = window.viewStart
                    }
                }

                onPositionChanged: mouse => {
                    updateCursorHover(mouse.x)
                    if (!(mouse.buttons & (Qt.LeftButton | Qt.RightButton))) return
                    const fraction = window.clamp(mouse.x / Math.max(1, width), 0, 1)
                    const targetTime = window.timeAtFraction(fraction)
                    if (movingCursorA) {
                        window.cursorATime = snapCursorTime(targetTime)
                    } else if (movingCursorB) {
                        window.cursorBTime = snapCursorTime(targetTime)
                    } else if (panning && window.waveformZoom > 1.0) {
                        const deltaTime = (mouse.x - pressX) / Math.max(1, width) * window.visibleDuration
                        const desiredStart = panStartTime - deltaTime
                        window.waveformPan = window.movableDuration > 0
                                             ? window.clamp((desiredStart - documentController.dataStartSeconds)
                                                            / window.movableDuration, 0, 1)
                                             : 0
                    }
                }

                onReleased: mouse => {
                    movingCursorA = false
                    movingCursorB = false
                    panning = false
                    updateCursorHover(mouse.x)
                }

                onCanceled: {
                    movingCursorA = false
                    movingCursorB = false
                    panning = false
                    hoverCursorA = false
                    hoverCursorB = false
                }

                onExited: {
                    if (!movingCursorA && !movingCursorB) {
                        hoverCursorA = false
                        hoverCursorB = false
                    }
                }

                onWheel: wheel => {
                    if (wheel.modifiers & Qt.ControlModifier) {
                        const factor = wheel.angleDelta.y > 0 ? 1.4 : (1.0 / 1.4)
                        window.zoomAround(factor, window.clamp(wheel.x / Math.max(1, width), 0, 1))
                    } else {
                        const maxY = Math.max(0, signalFlick.contentHeight - signalFlick.height)
                        signalFlick.contentY = window.clamp(signalFlick.contentY - wheel.angleDelta.y * 0.75, 0, maxY)
                    }
                    wheel.accepted = true
                }
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
                    text: "Wheel scrolls signals · Ctrl+wheel zooms time · drag cursor ↔ snaps to digital edges · right-click places Cursor 2"
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
