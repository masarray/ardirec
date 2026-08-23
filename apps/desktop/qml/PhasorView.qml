// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f6f7f8"
    property var document
    property var analysis
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property int activeCursor: 1
    readonly property real cursorTime: activeCursor === 2 ? cursorBTime : cursorATime

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            Label {
                text: "Fundamental phasors at Cursor " + root.activeCursor
                color: "#394149"
                font.pixelSize: 9
                font.weight: Font.DemiBold
            }
            Label {
                text: root.document ? ((root.cursorTime - root.document.triggerOffsetSeconds) * 1000.0).toFixed(3) + " ms" : "—"
                color: root.activeCursor === 1 ? "#2466b3" : "#c78100"
                font.pixelSize: 9
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "Full-cycle DFT · RMS magnitude · common phase reference"
                color: "#747c83"
                font.pixelSize: 8
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 7
            PhasorDiagram {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 360
                document: root.document
                analysis: root.analysis
                role: "Voltage"
                title: "VOLTAGE PHASORS"
                cursorTime: root.cursorTime
            }
            PhasorDiagram {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 360
                document: root.document
                analysis: root.analysis
                role: "Current"
                title: "CURRENT PHASORS"
                cursorTime: root.cursorTime
            }
        }
    }
}
