// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import Ardirec.Render 1.0

Rectangle {
    id: root
    color: "#fbfbfb"
    border.color: "#c8c8c8"

    property var document
    property var analysis
    property int channelIndex: -1
    property real zoomFactor: 1.0
    property real panFraction: 0.0
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property string displayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property color traceColor: "#315f8d"
    property real axisWidth: 92
    property color cursorAColor: "#244f9e"
    property color cursorBColor: "#b77900"

    readonly property real rawPeak: {
        const representationDependency = root.valueRepresentation
        return document && channelIndex >= 0 ? document.channelPeak(channelIndex) : 1.0
    }
    readonly property real engineeringScale: {
        if (!root.document || root.channelIndex < 0 || root.valueRepresentation !== "primary") return 1.0
        const unit = root.document.channelUnit(root.channelIndex).trim().toUpperCase()
        const peak = Math.abs(root.rawPeak)
        if (unit === "V" || unit === "A") {
            if (peak >= 1000000.0) return 0.000001
            if (peak >= 1000.0) return 0.001
        }
        return 1.0
    }
    readonly property string engineeringUnit: {
        if (!root.document || root.channelIndex < 0) return ""
        const unit = root.document.channelUnit(root.channelIndex)
        if (root.engineeringScale === 0.000001) return unit.trim().toUpperCase() === "V" ? "MV" : "MA"
        if (root.engineeringScale === 0.001) return unit.trim().toUpperCase() === "V" ? "kV" : "kA"
        return unit
    }
    readonly property real displayPeak: (displayMode === "rms" ? rawPeak / Math.sqrt(2.0) : rawPeak) * engineeringScale

    // Evaluate only the measurement mode that is visible. Keeping four always-live bindings here
    // would run two one-cycle RMS traversals per lane even while the operator is viewing samples.
    readonly property real cursorAValue: {
        const representationDependency = root.valueRepresentation
        if (!root.document || root.channelIndex < 0) return NaN
        if (root.displayMode === "rms")
            return root.analysis ? root.analysis.rmsValue(root.channelIndex, root.cursorATime) : NaN
        return root.document.sampleValue(root.channelIndex, root.cursorATime)
    }
    readonly property real cursorBValue: {
        const representationDependency = root.valueRepresentation
        if (!root.document || root.channelIndex < 0) return NaN
        if (root.displayMode === "rms")
            return root.analysis ? root.analysis.rmsValue(root.channelIndex, root.cursorBTime) : NaN
        return root.document.sampleValue(root.channelIndex, root.cursorBTime)
    }

    function formatAxis(value) {
        const magnitude = Math.abs(value)
        if (magnitude >= 100000) return value.toFixed(0)
        if (magnitude >= 10000) return value.toFixed(0)
        if (magnitude >= 1000) return value.toFixed(1)
        if (magnitude >= 100) return value.toFixed(1)
        if (magnitude >= 10) return value.toFixed(2)
        if (magnitude >= 1) return value.toFixed(3)
        return value.toFixed(4)
    }

    function formatCursorValue(value) {
        if (!Number.isFinite(value)) return "—"
        return root.formatAxis(value * root.engineeringScale) + (root.engineeringUnit.length ? " " + root.engineeringUnit : "")
    }

    Rectangle {
        id: axis
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.axisWidth
        color: "#f4f4f4"
        border.color: "#cccccc"

        Label {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 7
            anchors.topMargin: 5
            width: parent.width - 14
            text: root.document && root.channelIndex >= 0 ? root.document.channelName(root.channelIndex) : "—"
            color: root.traceColor
            font.pixelSize: 10
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Label {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 7
            anchors.topMargin: 22
            text: root.engineeringUnit
                  + (root.displayMode === "rms" ? " · RMS" : " · INSTANT")
                  + " · " + (root.valueRepresentation === "primary" ? "PRI" : "SEC")
            color: "#707070"
            font.pixelSize: 8
            ToolTip.visible: ratioHover.containsMouse
            ToolTip.text: root.document && root.channelIndex >= 0 ? root.document.channelRatioText(root.channelIndex) : ""
        }
        MouseArea { id: ratioHover; anchors.left: parent.left; anchors.top: parent.top; width: parent.width; height: 38; hoverEnabled: true; acceptedButtons: Qt.NoButton }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            y: 43
            height: 30
            radius: 3
            color: "#f7f9fc"
            border.color: "#cbd6e6"
            Row {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 5
                spacing: 5
                Label { text: "C1"; color: root.cursorAColor; font.pixelSize: 9; font.weight: Font.Bold; anchors.verticalCenter: parent.verticalCenter }
                Label {
                    width: Math.max(1, parent.width - 30)
                    text: root.formatCursorValue(root.cursorAValue)
                    color: "#23282d"; font.pixelSize: 10; font.family: "Consolas"; font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideLeft
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            y: 77
            height: 30
            radius: 3
            color: "#fcfaf5"
            border.color: "#e4d3ae"
            Row {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 5
                spacing: 5
                Label { text: "C2"; color: root.cursorBColor; font.pixelSize: 9; font.weight: Font.Bold; anchors.verticalCenter: parent.verticalCenter }
                Label {
                    width: Math.max(1, parent.width - 30)
                    text: root.formatCursorValue(root.cursorBValue)
                    color: "#23282d"; font.pixelSize: 10; font.family: "Consolas"; font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideLeft
                }
            }
        }

        Label {
            anchors.right: parent.right
            anchors.rightMargin: 7
            y: Math.max(112, parent.height * 0.13 - height * 0.5)
            text: root.formatAxis(root.displayPeak)
            color: "#656b70"
            font.pixelSize: 8
        }
        Label {
            anchors.right: parent.right
            anchors.rightMargin: 7
            y: parent.height * 0.55 - height * 0.5
            text: root.displayMode === "rms" ? root.formatAxis(root.displayPeak * 0.5) : "0"
            color: "#7a7f83"
            font.pixelSize: 8
        }
        Label {
            anchors.right: parent.right
            anchors.rightMargin: 7
            y: parent.height * 0.90 - height * 0.5
            text: root.displayMode === "rms" ? "0" : root.formatAxis(-root.displayPeak)
            color: "#656b70"
            font.pixelSize: 8
        }
    }

    Rectangle {
        id: chart
        anchors.left: axis.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: "#ffffff"
        clip: true

        Repeater {
            model: 11
            Rectangle {
                required property int index
                x: chart.width * index / 10
                y: 0
                width: 1
                height: chart.height
                color: index === 0 || index === 10 ? "#c3c3c3" : "#e4e4e4"
            }
        }

        Repeater {
            model: [0.10, 0.30, 0.50, 0.70, 0.90]
            Rectangle {
                required property real modelData
                x: 0
                y: chart.height * modelData
                width: chart.width
                height: 1
                color: modelData === 0.50 ? "#b8b8b8" : "#e1e1e1"
            }
        }

        WaveformItem {
            anchors.fill: parent
            visible: root.displayMode !== "rms"
            document: root.document
            channelIndex: root.channelIndex
            traceColor: root.traceColor
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
        }

        RmsWaveformItem {
            anchors.fill: parent
            visible: root.displayMode === "rms"
            document: root.document
            channelIndex: root.channelIndex
            traceColor: root.traceColor
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
        }

        TriggerReference {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: (root.document.triggerOffsetSeconds - root.viewStart) / root.visibleDuration * chart.width
            height: chart.height
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorATime >= root.viewStart
                     && root.cursorATime <= root.viewStart + root.visibleDuration
            x: (root.cursorATime - root.viewStart) / root.visibleDuration * chart.width
            y: 0
            width: 1
            height: chart.height
            color: root.cursorAColor
            opacity: 0.95
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorBTime >= root.viewStart
                     && root.cursorBTime <= root.viewStart + root.visibleDuration
            x: (root.cursorBTime - root.viewStart) / root.visibleDuration * chart.width
            y: 0
            width: 1
            height: chart.height
            color: root.cursorBColor
            opacity: 0.95
        }
    }
}
