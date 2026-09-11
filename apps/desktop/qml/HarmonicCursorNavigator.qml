// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    height: 36
    color: "#f8f9fa"
    border.color: "#c7ccd1"

    property var document
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorTime: 0.0
    property real axisWidth: 170
    property real hoverRadius: 20
    property real snapRadius: 12
    property color cursorColor: "#244f9e"
    property string labelText: "ANALYSIS CURSOR"
    property string detailText: "1-cycle trailing DFT"

    property real previewTime: cursorTime
    property bool previewActive: false
    property real pendingTime: 0.0
    property bool pendingValid: false
    readonly property real displayedTime: previewActive ? previewTime : cursorTime

    signal cursorRequested(real timeSeconds)

    onCursorTimeChanged: if (!previewActive) previewTime = cursorTime

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
    function queue(timeSeconds) {
        previewTime = timeSeconds
        previewActive = true
        pendingTime = timeSeconds
        pendingValid = true
        if (!commitTimer.running) commitTimer.start()
    }
    function flushPending() {
        if (commitTimer.running) commitTimer.stop()
        if (!pendingValid) return
        const value = pendingTime
        pendingValid = false
        cursorRequested(value)
    }

    Timer {
        id: commitTimer
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
        Column {
            anchors.centerIn: parent
            spacing: 1
            Label { text: root.labelText; color: "#3f4951"; font.pixelSize: 9; font.weight: Font.DemiBold }
            Label { text: root.detailText; color: "#6e7880"; font.pixelSize: 8 }
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
                    font.pixelSize: 9
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

        Item {
            visible: root.displayedTime >= root.viewStart && root.displayedTime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.displayedTime) - width * 0.5
            y: 2
            width: 24
            height: ruler.height - 4
            z: 4

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 4
                anchors.bottom: parent.bottom
                width: 2
                color: root.cursorColor
                opacity: 0.9
            }
            Rectangle {
                anchors.centerIn: parent
                width: 17
                height: 17
                radius: 2
                rotation: 45
                color: root.cursorColor
                border.width: 2
                border.color: "#ffffff"
            }
            Rectangle {
                anchors.centerIn: parent
                width: 19
                height: 19
                radius: 3
                rotation: 45
                color: "transparent"
                border.width: 1
                border.color: "#6d7378"
                opacity: 0.45
            }
            Label {
                anchors.centerIn: parent
                text: "1"
                color: "#ffffff"
                font.pixelSize: 8
                font.weight: Font.Bold
            }
        }

        MouseArea {
            id: cursorMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            property bool moving: false
            property bool nearCursor: false
            cursorShape: moving || nearCursor ? Qt.SizeHorCursor : Qt.ArrowCursor

            function updateHover(mouseX) {
                nearCursor = root.displayedTime >= root.viewStart
                             && root.displayedTime <= root.viewStart + root.visibleDuration
                             && Math.abs(mouseX - root.pixelForTime(root.displayedTime)) <= root.hoverRadius
            }
            function updateCursor(mouseX) {
                root.queue(root.snapTime(root.timeForPixel(mouseX)))
            }

            onPressed: mouse => {
                root.previewTime = root.cursorTime
                root.previewActive = true
                moving = true
                updateCursor(mouse.x)
                updateHover(mouse.x)
            }
            onPositionChanged: mouse => {
                updateHover(mouse.x)
                if (moving && (mouse.buttons & Qt.LeftButton)) updateCursor(mouse.x)
            }
            onReleased: mouse => {
                if (moving) updateCursor(mouse.x)
                root.flushPending()
                moving = false
                root.previewActive = false
                updateHover(mouse.x)
            }
            onCanceled: {
                root.flushPending()
                moving = false
                root.previewActive = false
                nearCursor = false
            }
            onExited: if (!moving) nearCursor = false
        }
    }
}
