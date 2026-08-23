// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import Ardirec.Render 1.0

Rectangle {
    id: root
    color: "#fbfbfb"
    border.color: "#d2d5d8"

    property var document
    property int channelIndex: -1
    property real zoomFactor: 1.0
    property real panFraction: 0.0
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property real axisWidth: 170
    property color activeColor: "#d99a32"
    property color cursorAColor: "#2466b3"
    property color cursorBColor: "#c78100"

    Rectangle {
        id: labelRail
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.axisWidth
        color: "#f6f7f8"
        border.color: "#d0d4d7"

        Row {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 6
            spacing: 6

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 5
                height: 5
                radius: 1
                color: root.document && root.document.digitalIsActive(root.channelIndex)
                       ? root.activeColor : "#b8bdc2"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(20, labelRail.width - 44)
                text: root.document ? root.document.digitalName(root.channelIndex) : "—"
                color: "#30353a"
                elide: Text.ElideRight
                font.pixelSize: 8
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.document ? root.document.digitalStateText(root.channelIndex, root.cursorBTime) : "0"
                color: "#7c838a"
                font.pixelSize: 8
            }
        }
    }

    Item {
        id: chart
        anchors.left: labelRail.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            y: parent.height * 0.78
            height: 1
            color: "#bfc4c8"
        }

        Repeater {
            model: 11
            Rectangle {
                required property int index
                x: chart.width * index / 10
                y: 0
                width: 1
                height: chart.height
                color: "#eceeef"
            }
        }

        DigitalItem {
            anchors.fill: parent
            document: root.document
            channelIndex: root.channelIndex
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
            activeColor: root.activeColor
        }

        TriggerReference {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: (root.document.triggerOffsetSeconds - root.viewStart) / root.visibleDuration * chart.width
            height: chart.height
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorATime >= root.viewStart
                     && root.cursorATime <= root.viewStart + root.visibleDuration
            x: (root.cursorATime - root.viewStart) / root.visibleDuration * chart.width
            width: 1
            height: chart.height
            color: root.cursorAColor
            opacity: 0.85
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorBTime >= root.viewStart
                     && root.cursorBTime <= root.viewStart + root.visibleDuration
            x: (root.cursorBTime - root.viewStart) / root.visibleDuration * chart.width
            width: 1
            height: chart.height
            color: root.cursorBColor
            opacity: 0.85
        }
    }
}
