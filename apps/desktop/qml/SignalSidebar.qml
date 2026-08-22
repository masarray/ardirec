// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#10171e"
    border.color: "#1d2934"
    property var channels: []
    property int analogCount: 0
    property int selectedIndex: -1
    signal channelSelected(int index)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        Label { text: "SIGNALS"; color: "#8fa0af"; font.pixelSize: 10; font.letterSpacing: 1.2 }
        TextField { id: filter; Layout.fillWidth: true; placeholderText: "Filter signals…" }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.channels
            spacing: 2
            delegate: ItemDelegate {
                required property int index
                required property string modelData
                width: ListView.view.width
                height: 32
                text: modelData
                font.pixelSize: 11
                highlighted: index === root.selectedIndex
                enabled: index < root.analogCount
                visible: filter.text.length === 0 || modelData.toLowerCase().includes(filter.text.toLowerCase())
                onClicked: root.channelSelected(index)
                ToolTip.visible: hovered && !enabled
                ToolTip.text: "Digital-track rendering lands next in G1"
            }
        }
        Label {
            visible: root.channels.length === 0
            text: "Open a CFG to list channels"
            color: "#536271"
            font.pixelSize: 10
        }
    }
}
