// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f6f7f8"
    property var document
    property var analysis
    property var visibleChannels: []
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"

    function sampleText(channelIndex, timeSeconds) {
        const representationDependency = root.valueRepresentation
        return root.document ? root.document.sampleValueText(channelIndex, timeSeconds) : "—"
    }
    function rmsText(channelIndex, timeSeconds) {
        const representationDependency = root.valueRepresentation
        return root.analysis ? root.analysis.rmsValueText(channelIndex, timeSeconds) : "—"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#e9edf0"
            border.color: "#c5cbd0"
            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                spacing: 0
                Label { width: 180; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Signal"; color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { width: 52; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Phase"; color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "C1 instant"; color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "C1 RMS"; color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "C1 angle"; color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "C2 instant"; color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "C2 RMS"; color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "C2 angle · " + (root.valueRepresentation === "primary" ? "PRI" : "SEC"); color: "#4d565d"; font.pixelSize: 8; font.weight: Font.DemiBold }
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: rows.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: rows
                width: parent.width
                Repeater {
                    model: root.visibleChannels
                    Rectangle {
                        required property int index
                        required property int modelData
                        width: rows.width
                        height: 28
                        color: index % 2 ? "#fafbfc" : "#ffffff"
                        border.color: "#e0e4e7"
                        readonly property var p1: {
                            const representationDependency = root.valueRepresentation
                            return root.analysis ? root.analysis.phasorAt(modelData, root.cursorATime) : ({valid:false})
                        }
                        readonly property var p2: {
                            const representationDependency = root.valueRepresentation
                            return root.analysis ? root.analysis.phasorAt(modelData, root.cursorBTime) : ({valid:false})
                        }
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            Label { width: 180; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.document ? root.document.channelName(modelData) : "—"; color: root.analysis ? root.analysis.phaseColor(modelData) : "#444"; font.pixelSize: 8; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Label { width: 52; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.analysis ? root.analysis.channelPhase(modelData) : "—"; color: "#596168"; font.pixelSize: 8 }
                            Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.sampleText(modelData, root.cursorATime); color: "#343b40"; font.pixelSize: 8 }
                            Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.rmsText(modelData, root.cursorATime); color: "#343b40"; font.pixelSize: 8 }
                            Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: p1.valid ? p1.angle.toFixed(2) + "°" : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.sampleText(modelData, root.cursorBTime); color: "#343b40"; font.pixelSize: 8 }
                            Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.rmsText(modelData, root.cursorBTime); color: "#343b40"; font.pixelSize: 8 }
                            Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: p2.valid ? p2.angle.toFixed(2) + "°" : "—"; color: "#343b40"; font.pixelSize: 8 }
                        }
                    }
                }
            }
        }
    }
}
