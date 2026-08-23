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
    property real hoverRadius: 8
    property real snapRadius: 12
    property color cursorAColor: "#2466b3"
    property color cursorBColor: "#c78100"

    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)

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

        Rectangle {
            visible: root.cursorATime >= root.viewStart && root.cursorATime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.cursorATime) - 3
            anchors.bottom: parent.bottom
            width: 7
            height: 7
            color: root.cursorAColor
            rotation: 45
        }
        Rectangle {
            visible: root.cursorBTime >= root.viewStart && root.cursorBTime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.cursorBTime) - 3
            anchors.bottom: parent.bottom
            width: 7
            height: 7
            color: root.cursorBColor
            rotation: 45
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
                const ax = root.pixelForTime(root.cursorATime)
                const bx = root.pixelForTime(root.cursorBTime)
                hoverA = root.cursorATime >= root.viewStart && root.cursorATime <= root.viewStart + root.visibleDuration
                         && Math.abs(mouseX - ax) <= root.hoverRadius
                hoverB = root.cursorBTime >= root.viewStart && root.cursorBTime <= root.viewStart + root.visibleDuration
                         && Math.abs(mouseX - bx) <= root.hoverRadius
            }

            onPressed: mouse => {
                updateHover(mouse.x)
                movingA = false
                movingB = false
                if (mouse.button === Qt.RightButton) {
                    movingB = true
                    root.cursorBRequested(root.snapTime(root.timeForPixel(mouse.x)))
                    return
                }
                const da = Math.abs(mouse.x - root.pixelForTime(root.cursorATime))
                const db = Math.abs(mouse.x - root.pixelForTime(root.cursorBTime))
                if (hoverA || hoverB) {
                    if (da <= db) movingA = true
                    else movingB = true
                } else {
                    movingA = true
                    root.cursorARequested(root.snapTime(root.timeForPixel(mouse.x)))
                }
            }
            onPositionChanged: mouse => {
                updateHover(mouse.x)
                if (!(mouse.buttons & (Qt.LeftButton | Qt.RightButton))) return
                const snapped = root.snapTime(root.timeForPixel(mouse.x))
                if (movingA) root.cursorARequested(snapped)
                else if (movingB) root.cursorBRequested(snapped)
            }
            onReleased: mouse => {
                movingA = false
                movingB = false
                updateHover(mouse.x)
            }
            onCanceled: { movingA = false; movingB = false; hoverA = false; hoverB = false }
            onExited: { if (!movingA && !movingB) { hoverA = false; hoverB = false } }
        }
    }
}
