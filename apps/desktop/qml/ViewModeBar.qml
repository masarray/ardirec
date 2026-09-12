// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 48
    color: "#e9ecef"
    border.color: "#c4c9ce"

    property var actions
    property string currentView: "time"
    property string timeDisplayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property string ratioSummary: ""
    property bool hasRecord: false
    property bool transformerRatiosAvailable: false
    readonly property bool showViewLabels: width >= 1260

    function viewButtonText(viewName) {
        if (viewName === "time") return "Time Signals"
        if (viewName === "phasor") return "Phasor"
        if (viewName === "locus") return "Locus"
        if (viewName === "harmonics") return "Harmonics"
        return "Table"
    }

    function viewIcon(viewName) {
        if (viewName === "time") return "icons/activity.svg"
        if (viewName === "phasor") return "icons/circle-dot.svg"
        if (viewName === "locus") return "icons/crosshair.svg"
        if (viewName === "harmonics") return "icons/chart-column.svg"
        return "icons/table-2.svg"
    }

    function viewAction(viewName) {
        if (!root.actions) return null
        if (viewName === "time") return root.actions.timeView
        if (viewName === "phasor") return root.actions.phasorView
        if (viewName === "locus") return root.actions.locusView
        if (viewName === "harmonics") return root.actions.harmonicsView
        return root.actions.tableView
    }

    component ViewButton: ToolButton {
        id: control
        required property string viewName
        property string labelText: root.viewButtonText(viewName)
        property url iconSource: root.viewIcon(viewName)
        action: root.viewAction(viewName)
        checkable: true
        hoverEnabled: true
        Layout.preferredWidth: root.showViewLabels ? Math.max(80, contentRow.implicitWidth + 20) : 40
        Layout.preferredHeight: 36
        padding: 0

        contentItem: Item {
            implicitWidth: contentRow.implicitWidth
            implicitHeight: 30
            Row {
                id: contentRow
                anchors.centerIn: parent
                spacing: 6
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    source: control.iconSource
                    sourceSize.width: 22
                    sourceSize.height: 22
                    fillMode: Image.PreserveAspectFit
                    opacity: control.enabled ? 1.0 : 0.35
                }
                Label {
                    visible: root.showViewLabels
                    anchors.verticalCenter: parent.verticalCenter
                    text: control.labelText
                    color: control.enabled ? "#303940" : "#93999e"
                    font.pixelSize: 11
                    font.weight: control.checked ? Font.DemiBold : Font.Normal
                }
            }
        }

        ToolTip.visible: hovered
        ToolTip.delay: 300
        ToolTip.text: labelText + "  ·  " + (viewName === "time" ? "1" : viewName === "phasor" ? "2" : viewName === "locus" ? "3" : viewName === "harmonics" ? "4" : "5")
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 9
        spacing: 4

        Label {
            text: "VIEW"
            color: "#56616a"
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 0.8
            Layout.rightMargin: 4
        }

        Repeater {
            model: ["time", "phasor", "locus", "harmonics", "table"]
            ViewButton {
                required property string modelData
                viewName: modelData
            }
        }

        Rectangle { width: 1; height: 28; color: "#c0c5c9"; Layout.leftMargin: 5; Layout.rightMargin: 5 }

        RowLayout {
            visible: root.currentView === "time"
            spacing: 3
            Label { text: "Waveform"; color: "#626c74"; font.pixelSize: 10; font.weight: Font.DemiBold; Layout.rightMargin: 3 }
            ToolButton {
                action: root.actions?.waveformInstant ?? null
                text: "Instant"
                checkable: true
                font.pixelSize: 10
                Layout.preferredHeight: 32
            }
            ToolButton {
                action: root.actions?.waveformRms ?? null
                text: "RMS"
                checkable: true
                font.pixelSize: 10
                Layout.preferredHeight: 32
            }
        }

        RowLayout {
            visible: root.currentView === "phasor"
            spacing: 6
            Rectangle { width: 8; height: 8; radius: 4; color: "#244f9e" }
            Label { text: "C1"; color: "#244f9e"; font.pixelSize: 10; font.weight: Font.DemiBold }
            Label { text: "↔"; color: "#858c92"; font.pixelSize: 11 }
            Rectangle { width: 8; height: 8; radius: 4; color: "#b77900" }
            Label { visible: root.width >= 1180; text: "C2 · simultaneous comparison"; color: "#775817"; font.pixelSize: 10; font.weight: Font.DemiBold }
            Label { visible: root.width < 1180; text: "C2"; color: "#775817"; font.pixelSize: 10; font.weight: Font.DemiBold }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            spacing: 3
            Label { text: "VALUES"; color: "#56616a"; font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 0.5; Layout.rightMargin: 2 }
            ToolButton {
                action: root.actions?.secondaryValues ?? null
                text: "Secondary"
                checkable: true
                font.pixelSize: 10
                Layout.preferredHeight: 32
                ToolTip.visible: hovered
                ToolTip.text: root.transformerRatiosAvailable ? root.ratioSummary : "No valid CT/PT ratio metadata; values remain 1:1"
            }
            ToolButton {
                action: root.actions?.primaryValues ?? null
                text: "Primary"
                checkable: true
                font.pixelSize: 10
                Layout.preferredHeight: 32
                ToolTip.visible: hovered
                ToolTip.text: root.transformerRatiosAvailable ? root.ratioSummary : "No valid CT/PT ratio metadata; values remain 1:1"
            }
        }

        Rectangle { visible: root.width >= 1320; width: 1; height: 28; color: "#c0c5c9"; Layout.leftMargin: 4; Layout.rightMargin: 4 }

        Label {
            visible: root.width >= 1320
            text: root.currentView === "time" ? (root.timeDisplayMode === "rms" ? "ONE-CYCLE RMS" : "RECORDED SAMPLES")
                  : root.currentView === "phasor" ? "FUNDAMENTAL DFT · SHARED SCALE"
                  : root.currentView === "locus" ? "PROTECTION R-X"
                  : root.currentView === "harmonics" ? "TRAILING 1-CYCLE DFT"
                  : "1-CYCLE SNAPSHOT"
            color: "#69737b"
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 0.35
        }
    }
}
