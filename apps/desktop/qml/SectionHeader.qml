// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#e5ebf0"
    border.color: "#aebcc7"

    // Section divider separates Voltage / Current / Other / Digital groups,
    // visually distinct from both per-lane boundaries and the zero baseline.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 2
        color: "#93a6b5"
    }

    property string title: "SECTION"
    property int count: 0
    property string detail: ""
    property color accent: "#65717d"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8

        Rectangle {
            width: 3
            height: 15
            radius: 1
            color: root.accent
        }
        Label {
            text: root.title
            color: "#30363c"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.letterSpacing: 0.7
        }
        Label {
            text: root.count.toString()
            color: "#6c757d"
            font.pixelSize: 8
        }
        Item { Layout.fillWidth: true }
        Label {
            visible: root.detail.length > 0
            text: root.detail
            color: "#707980"
            font.pixelSize: 8
        }
    }
}
