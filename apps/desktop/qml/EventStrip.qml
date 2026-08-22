// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#f8f9fa"
    border.color: "#c7ccd1"

    property var document
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property real axisWidth: 170
    property color cursorAColor: "#2466b3"
    property color cursorBColor: "#c78100"
    property color triggerColor: "#10a05a"

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.axisWidth
        color: "#f1f3f5"
        border.color: "#c7ccd1"

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Label {
                text: "EVENT"
                color: "#4b5560"
                font.pixelSize: 9
                font.weight: Font.DemiBold
                font.letterSpacing: 0.8
            }
            Label {
                text: "Trigger reference"
                color: "#7a828a"
                font.pixelSize: 8
            }
        }
    }

    Item {
        id: plot
        anchors.left: parent.left
        anchors.leftMargin: root.axisWidth
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 1
            color: "#c5c9cd"
        }

        Repeater {
            model: 11
            Rectangle {
                required property int index
                x: plot.width * index / 10
                width: 1
                height: plot.height
                color: "#e2e5e8"
            }
        }

        Rectangle {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: (root.document.triggerOffsetSeconds - root.viewStart) / root.visibleDuration * plot.width
            width: 2
            height: plot.height
            color: root.triggerColor
        }

        Label {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: Math.min(plot.width - width - 4,
                        Math.max(4, (root.document.triggerOffsetSeconds - root.viewStart)
                                 / root.visibleDuration * plot.width + 5))
            anchors.verticalCenter: parent.verticalCenter
            text: "Trigger  ·  0 ms"
            color: "#167447"
            font.pixelSize: 9
            font.weight: Font.DemiBold
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorATime >= root.viewStart
                     && root.cursorATime <= root.viewStart + root.visibleDuration
            x: (root.cursorATime - root.viewStart) / root.visibleDuration * plot.width
            width: 1
            height: plot.height
            color: root.cursorAColor
        }
        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorBTime >= root.viewStart
                     && root.cursorBTime <= root.viewStart + root.visibleDuration
            x: (root.cursorBTime - root.viewStart) / root.visibleDuration * plot.width
            width: 1
            height: plot.height
            color: root.cursorBColor
        }
    }
}
