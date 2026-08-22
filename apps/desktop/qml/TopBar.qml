// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 58
    color: "#111820"
    border.color: "#1d2934"
    property string recordTitle: "No record open"
    property string recordMetadata: ""
    signal openRequested()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 14

        Label { text: "ardirec"; color: "#e4edf5"; font.pixelSize: 18; font.weight: Font.DemiBold }
        Rectangle { width: 1; height: 24; color: "#263644" }
        Button { text: "Open CFG"; onClicked: root.openRequested() }
        ColumnLayout {
            Layout.leftMargin: 8
            spacing: 1
            Label { text: root.recordTitle; color: "#d5e0ea"; font.pixelSize: 12 }
            Label { text: root.recordMetadata; color: "#718090"; font.pixelSize: 10 }
        }
        Item { Layout.fillWidth: true }
        Label { text: "PRIMARY"; color: "#67d7ff"; font.pixelSize: 10; font.letterSpacing: 0.8 }
        Label { text: "50 Hz"; color: "#8393a2"; font.pixelSize: 10 }
    }
}
