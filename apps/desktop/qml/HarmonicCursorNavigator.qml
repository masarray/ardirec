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
    property real cursorTime: 0.0
    property real axisWidth: 170
    property real hoverRadius: 8
    property real snapRadius: 12
    property color cursorColor: "#244f9e"

    signal cursorRequested(real timeSeconds)

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
        Column {
            anchors.centerIn: parent
            spacing: 1
            Label { text: "HARMONIC CURSOR"; color: "#4c545b"; font.pixelSize: 8; font.weight: Font.DemiBold }
            Label { text: "1-cycle trailing DFT"; color: "#747d84"; font.pixelSize: 7 }
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
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; width: 1; height: 6; color: "#788087" }
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
            lineOpacity: 0.62
        }

        Rectangle {
            visible: root.cursorTime >= root.viewStart && root.cursorTime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.cursorTime) - 4
            anchors.bottom: parent.bottom
            width: 8
            height: 8
            color: root.cursorColor
            rotation: 45
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            property bool moving: false
            property bool nearCursor: false
            cursorShape: moving || nearCursor ? Qt.SizeHorCursor : Qt.ArrowCursor

            function updateHover(mouseX) {
                nearCursor = root.cursorTime >= root.viewStart
                             && root.cursorTime <= root.viewStart + root.visibleDuration
                             && Math.abs(mouseX - root.pixelForTime(root.cursorTime)) <= root.hoverRadius
            }
            onPressed: mouse => {
                moving = true
                root.cursorRequested(root.snapTime(root.timeForPixel(mouse.x)))
                updateHover(mouse.x)
            }
            onPositionChanged: mouse => {
                updateHover(mouse.x)
                if (moving && (mouse.buttons & Qt.LeftButton))
                    root.cursorRequested(root.snapTime(root.timeForPixel(mouse.x)))
            }
            onReleased: mouse => { moving = false; updateHover(mouse.x) }
            onCanceled: { moving = false; nearCursor = false }
            onExited: { if (!moving) nearCursor = false }
        }
    }
}
