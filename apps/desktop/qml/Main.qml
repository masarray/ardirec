// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    minimumWidth: 1040
    minimumHeight: 700
    visible: true
    title: "ardirec — COMTRADE Workstation"
    color: "#efefef"

    property real waveformZoom: 1.0
    property real waveformPan: 0.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property var visibleChannels: []
    property int measurementChannel: -1
    property int maximumTracks: 8
    property real axisWidth: 92
    property var tracePalette: ["#9a7b29", "#364d72", "#3f8b86", "#8a4f63", "#556f32", "#70528d", "#a05935", "#4f6f79"]

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
        for (let i = 0; i < documentController.analogCount && preferred.length < 3; ++i) {
            const unit = documentController.channelUnit(i).toUpperCase()
            if (unit === "V" || unit.includes("VOLT")) preferred.push(i)
        }
        if (preferred.length < 3) {
            preferred = []
            for (let i = 0; i < Math.min(3, documentController.analogCount); ++i) preferred.push(i)
        }
        return preferred
    }

    function initializeRecord() {
        waveformZoom = 1.0
        waveformPan = 0.0
        visibleChannels = defaultVisibleChannels()
        measurementChannel = visibleChannels.length ? visibleChannels[0] : -1
        focusTrigger()
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
        if (measurementChannel < 0 || visibleChannels.indexOf(measurementChannel) < 0)
            measurementChannel = visibleChannels.length ? visibleChannels[0] : -1
    }

    FileDialog {
        id: openDialog
        title: "Open COMTRADE configuration"
        nameFilters: ["COMTRADE configuration (*.cfg *.CFG)"]
        onAccepted: documentController.openCfg(selectedFile)
    }

    Shortcut {
        sequence: StandardKey.Open
        onActivated: openDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+0"
        onActivated: fitRecord()
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
        width: Math.min(340, window.width * 0.34)
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
                id: trackStack
                anchors.fill: parent
                visible: window.hasRecord
                spacing: 0

                TimeRuler {
                    id: timeRuler
                    width: parent.width
                    height: 32
                    document: documentController
                    viewStart: window.viewStart
                    visibleDuration: window.visibleDuration
                    cursorATime: window.cursorATime
                    cursorBTime: window.cursorBTime
                    axisWidth: window.axisWidth
                }

                Item {
                    id: tracks
                    width: parent.width
                    height: parent.height - timeRuler.height

                    Column {
                        anchors.fill: parent
                        spacing: 0

                        Repeater {
                            model: window.visibleChannels
                            TrackView {
                                required property int index
                                required property int modelData
                                width: tracks.width
                                height: tracks.height / Math.max(1, window.visibleChannels.length)
                                document: documentController
                                channelIndex: modelData
                                zoomFactor: window.waveformZoom
                                panFraction: window.waveformPan
                                viewStart: window.viewStart
                                visibleDuration: window.visibleDuration
                                cursorATime: window.cursorATime
                                cursorBTime: window.cursorBTime
                                axisWidth: window.axisWidth
                                traceColor: window.tracePalette[index % window.tracePalette.length]
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: window.visibleChannels.length === 0
                        text: "No waveform tracks selected · open Signals to add channels"
                        color: "#686868"
                        font.pixelSize: 11
                    }
                }
            }

            MouseArea {
                id: analysisMouse
                visible: window.hasRecord && window.visibleChannels.length > 0
                x: window.axisWidth
                y: 32
                width: analysisArea.width - window.axisWidth
                height: analysisArea.height - 32
                z: 50
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                preventStealing: true

                property bool movingCursorA: false
                property bool movingCursorB: false
                property bool panning: false
                property real pressX: 0
                property real panStartTime: 0

                onPressed: mouse => {
                    const fraction = window.clamp(mouse.x / Math.max(1, width), 0, 1)
                    const targetTime = window.timeAtFraction(fraction)
                    movingCursorA = false
                    movingCursorB = false
                    panning = false

                    if (mouse.button === Qt.RightButton) {
                        window.cursorBTime = targetTime
                        movingCursorB = true
                        return
                    }

                    const ax = (window.cursorATime - window.viewStart) / Math.max(1e-12, window.visibleDuration) * width
                    const bx = (window.cursorBTime - window.viewStart) / Math.max(1e-12, window.visibleDuration) * width
                    if (Math.abs(mouse.x - ax) <= 9) {
                        movingCursorA = true
                    } else if (Math.abs(mouse.x - bx) <= 9) {
                        movingCursorB = true
                    } else {
                        panning = true
                        pressX = mouse.x
                        panStartTime = window.viewStart
                    }
                }

                onPositionChanged: mouse => {
                    if (!(mouse.buttons & (Qt.LeftButton | Qt.RightButton))) return
                    const fraction = window.clamp(mouse.x / Math.max(1, width), 0, 1)
                    const targetTime = window.timeAtFraction(fraction)
                    if (movingCursorA) {
                        window.cursorATime = window.clamp(targetTime,
                                                         documentController.dataStartSeconds,
                                                         documentController.dataEndSeconds)
                    } else if (movingCursorB) {
                        window.cursorBTime = window.clamp(targetTime,
                                                         documentController.dataStartSeconds,
                                                         documentController.dataEndSeconds)
                    } else if (panning && window.waveformZoom > 1.0) {
                        const deltaTime = (mouse.x - pressX) / Math.max(1, width) * window.visibleDuration
                        const desiredStart = panStartTime - deltaTime
                        window.waveformPan = window.movableDuration > 0
                                             ? window.clamp((desiredStart - documentController.dataStartSeconds)
                                                            / window.movableDuration, 0, 1)
                                             : 0
                    }
                }

                onReleased: {
                    movingCursorA = false
                    movingCursorB = false
                    panning = false
                }

                onWheel: wheel => {
                    const factor = wheel.angleDelta.y > 0 ? 1.4 : (1.0 / 1.4)
                    window.zoomAround(factor, window.clamp(wheel.x / Math.max(1, width), 0, 1))
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
                    text: "Wheel zoom · left-drag pan · drag cursor line · right-click places Cursor 2"
                    color: "#666666"
                    font.pixelSize: 8
                }
                Label {
                    text: "ardirec 0.2.0-alpha.2"
                    color: "#777777"
                    font.pixelSize: 8
                }
            }
        }
    }
}
