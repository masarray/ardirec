// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Ardirec.Render 1.0

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: "ardirec — COMTRADE Workstation"
    color: "#0e1217"

    property real waveformZoom: 1.0
    property real waveformPan: 0.0
    property real cursorA: 0.32
    property real cursorB: 0.68

    function clamp(value, lo, hi) { return Math.max(lo, Math.min(hi, value)) }
    function cursorTime(fraction) {
        const duration = documentController.durationSeconds
        if (duration <= 0) return "—"
        const visible = duration / waveformZoom
        const start = waveformPan * Math.max(0, duration - visible)
        return ((start + fraction * visible) * 1000.0).toFixed(3) + " ms"
    }

    FileDialog {
        id: openDialog
        title: "Open COMTRADE configuration"
        nameFilters: ["COMTRADE configuration (*.cfg)"]
        onAccepted: {
            waveformZoom = 1.0
            waveformPan = 0.0
            documentController.openCfg(selectedFile)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            Layout.fillWidth: true
            recordTitle: documentController.title
            recordMetadata: documentController.metadata
            onOpenRequested: openDialog.open()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            SignalSidebar {
                Layout.preferredWidth: 242
                Layout.fillHeight: true
                channels: documentController.channels
                analogCount: documentController.analogCount
                selectedIndex: documentController.selectedAnalogIndex
                onChannelSelected: documentController.selectChannel(index)
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#0b0f14"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "TIME SIGNALS  ·  " + documentController.selectedSignal; color: "#9aa9b8"; font.pixelSize: 11; font.letterSpacing: 1.0 }
                        Item { Layout.fillWidth: true }
                        Label { text: "A " + cursorTime(cursorA); color: "#67d7ff"; font.pixelSize: 10 }
                        Label { text: "B " + cursorTime(cursorB); color: "#ffbe68"; font.pixelSize: 10 }
                        Label {
                            text: documentController.durationSeconds > 0
                                  ? "Δ " + (Math.abs(cursorB - cursorA) * documentController.durationSeconds / waveformZoom * 1000.0).toFixed(3) + " ms"
                                  : "Δ —"
                            color: "#8393a2"; font.pixelSize: 10
                        }
                    }

                    Rectangle {
                        id: plot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0d1319"
                        border.color: "#1c2833"
                        radius: 4
                        clip: true

                        Repeater {
                            model: 8
                            Rectangle {
                                required property int index
                                x: 0
                                y: plot.height * (index + 1) / 9
                                width: plot.width
                                height: 1
                                color: "#17212b"
                            }
                        }

                        WaveformItem {
                            id: waveform
                            anchors.fill: parent
                            anchors.margins: 16
                            document: documentController
                            zoomFactor: waveformZoom
                            panFraction: waveformPan
                        }

                        Rectangle {
                            x: 16 + cursorA * Math.max(1, plot.width - 32)
                            y: 12
                            width: 1
                            height: plot.height - 24
                            color: "#67d7ff"
                            opacity: 0.9
                        }
                        Rectangle {
                            x: 16 + cursorB * Math.max(1, plot.width - 32)
                            y: 12
                            width: 1
                            height: plot.height - 24
                            color: "#ffbe68"
                            opacity: 0.9
                        }

                        MouseArea {
                            id: interaction
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            property real dragStartX: 0
                            property real panStart: 0
                            property bool movingCursor: false
                            property bool cursorIsA: true
                            onPressed: mouse => {
                                const f = clamp((mouse.x - 16) / Math.max(1, width - 32), 0, 1)
                                if (Math.abs(f - cursorA) < 0.025) { movingCursor = true; cursorIsA = true }
                                else if (Math.abs(f - cursorB) < 0.025) { movingCursor = true; cursorIsA = false }
                                else { movingCursor = false; dragStartX = mouse.x; panStart = waveformPan }
                            }
                            onPositionChanged: mouse => {
                                if (!(mouse.buttons & Qt.LeftButton)) return
                                const f = clamp((mouse.x - 16) / Math.max(1, width - 32), 0, 1)
                                if (movingCursor) {
                                    if (cursorIsA) cursorA = f; else cursorB = f
                                } else if (waveformZoom > 1.0) {
                                    const delta = (mouse.x - dragStartX) / Math.max(1, width)
                                    waveformPan = clamp(panStart - delta, 0, 1)
                                }
                            }
                            onWheel: wheel => {
                                const direction = wheel.angleDelta.y > 0 ? 1.35 : (1.0 / 1.35)
                                waveformZoom = clamp(waveformZoom * direction, 1.0, 200.0)
                                wheel.accepted = true
                            }
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 2
                            Label { text: documentController.selectedSignal; color: "#d7e4ee"; font.pixelSize: 11 }
                            Label { text: documentController.sampleCount + " samples · zoom " + waveformZoom.toFixed(2) + "×"; color: "#657687"; font.pixelSize: 9 }
                        }

                        Label {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 12
                            text: "Wheel: zoom · drag: pan · drag near cursor: measure"
                            color: "#526171"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#111820"
            border.color: "#1a2631"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 14
                Label {
                    text: documentController.error.length ? documentController.error : documentController.recordHealth
                    color: documentController.error.length ? "#ff8178" : "#718090"
                    font.pixelSize: 10
                }
                Item { Layout.fillWidth: true }
                Label { text: "ardirec 0.2.0-alpha.1 · G1 Viewer Alpha"; color: "#526171"; font.pixelSize: 9 }
            }
        }
    }
}
