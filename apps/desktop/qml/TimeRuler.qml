// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#f8f8f8"
    border.color: "#c2c2c2"

    property var document
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property real axisWidth: 92
    property color cursorAColor: "#244f9e"
    property color cursorBColor: "#b77900"
    property color triggerColor: "#00a846"

    function relativeMs(timeSeconds) {
        if (!document) return 0
        return (timeSeconds - document.triggerOffsetSeconds) * 1000.0
    }

    function formatTick(timeSeconds) {
        const value = relativeMs(timeSeconds)
        const spanMs = visibleDuration * 1000.0
        if (spanMs >= 1000) return value.toFixed(0)
        if (spanMs >= 100) return value.toFixed(1)
        if (spanMs >= 10) return value.toFixed(2)
        return value.toFixed(3)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.axisWidth
        color: "#eeeeee"
        border.color: "#c5c5c5"
        Label {
            anchors.centerIn: parent
            text: "Time in ms"
            color: "#444444"
            font.pixelSize: 9
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
                y: 0
                width: 1
                height: ruler.height
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: 1
                    height: 8
                    color: "#7a7a7a"
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    text: root.formatTick(root.viewStart + root.visibleDuration * index / 10)
                    color: "#4f4f4f"
                    font.pixelSize: 8
                }
            }
        }

        Rectangle {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: (root.document.triggerOffsetSeconds - root.viewStart) / root.visibleDuration * ruler.width - 3
            anchors.bottom: parent.bottom
            width: 7
            height: 7
            color: root.triggerColor
            rotation: 45
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorATime >= root.viewStart
                     && root.cursorATime <= root.viewStart + root.visibleDuration
            x: (root.cursorATime - root.viewStart) / root.visibleDuration * ruler.width - 3
            anchors.bottom: parent.bottom
            width: 7
            height: 7
            color: root.cursorAColor
            rotation: 45
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorBTime >= root.viewStart
                     && root.cursorBTime <= root.viewStart + root.visibleDuration
            x: (root.cursorBTime - root.viewStart) / root.visibleDuration * ruler.width - 3
            anchors.bottom: parent.bottom
            width: 7
            height: 7
            color: root.cursorBColor
            rotation: 45
        }
    }
}
