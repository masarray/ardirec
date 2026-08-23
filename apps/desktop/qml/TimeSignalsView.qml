// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#ffffff"

    property var document
    property var analysis
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
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28
    property real cursorHoverRadius: 8
    property real cursorSnapRadius: 12

    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)
    signal panRequested(real panFraction)
    signal zoomRequested(real factor, real anchorFraction)
    signal digitalDisplayModeRequested(string mode)

    function clamp(value, lo, hi) { return Math.max(lo, Math.min(hi, value)) }
    function resetScroll() { signalFlick.contentY = 0 }
    function cursorPixel(timeSeconds, plotWidth) {
        return (timeSeconds - root.viewStart) / Math.max(1e-12, root.visibleDuration) * plotWidth
    }
    function snapCursorTime(targetTime, plotWidth) {
        if (!root.document) return targetTime
        const clamped = clamp(targetTime, root.document.dataStartSeconds, root.document.dataEndSeconds)
        if (root.document.digitalCount <= 0 || root.visibleDuration <= 0) return clamped
        const threshold = root.visibleDuration * root.cursorSnapRadius / Math.max(1, plotWidth)
        return root.document.snapToDigitalEdge(clamped, threshold)
    }

    Flickable {
        id: signalFlick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: signalStack.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        interactive: false

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 12 }

        Column {
            id: signalStack
            width: signalFlick.width
            spacing: 0

            EventStrip {
                width: parent.width
                height: 46
                document: root.document
                viewStart: root.viewStart
                visibleDuration: root.visibleDuration
                cursorATime: root.cursorATime
                cursorBTime: root.cursorBTime
                axisWidth: root.axisWidth
            }

            SectionHeader {
                width: parent.width
                height: root.voltageChannels.length ? 25 : 0
                visible: root.voltageChannels.length > 0
                title: "VOLTAGE"
                count: root.voltageChannels.length
                detail: (root.displayMode === "rms" ? "one-cycle RMS" : "instantaneous")
                        + " · " + (root.valueRepresentation === "primary" ? "primary" : "secondary")
                accent: "#d32f2f"
            }
            Repeater {
                model: root.voltageChannels
                TrackView {
                    required property int modelData
                    width: signalStack.width
                    height: root.analogTrackHeight
                    document: root.document
                    analysis: root.analysis
                    channelIndex: modelData
                    zoomFactor: root.zoomFactor
                    panFraction: root.panFraction
                    viewStart: root.viewStart
                    visibleDuration: root.visibleDuration
                    cursorATime: root.cursorATime
                    cursorBTime: root.cursorBTime
                    axisWidth: root.axisWidth
                    displayMode: root.displayMode
                    valueRepresentation: root.valueRepresentation
                    traceColor: root.analysis ? root.analysis.phaseColor(modelData) : "#6f7780"
                }
            }

            SectionHeader {
                width: parent.width
                height: root.currentChannels.length ? 25 : 0
                visible: root.currentChannels.length > 0
                title: "CURRENT"
                count: root.currentChannels.length
                detail: (root.displayMode === "rms" ? "one-cycle RMS" : "instantaneous")
                        + " · " + (root.valueRepresentation === "primary" ? "primary" : "secondary")
                accent: "#1976d2"
            }
            Repeater {
                model: root.currentChannels
                TrackView {
                    required property int modelData
                    width: signalStack.width
                    height: root.analogTrackHeight
                    document: root.document
                    analysis: root.analysis
                    channelIndex: modelData
                    zoomFactor: root.zoomFactor
                    panFraction: root.panFraction
                    viewStart: root.viewStart
                    visibleDuration: root.visibleDuration
                    cursorATime: root.cursorATime
                    cursorBTime: root.cursorBTime
                    axisWidth: root.axisWidth
                    displayMode: root.displayMode
                    valueRepresentation: root.valueRepresentation
                    traceColor: root.analysis ? root.analysis.phaseColor(modelData) : "#6f7780"
                }
            }

            SectionHeader {
                width: parent.width
                height: root.otherChannels.length ? 25 : 0
                visible: root.otherChannels.length > 0
                title: "OTHER ANALOG"
                count: root.otherChannels.length
                detail: (root.displayMode === "rms" ? "one-cycle RMS" : "recorded channels")
                        + " · " + (root.valueRepresentation === "primary" ? "primary" : "secondary")
                accent: "#6f7780"
            }
            Repeater {
                model: root.otherChannels
                TrackView {
                    required property int modelData
                    width: signalStack.width
                    height: root.analogTrackHeight
                    document: root.document
                    analysis: root.analysis
                    channelIndex: modelData
                    zoomFactor: root.zoomFactor
                    panFraction: root.panFraction
                    viewStart: root.viewStart
                    visibleDuration: root.visibleDuration
                    cursorATime: root.cursorATime
                    cursorBTime: root.cursorBTime
                    axisWidth: root.axisWidth
                    displayMode: root.displayMode
                    valueRepresentation: root.valueRepresentation
                    traceColor: root.analysis ? root.analysis.phaseColor(modelData) : "#6f7780"
                }
            }

            Rectangle {
                width: parent.width
                height: root.document && root.document.digitalCount > 0 ? 30 : 0
                visible: root.document && root.document.digitalCount > 0
                color: "#eceff2"
                border.color: "#c8cdd2"

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 12
                    spacing: 7
                    Rectangle { width: 3; height: 13; radius: 1; color: "#d5942b"; anchors.verticalCenter: parent.verticalCenter }
                    Label { text: "DIGITAL EVENTS"; color: "#30363c"; font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 0.7; anchors.verticalCenter: parent.verticalCenter }
                    Label { text: root.displayedDigitalChannels.length + " / " + (root.document ? root.document.digitalCount : 0); color: "#6c757d"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                    Label { text: root.document ? root.document.activeDigitalCount + " active" : ""; color: "#7b8187"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                    Item { width: Math.max(0, parent.width - 430); height: 1 }
                    ToolButton {
                        text: "Active"
                        checkable: true
                        checked: root.digitalDisplayMode === "active"
                        font.pixelSize: 8
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: root.digitalDisplayModeRequested("active")
                    }
                    ToolButton {
                        text: "All"
                        checkable: true
                        checked: root.digitalDisplayMode === "all"
                        font.pixelSize: 8
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: root.digitalDisplayModeRequested("all")
                    }
                }
            }

            Repeater {
                model: root.displayedDigitalChannels
                DigitalTrackView {
                    required property int modelData
                    width: signalStack.width
                    height: root.digitalTrackHeight
                    document: root.document
                    channelIndex: modelData
                    zoomFactor: root.zoomFactor
                    panFraction: root.panFraction
                    viewStart: root.viewStart
                    visibleDuration: root.visibleDuration
                    cursorATime: root.cursorATime
                    cursorBTime: root.cursorBTime
                    axisWidth: root.axisWidth
                }
            }

            Rectangle { width: parent.width; height: 14; color: "#f4f5f6" }
        }
    }

    MouseArea {
        id: analysisMouse
        x: root.axisWidth
        y: 0
        width: Math.max(1, root.width - root.axisWidth - 14)
        height: root.height
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
        property real panStart: 0

        cursorShape: movingCursorA || movingCursorB || hoverCursorA || hoverCursorB
                     ? Qt.SizeHorCursor : (panning ? Qt.ClosedHandCursor : Qt.ArrowCursor)

        function updateCursorHover(mouseX) {
            const ax = root.cursorPixel(root.cursorATime, width)
            const bx = root.cursorPixel(root.cursorBTime, width)
            hoverCursorA = root.cursorATime >= root.viewStart && root.cursorATime <= root.viewStart + root.visibleDuration
                           && Math.abs(mouseX - ax) <= root.cursorHoverRadius
            hoverCursorB = root.cursorBTime >= root.viewStart && root.cursorBTime <= root.viewStart + root.visibleDuration
                           && Math.abs(mouseX - bx) <= root.cursorHoverRadius
        }
        function targetTime(mouseX) {
            const fraction = root.clamp(mouseX / Math.max(1, width), 0, 1)
            return root.viewStart + fraction * root.visibleDuration
        }

        onPressed: mouse => {
            updateCursorHover(mouse.x)
            movingCursorA = false
            movingCursorB = false
            panning = false
            if (mouse.button === Qt.RightButton) {
                movingCursorB = true
                root.cursorBRequested(root.snapCursorTime(targetTime(mouse.x), width))
                return
            }
            const da = Math.abs(mouse.x - root.cursorPixel(root.cursorATime, width))
            const db = Math.abs(mouse.x - root.cursorPixel(root.cursorBTime, width))
            if (hoverCursorA || hoverCursorB) {
                if (da <= db) movingCursorA = true
                else movingCursorB = true
            } else {
                panning = true
                pressX = mouse.x
                panStart = root.panFraction
            }
        }

        onPositionChanged: mouse => {
            updateCursorHover(mouse.x)
            if (!(mouse.buttons & (Qt.LeftButton | Qt.RightButton))) return
            if (movingCursorA) root.cursorARequested(root.snapCursorTime(targetTime(mouse.x), width))
            else if (movingCursorB) root.cursorBRequested(root.snapCursorTime(targetTime(mouse.x), width))
            else if (panning && root.zoomFactor > 1.0) {
                const deltaFraction = (mouse.x - pressX) / Math.max(1, width)
                root.panRequested(root.clamp(panStart - deltaFraction / Math.max(1.0, root.zoomFactor - 1.0), 0, 1))
            }
        }

        onReleased: mouse => { movingCursorA = false; movingCursorB = false; panning = false; updateCursorHover(mouse.x) }
        onCanceled: { movingCursorA = false; movingCursorB = false; panning = false; hoverCursorA = false; hoverCursorB = false }
        onExited: { if (!movingCursorA && !movingCursorB) { hoverCursorA = false; hoverCursorB = false } }

        onWheel: wheel => {
            if (wheel.modifiers & Qt.ControlModifier) {
                const factor = wheel.angleDelta.y > 0 ? 1.4 : (1.0 / 1.4)
                root.zoomRequested(factor, root.clamp(wheel.x / Math.max(1, width), 0, 1))
            } else {
                const maxY = Math.max(0, signalFlick.contentHeight - signalFlick.height)
                signalFlick.contentY = root.clamp(signalFlick.contentY - wheel.angleDelta.y * 0.75, 0, maxY)
            }
            wheel.accepted = true
        }
    }
}
