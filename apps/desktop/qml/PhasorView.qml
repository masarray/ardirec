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
    property var voltageChannels: []
    property var currentChannels: []
    property var residualChannels: []

    readonly property real voltageScale: sharedScale(voltageChannels)
    readonly property real currentScale: sharedScale(currentChannels)
    readonly property real residualScale: sharedScale(residualChannels)
    readonly property var groupModel: [
        { title: "VOLTAGE", subtitle: "phase / neutral voltage vectors", channels: voltageChannels, scale: voltageScale },
        { title: "CURRENT", subtitle: "phase / neutral current vectors", channels: currentChannels, scale: currentScale },
        { title: "RESIDUAL / ZERO-SEQUENCE", subtitle: "recorded IN / UN / I0 / 3I0-style signals", channels: residualChannels, scale: residualScale }
    ]

    function relativeMs(timeSeconds) {
        return root.document ? (timeSeconds - root.document.triggerOffsetSeconds) * 1000.0 : 0.0
    }

    function sharedScale(channels) {
        const representationDependency = root.document ? root.document.valueRepresentation : "secondary"
        if (!root.analysis || !channels || !channels.length) return 0.0
        let maximum = 0.0
        for (let index of channels) {
            const a = root.analysis.phasorAt(index, root.cursorATime)
            const b = root.analysis.phasorAt(index, root.cursorBTime)
            if (a && a.valid && Number.isFinite(a.magnitude)) maximum = Math.max(maximum, a.magnitude)
            if (b && b.valid && Number.isFinite(b.magnitude)) maximum = Math.max(maximum, b.magnitude)
        }
        if (!(maximum > 0.0)) return 0.0
        // Small headroom keeps arrowheads/labels readable while preserving direct C1/C2 magnitude comparison.
        return maximum * 1.06
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#ffffff"
            border.color: "#c7ccd0"
            radius: 2

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                ColumnLayout {
                    spacing: 0
                    Label {
                        text: "Fundamental Phasor Comparison"
                        color: "#30383f"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "Full-cycle DFT · RMS magnitude · same radial scale within each measurement group"
                        color: "#778087"
                        font.pixelSize: 8
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 174
                    Layout.preferredHeight: 30
                    radius: 3
                    color: "#f5f8fd"
                    border.color: "#c4d3e8"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6
                        Rectangle { width: 8; height: 8; radius: 4; color: "#244f9e" }
                        Label { text: "C1"; color: "#244f9e"; font.pixelSize: 9; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: root.relativeMs(root.cursorATime).toFixed(3) + " ms"
                            color: "#26323d"
                            font.pixelSize: 10
                            font.family: "Consolas"
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 174
                    Layout.preferredHeight: 30
                    radius: 3
                    color: "#fdf9f1"
                    border.color: "#e2cfaa"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6
                        Rectangle { width: 8; height: 8; radius: 4; color: "#b77900" }
                        Label { text: "C2"; color: "#9b6900"; font.pixelSize: 9; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: root.relativeMs(root.cursorBTime).toFixed(3) + " ms"
                            color: "#3b3222"
                            font.pixelSize: 10
                            font.family: "Consolas"
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }

        Flickable {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: groupColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: groupColumn
                width: scroller.width
                spacing: 8

                Repeater {
                    model: root.groupModel

                    Rectangle {
                        required property int index
                        required property var modelData
                        width: groupColumn.width
                        height: modelData.channels && modelData.channels.length ? 392 : 0
                        visible: height > 0
                        color: "#eef1f3"
                        border.color: "#c5cbd0"
                        radius: 2

                        Rectangle {
                            id: groupHeader
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 34
                            color: "#e8ecef"
                            border.color: "#c9ced2"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8
                                Label {
                                    text: modelData.title
                                    color: "#30383e"
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 0.6
                                }
                                Label {
                                    text: modelData.subtitle
                                    color: "#798087"
                                    font.pixelSize: 8
                                }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: modelData.channels.length + " signal" + (modelData.channels.length === 1 ? "" : "s")
                                    color: "#697178"
                                    font.pixelSize: 8
                                }
                            }
                        }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: groupHeader.bottom
                            anchors.bottom: parent.bottom
                            anchors.margins: 6
                            spacing: 6

                            PhasorDiagram {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumWidth: 360
                                document: root.document
                                analysis: root.analysis
                                channels: modelData.channels
                                title: "C1 · " + modelData.title
                                cursorTime: root.cursorATime
                                scaleMagnitude: modelData.scale
                                cursorAccent: "#244f9e"
                            }

                            PhasorDiagram {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumWidth: 360
                                document: root.document
                                analysis: root.analysis
                                channels: modelData.channels
                                title: "C2 · " + modelData.title
                                cursorTime: root.cursorBTime
                                scaleMagnitude: modelData.scale
                                cursorAccent: "#b77900"
                            }
                        }
                    }
                }

                Rectangle {
                    width: groupColumn.width
                    height: root.voltageChannels.length || root.currentChannels.length || root.residualChannels.length ? 0 : 120
                    visible: height > 0
                    color: "transparent"
                    Label {
                        anchors.centerIn: parent
                        text: "No phasor groups are assigned. Open Signals → Signal Configuration…"
                        color: "#737b82"
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
