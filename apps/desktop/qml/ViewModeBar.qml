// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: currentView === "table" ? 34 : 38
    color: "#e9ecef"
    border.color: "#c4c9ce"

    property string currentView: "time"
    property string timeDisplayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property string ratioSummary: ""
    property bool hasRecord: false
    property bool transformerRatiosAvailable: false

    signal viewRequested(string viewName)
    signal timeDisplayModeRequested(string mode)
    signal valueRepresentationRequested(string representation)

    function viewButtonText(viewName) {
        if (viewName === "time") return "Time Signals"
        if (viewName === "phasor") return "Phasor"
        if (viewName === "locus") return "Locus"
        if (viewName === "harmonics") return "Harmonics"
        return "Table"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 7
        anchors.rightMargin: 8
        spacing: 4

        Label {
            text: "VIEW"
            color: "#606870"
            font.pixelSize: 8
            font.weight: Font.DemiBold
            font.letterSpacing: 0.8
            Layout.rightMargin: 3
        }

        Repeater {
            model: ["time", "phasor", "locus", "harmonics", "table"]
            ToolButton {
                required property string modelData
                text: root.viewButtonText(modelData)
                checkable: true
                checked: root.currentView === modelData
                enabled: root.hasRecord
                font.pixelSize: root.currentView === "table" ? 8 : 9
                onClicked: root.viewRequested(modelData)
            }
        }

        Rectangle { width: 1; height: root.currentView === "table" ? 18 : 22; color: "#c0c5c9"; Layout.leftMargin: 4; Layout.rightMargin: 4 }

        RowLayout {
            visible: root.currentView === "time"
            spacing: 2
            Label { text: "Waveform"; color: "#6b7279"; font.pixelSize: 8; Layout.rightMargin: 3 }
            ToolButton {
                text: "Instant"
                checkable: true
                checked: root.timeDisplayMode === "instantaneous"
                enabled: root.hasRecord
                font.pixelSize: 8
                onClicked: root.timeDisplayModeRequested("instantaneous")
            }
            ToolButton {
                text: "RMS"
                checkable: true
                checked: root.timeDisplayMode === "rms"
                enabled: root.hasRecord
                font.pixelSize: 8
                onClicked: root.timeDisplayModeRequested("rms")
            }
        }

        RowLayout {
            visible: root.currentView === "phasor"
            spacing: 5
            Rectangle {
                width: 7; height: 7; radius: 4; color: "#244f9e"
            }
            Label {
                text: "C1"
                color: "#244f9e"
                font.pixelSize: 8
                font.weight: Font.DemiBold
            }
            Label { text: "↔"; color: "#858c92"; font.pixelSize: 9 }
            Rectangle {
                width: 7; height: 7; radius: 4; color: "#b77900"
            }
            Label {
                text: "C2 · simultaneous comparison"
                color: "#775817"
                font.pixelSize: 8
                font.weight: Font.DemiBold
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            spacing: 2
            Label {
                text: "VALUES"
                color: "#626a71"
                font.pixelSize: 8
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
                Layout.rightMargin: 2
            }
            ToolButton {
                text: "Secondary"
                checkable: true
                checked: root.valueRepresentation === "secondary"
                enabled: root.hasRecord
                font.pixelSize: 8
                onClicked: root.valueRepresentationRequested("secondary")
                ToolTip.visible: hovered
                ToolTip.text: root.transformerRatiosAvailable ? root.ratioSummary : "No valid CT/PT ratio metadata; values remain 1:1"
            }
            ToolButton {
                text: "Primary"
                checkable: true
                checked: root.valueRepresentation === "primary"
                enabled: root.hasRecord
                font.pixelSize: 8
                onClicked: root.valueRepresentationRequested("primary")
                ToolTip.visible: hovered
                ToolTip.text: root.transformerRatiosAvailable ? root.ratioSummary : "No valid CT/PT ratio metadata; values remain 1:1"
            }
        }

        Rectangle { width: 1; height: root.currentView === "table" ? 18 : 22; color: "#c0c5c9"; Layout.leftMargin: 3; Layout.rightMargin: 3 }

        Label {
            text: root.currentView === "time" ? (root.timeDisplayMode === "rms" ? "ONE-CYCLE RMS" : "RECORDED SAMPLES")
                  : root.currentView === "phasor" ? "FUNDAMENTAL DFT · SHARED SCALE"
                  : root.currentView === "locus" ? "PROTECTION R-X"
                  : root.currentView === "harmonics" ? "TRAILING 1-CYCLE DFT"
                  : "1-CYCLE SNAPSHOT"
            color: "#737b82"
            font.pixelSize: 8
            font.letterSpacing: 0.4
        }
    }
}
