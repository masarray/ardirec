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

    readonly property real deltaMs: Math.abs(cursorBTime - cursorATime) * 1000.0
    readonly property real deltaCycles: document
                                               ? Math.abs(cursorBTime - cursorATime) * Math.max(1.0, document.nominalFrequency)
                                               : 0.0

    function pixelForTime(timeSeconds) {
        return (timeSeconds - viewStart) / Math.max(1e-12, visibleDuration) * plot.width
    }

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
                color: "#3f4a53"
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.letterSpacing: 0.7
            }
            Label {
                text: "Trigger reference · fixed"
                color: "#6f7880"
                font.pixelSize: 9
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

        TriggerReference {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.document ? root.document.triggerOffsetSeconds : 0)
            height: plot.height
            lineColor: "#7f968c"
            lineOpacity: 0.65
        }

        Label {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: Math.min(plot.width - width - 4,
                        Math.max(4, root.pixelForTime(root.document ? root.document.triggerOffsetSeconds : 0) + 5))
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            text: "Trigger · 0 ms"
            color: "#66766f"
            font.pixelSize: 9
            font.weight: Font.DemiBold
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorATime >= root.viewStart
                     && root.cursorATime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.cursorATime)
            width: 1
            height: plot.height
            color: root.cursorAColor
        }
        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorBTime >= root.viewStart
                     && root.cursorBTime <= root.viewStart + root.visibleDuration
            x: root.pixelForTime(root.cursorBTime)
            width: 1
            height: plot.height
            color: root.cursorBColor
        }

        // The dimension value is an investigation datum, not decoration. Keep the
        // millisecond value visible even when the cursors are too close to contain it.
        Item {
            id: dimension
            anchors.fill: parent
            readonly property real ax: root.pixelForTime(root.cursorATime)
            readonly property real bx: root.pixelForTime(root.cursorBTime)
            readonly property real leftX: Math.min(ax, bx)
            readonly property real rightX: Math.max(ax, bx)
            readonly property real span: Math.max(0, rightX - leftX)
            readonly property real labelWidth: dimensionLabel.implicitWidth + 12
            readonly property bool labelInside: span >= labelWidth + 24
            readonly property bool outsideRight: !labelInside && rightX + 10 + labelWidth <= width
            readonly property real desiredLabelX: labelInside
                                                   ? (leftX + rightX - labelWidth) * 0.5
                                                   : (outsideRight ? rightX + 10 : leftX - 10 - labelWidth)
            readonly property real clampedLabelX: Math.max(2, Math.min(width - labelWidth - 2, desiredLabelX))
            visible: root.visibleDuration > 0
                     && root.cursorATime >= root.viewStart && root.cursorATime <= root.viewStart + root.visibleDuration
                     && root.cursorBTime >= root.viewStart && root.cursorBTime <= root.viewStart + root.visibleDuration
            z: 3

            Rectangle {
                x: dimension.leftX
                y: 15
                width: Math.max(1, dimension.span)
                height: 1
                color: "#626d75"
                opacity: 0.85
            }

            // inward arrow heads remain attached to the two cursor positions.
            Rectangle { x: dimension.leftX; y: 11; width: 8; height: 1.4; rotation: 35; transformOrigin: Item.Left; color: "#626d75" }
            Rectangle { x: dimension.leftX; y: 18; width: 8; height: 1.4; rotation: -35; transformOrigin: Item.Left; color: "#626d75" }
            Rectangle { x: dimension.rightX - 8; y: 11; width: 8; height: 1.4; rotation: -35; transformOrigin: Item.Right; color: "#626d75" }
            Rectangle { x: dimension.rightX - 8; y: 18; width: 8; height: 1.4; rotation: 35; transformOrigin: Item.Right; color: "#626d75" }

            Rectangle {
                id: outsideLeader
                visible: !dimension.labelInside
                x: dimension.outsideRight ? dimension.rightX : labelBox.x + labelBox.width
                y: 15
                width: dimension.outsideRight
                       ? Math.max(0, labelBox.x - dimension.rightX)
                       : Math.max(0, dimension.leftX - (labelBox.x + labelBox.width))
                height: 1
                color: "#626d75"
                opacity: 0.85
            }

            Rectangle {
                id: labelBox
                x: dimension.clampedLabelX
                y: 6
                width: dimension.labelWidth
                height: 18
                radius: 3
                color: "#f8f9fa"
                border.color: "#c9ced2"
                Label {
                    id: dimensionLabel
                    anchors.centerIn: parent
                    text: dimension.span >= 165
                          ? root.deltaMs.toFixed(3) + " ms  ·  " + root.deltaCycles.toFixed(4) + " cyc"
                          : root.deltaMs.toFixed(3) + " ms"
                    color: "#303940"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
