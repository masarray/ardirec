// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root
    width: 1
    property color lineColor: "#7f968c"
    property real segmentHeight: 4
    property real gap: 4
    property real lineOpacity: 0.58

    Repeater {
        model: Math.max(1, Math.ceil(root.height / (root.segmentHeight + root.gap)))
        Rectangle {
            required property int index
            x: 0
            y: index * (root.segmentHeight + root.gap)
            width: 1
            height: Math.min(root.segmentHeight, Math.max(0, root.height - y))
            color: root.lineColor
            opacity: root.lineOpacity
        }
    }
}
