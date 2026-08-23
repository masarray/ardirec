// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f3f4f5"

    property var document
    property var analysis
    property int channelIndex: -1
    property var voltageChannels: []
    property var currentChannels: []
    property var otherChannels: []
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property int activeCursor: 1
    property int maximumOrder: 15
    property string displayMode: "percent"
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"

    readonly property string activeRole: document && channelIndex >= 0 ? document.analogRole(channelIndex) : "Other"
    readonly property var displayedChannels: {
        if (!document || channelIndex < 0) return []
        if (activeRole === "Voltage" && voltageChannels.length) return voltageChannels
        if (activeRole === "Current" && currentChannels.length) return currentChannels
        if (activeRole === "Other" && otherChannels.indexOf(channelIndex) >= 0) return [channelIndex]
        return [channelIndex]
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#e8ebed"
            border.color: "#c4c9cd"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 5

                Label {
                    text: "HARMONIC ANALYSIS"
                    color: "#343c43"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }
                Label {
                    text: root.activeRole.toUpperCase() + " · " + root.displayedChannels.length + " signal" + (root.displayedChannels.length === 1 ? "" : "s")
                    color: "#6d757b"
                    font.pixelSize: 8
                }

                Rectangle { width: 1; height: 20; color: "#c1c6ca"; Layout.leftMargin: 4; Layout.rightMargin: 3 }
                Label { text: "DISPLAY"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                ToolButton {
                    text: "% H1"
                    checkable: true
                    checked: root.displayMode === "percent"
                    font.pixelSize: 8
                    onClicked: root.displayMode = "percent"
                }
                ToolButton {
                    text: "RMS"
                    checkable: true
                    checked: root.displayMode === "rms"
                    font.pixelSize: 8
                    onClicked: root.displayMode = "rms"
                }

                Rectangle { width: 1; height: 20; color: "#c1c6ca"; Layout.leftMargin: 3; Layout.rightMargin: 3 }
                Label { text: "ORDER"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Repeater {
                    model: [15, 25, 50]
                    ToolButton {
                        required property int modelData
                        text: "H" + modelData
                        checkable: true
                        checked: root.maximumOrder === modelData
                        font.pixelSize: 8
                        onClicked: root.maximumOrder = modelData
                    }
                }

                Item { Layout.fillWidth: true }
                Label {
                    text: "C" + root.activeCursor + " active · C1/C2 compared together · "
                          + (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
                    color: root.activeCursor === 1 ? "#244f9e" : "#9a6500"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: "#f5f6f7"
            border.color: "#d3d7da"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                spacing: 10
                Label {
                    text: "H1 RMS and THD stay in the summary rail; the plot focuses on H2…Hn so distortion remains readable."
                    color: "#687078"
                    font.pixelSize: 7
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.displayMode === "percent" ? "Bars: % of each signal's H1" : "Bars: absolute RMS engineering units"
                    color: "#737b81"
                    font.pixelSize: 7
                }
            }
        }

        Flickable {
            id: spectrumFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: spectrumStack.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 11 }

            Column {
                id: spectrumStack
                width: spectrumFlick.width
                spacing: 4
                topPadding: 5
                bottomPadding: 5
                leftPadding: 5
                rightPadding: 16

                Repeater {
                    model: root.displayedChannels
                    HarmonicDiagram {
                        required property int modelData
                        width: spectrumStack.width - spectrumStack.leftPadding - spectrumStack.rightPadding
                        height: 176
                        document: root.document
                        analysis: root.analysis
                        channelIndex: modelData
                        cursorATime: root.cursorATime
                        cursorBTime: root.cursorBTime
                        activeCursor: root.activeCursor
                        maximumOrder: root.maximumOrder
                        displayMode: root.displayMode
                        valueRepresentation: root.valueRepresentation
                    }
                }
            }
        }
    }
}
