// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    height: 32
    color: "#f8f9fa"
    border.color: "#c7ccd1"

    property var document
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property real axisWidth: 170
    property real hoverRadius: 16
    property real snapRadius: 12
    property color cursorAColor: "#2466b3"
    property color cursorBColor: "#c78100"

    // The thumb follows the pointer locally at full input rate. Heavy analysis receives a
    // coalesced cursor update at most once per display frame, then the exact final value on release.
    property real previewATime: cursorATime
    property real previewBTime: cursorBTime
    property bool previewAActive: false
    property bool previewBActive: false
    property real pendingATime: 0.0
    property real pendingBTime: 0.0
    property bool pendingAValid: false
    property bool pendingBValid: false

    readonly property real displayedATime: previewAActive ? previewATime : cursorATime
    readonly property real displayedBTime: previewBActive ? previewBTime : cursorBTime

    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)

    onCursorATimeChanged: if (!previewAActive) previewATime = cursorATime
    onCursorBTimeChanged: if (!previewBActive) previewBTime = cursorBTime

    function clamp(value, lo, hi) { return Math.max(lo, Math.min(hi, value)) }
    function relativeMs(timeSeconds) {
        return document ? (timeSeconds - document.triggerOffsetSeconds) * 1000.0 : 0
    }
    function formatTick(timeSeconds) {
        const value = relativeMs(timeSeconds)
        const spanMs = visibleDuration * 1000.0
        if (spanMs >= 1000) return value.toFixed(0)
        if (spanMs >= 100) return value.toFixed(1)
        if (spanMs >= 10) return value.toFixed(2)
        return value.toFixed(3)
    }
    function pixelForTime(timeSeconds) {
        return (timeSeconds - viewStart) / Math.max(1e-12, visibleDuration) * ruler.width
    }
    function timeForPixel(pixel) {
        return viewStart + clamp(pixel / Math.max(1, ruler.width), 0, 1) * visibleDuration
    }
    function snapTime(targetTime) {
        if (!document) return targetTime
        const clamped = clamp(targetTime, document.dataStartSeconds, document.dataEndSeconds)
        if (document.digitalCount <= 0 || visibleDuration <= 0) return clamped
        const threshold = visibleDuration * snapRadius / Math.max(1, ruler.width)
        return document.snapToDigitalEdge(clamped, threshold)
    }
    function queueA(timeSeconds) {
        previewATime = timeSeconds
        previewAActive = true
        pendingATime = timeSeconds
        pendingAValid = true
        if (!analysisCommitTimer.running) analysisCommitTimer.start()
    }
    function queueB(timeSeconds) {
        previewBTime = timeSeconds
        previewBActive = true
        pendingBTime = timeSeconds
        pendingBValid = true
        if (!analysisCommitTimer.running) analysisCommitTimer.start()
    }
    function flushPending() {
        if (analysisCommitTimer.running) analysisCommitTimer.stop()
        if (pendingAValid) {
            const value = pendingATime
            pendingAValid = false
            cursorARequested(value)
        }
        if (pendingBValid) {
            const value = pendingBTime
            pendingBValid = false
            cursorBRequested(value)
        }
    }

    Timer {
        id: analysisCommitTimer
        interval: 16
        repeat: false
        onTriggered: root.flushPending()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.axisWidth
        color: "#eef1f3"
        border.color: "#c7ccd1"
        Row {
            anchors.centerIn: parent
            spacing: 7
            Label { text: "Time [ms]"; color: "#4c545b"; font.pixelSize: 8; font.weight: Font.DemiBold }
            Label { text: "Trigger = 0"; color: "#7b8882"; font.pixelSize: 8 }
        }
    }

    Item {
        id: ruler
        anchors.left: parent.left
        anchors.leftMargin: root.axisWidth
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        Repeater {
            model: 11
            Item {
                required property int index
                x: ruler.width * index / 10
                width: 1
                height: ruler.height
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; width: 1; height: 7; color: "#788087" }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    text: root.formatTick(root.viewStart + root.visibleDuration * index / 10)
                    color: "#4f575d"
                    font.pixelSize: 8
                }
            }
        }

        TriggerReference {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.document ? root.document.triggerOffsetSeconds : 0)
            height: ruler.height
            lineColor: "#7f968c"
            lineOpacity: 0.65
        }

        Label {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: Math.min(ruler.width - width - 3, Math.max(3, root.pixelForTime(root.document ? root.document.triggerOffsetSeconds : 0) + 4))
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            text: "0"
            color: "#708078"
            font.pixelSize: 7
        }

        Item {
            visible: root.displayedATime >= root.viewStart && root.displayedATime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.displayedATime) - 8
            y: ruler.height - 18
            width: 16
            height: 18
            z: 3
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; width: 2; height: 8; color: root.cursorAColor }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 1
                width: 12
                height: 12
                rotation: 45
                color: root.cursorAColor
                border.width: 1.5
                border.color: "#ffffff"
            }
            Label { anchors.horizontalCenter: parent.horizontalCenter; y: 2; text: "1"; color: "#ffffff"; font.pixelSize: 7; font.weight: Font.DemiBold }
        }

        Item {
            visible: root.displayedBTime >= root.viewStart && root.displayedBTime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.displayedBTime) - 8
            y: ruler.height - 18
            width: 16
            height: 18
            z: 3
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; width: 2; height: 8; color: root.cursorBColor }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 1
                width: 12
                height: 12
                rotation: 45
                color: root.cursorBColor
                border.width: 1.5
                border.color: "#ffffff"
            }
            Label { anchors.horizontalCenter: parent.horizontalCenter; y: 2; text: "2"; color: "#ffffff"; font.pixelSize: 7; font.weight: Font.DemiBold }
        }

        MouseArea {
            id: cursorMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            preventStealing: true
            property bool movingA: false
            property bool movingB: false
            property bool hoverA: false
            property bool hoverB: false

            cursorShape: movingA || movingB || hoverA || hoverB ? Qt.SizeHorCursor : Qt.ArrowCursor

            function updateHover(mouseX) {
                const ax = root.pixelForTime(root.displayedATime)
                const bx = root.pixelForTime(root.displayedBTime)
                hoverA = root.displayedATime >= root.viewStart && root.displayedATime <= root.viewStart + root.visibleDuration
                         && Math.abs(mouseX - ax) <= root.hoverRadius
                hoverB = root.displayedBTime >= root.viewStart && root.displayedBTime <= root.viewStart + root.visibleDuration
                         && Math.abs(mouseX - bx) <= root.hoverRadius
            }

            function updateActiveCursor(mouseX) {
                const snapped = root.snapTime(root.timeForPixel(mouseX))
                if (movingA) root.queueA(snapped)
                else if (movingB) root.queueB(snapped)
            }

            onPressed: mouse => {
                updateHover(mouse.x)
                movingA = false
                movingB = false
                if (mouse.button === Qt.RightButton) {
                    root.previewBTime = root.cursorBTime
                    root.previewBActive = true
                    movingB = true
                    updateActiveCursor(mouse.x)
                    return
                }
                const da = Math.abs(mouse.x - root.pixelForTime(root.displayedATime))
                const db = Math.abs(mouse.x - root.pixelForTime(root.displayedBTime))
                if (hoverA || hoverB) {
                    if (da <= db) {
                        root.previewATime = root.cursorATime
                        root.previewAActive = true
                        movingA = true
                    } else {
                        root.previewBTime = root.cursorBTime
                        root.previewBActive = true
                        movingB = true
                    }
                } else {
                    root.previewATime = root.cursorATime
                    root.previewAActive = true
                    movingA = true
                    updateActiveCursor(mouse.x)
                }
            }

            onPositionChanged: mouse => {
                updateHover(mouse.x)
                if (!(mouse.buttons & (Qt.LeftButton | Qt.RightButton))) return
                updateActiveCursor(mouse.x)
            }

            onReleased: mouse => {
                if (movingA || movingB) updateActiveCursor(mouse.x)
                root.flushPending()
                movingA = false
                movingB = false
                root.previewAActive = false
                root.previewBActive = false
                updateHover(mouse.x)
            }

            onCanceled: {
                root.flushPending()
                movingA = false
                movingB = false
                root.previewAActive = false
                root.previewBActive = false
                hoverA = false
                hoverB = false
            }
            onExited: { if (!movingA && !movingB) { hoverA = false; hoverB = false } }
        }
    }
}
