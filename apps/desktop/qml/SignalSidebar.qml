// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#10171e"
    border.color: "#1d2934"
    property var channels: []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        Label { text: "SIGNALS"; color: "#8fa0af"; font.pixelSize: 10; font.letterSpacing: 1.2 }
        TextField { Layout.fillWidth: true; placeholderText: "Filter signals…" }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.channels
            delegate: ItemDelegate {
                required property string modelData
                width: ListView.view.width
                text: modelData
                font.pixelSize: 11
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
