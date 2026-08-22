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

    FileDialog {
        id: openDialog
        title: "Open COMTRADE configuration"
        nameFilters: ["COMTRADE configuration (*.cfg)"]
        onAccepted: documentController.openCfg(selectedFile)
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
                Layout.preferredWidth: 236
                Layout.fillHeight: true
                channels: documentController.channels
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
                        Label { text: "TIME SIGNALS"; color: "#9aa9b8"; font.pixelSize: 11; font.letterSpacing: 1.2 }
                        Item { Layout.fillWidth: true }
                        Label { text: "A  —   B  —   Δt —"; color: "#718090"; font.pixelSize: 11 }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0d1319"
                        border.color: "#1c2833"
                        radius: 4

                        Repeater {
                            model: 8
                            Rectangle {
                                required property int index
                                x: 0
                                y: parent.height * (index + 1) / 9
                                width: parent.width
                                height: 1
                                color: "#17212b"
                            }
                        }

                        WaveformItem { anchors.fill: parent; anchors.margins: 16 }

                        Label {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 12
                            text: "GPU Scene Graph renderer skeleton · G1 connects real samples + LOD"
                            color: "#526171"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#111820"
            border.color: "#1a2631"
            Label {
                anchors.centerIn: parent
                text: documentController.error.length ? documentController.error : "READY · ardirec foundation 0.1.0"
                color: documentController.error.length ? "#ff8178" : "#718090"
                font.pixelSize: 10
            }
        }
    }
}
