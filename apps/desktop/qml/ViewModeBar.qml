// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 38
    color: "#e9ecef"
    border.color: "#c4c9ce"

    property string currentView: "time"
    property string timeDisplayMode: "instantaneous"
    property int activeAnalysisCursor: 1
    property bool hasRecord: false

    signal viewRequested(string viewName)
    signal timeDisplayModeRequested(string mode)
    signal activeAnalysisCursorRequested(int cursorNumber)

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
                font.pixelSize: 9
                onClicked: root.viewRequested(modelData)
            }
        }

        Rectangle { width: 1; height: 22; color: "#c0c5c9"; Layout.leftMargin: 4; Layout.rightMargin: 4 }

        RowLayout {
            visible: root.currentView === "time"
            spacing: 2
            Label { text: "Waveform"; color: "#6b7279"; font.pixelSize: 8; Layout.rightMargin: 3 }
            ToolButton {
                text: "Sinusoidal"
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
            visible: root.currentView !== "time" && root.currentView !== "locus" && root.currentView !== "table"
            spacing: 2
            Label { text: "Analysis cursor"; color: "#6b7279"; font.pixelSize: 8; Layout.rightMargin: 3 }
            ToolButton {
                text: "C1"
                checkable: true
                checked: root.activeAnalysisCursor === 1
                enabled: root.hasRecord
                font.pixelSize: 8
                onClicked: root.activeAnalysisCursorRequested(1)
            }
            ToolButton {
                text: "C2"
                checkable: true
                checked: root.activeAnalysisCursor === 2
                enabled: root.hasRecord
                font.pixelSize: 8
                onClicked: root.activeAnalysisCursorRequested(2)
            }
        }

        Item { Layout.fillWidth: true }
        Label {
            text: root.currentView === "time" ? (root.timeDisplayMode === "rms" ? "ONE-CYCLE SLIDING RMS" : "RECORDED SAMPLES")
                  : root.currentView === "phasor" ? "FUNDAMENTAL DFT"
                  : root.currentView === "locus" ? "RAW PHASE V/I IMPEDANCE"
                  : root.currentView === "harmonics" ? "FULL-CYCLE DFT"
                  : "CURSOR VALUE TABLE"
            color: "#737b82"
            font.pixelSize: 8
            font.letterSpacing: 0.5
        }
    }
}
