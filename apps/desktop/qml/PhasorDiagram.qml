// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import Ardirec.Render 1.0

Rectangle {
    id: root
    color: "#ffffff"
    border.color: "#c5cbd0"

    property var document
    property var analysis
    property var snapshot: ({valid:false})
    property var channels: []
    property string role: "Voltage"
    property string title: "PHASORS"
    property real scaleMagnitude: 0.0
    property color cursorAccent: "#244f9e"
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"

    function wrapDegrees(value) {
        let angle = value
        while (angle <= -180.0) angle += 360.0
        while (angle > 180.0) angle -= 360.0
        return angle
    }

    function fallbackChannels() {
        let result = []
        if (!root.analysis) return result
        const phases = ["L1", "L2", "L3", "E"]
        for (let phase of phases) {
            const index = root.analysis.phaseChannel(root.role, phase)
            if (index >= 0) result.push(index)
        }
        return result
    }

    function effectiveChannels() {
        return root.channels && root.channels.length ? root.channels : root.fallbackChannels()
    }

    function snapshotRow(index) {
        if (!root.snapshot || !root.snapshot.valid || !root.snapshot.channels) return ({valid:false})
        if (index < 0 || index >= root.snapshot.channels.length) return ({valid:false})
        return root.snapshot.channels[index] || ({valid:false})
    }

    function buildVectors() {
        let vectors = []
        for (let index of root.effectiveChannels()) {
            const row = root.snapshotRow(index)
            if (!row.valid || !Number.isFinite(row.magnitude) || !Number.isFinite(row.angle)) continue
            vectors.push({
                valid: true,
                index: index,
                magnitude: row.magnitude,
                angle: root.wrapDegrees(row.angle + 90.0),
                phase: row.phase || (root.analysis ? root.analysis.channelPhase(index) : "Other"),
                color: root.analysis ? root.analysis.phaseColor(index) : "#6f7780"
            })
        }
        return vectors
    }

    readonly property var vectorRows: buildVectors()
    readonly property real localMaximum: {
        let maximum = 0.0
        for (let row of vectorRows) maximum = Math.max(maximum, Number(row.magnitude) || 0.0)
        return maximum
    }
    readonly property real effectiveScale: scaleMagnitude > 0.0 ? scaleMagnitude : localMaximum

    function displayScale(index) {
        if (!root.document || index < 0 || root.valueRepresentation !== "primary") return 1.0
        const unit = root.document.channelUnit(index).trim().toUpperCase()
        const peak = Math.abs(root.document.channelPeak(index))
        if (unit === "V" || unit === "A") {
            if (peak >= 1000000.0) return 0.000001
            if (peak >= 1000.0) return 0.001
        }
        return 1.0
    }

    function displayUnit(index) {
        if (!root.document || index < 0) return ""
        const unit = root.document.channelUnit(index)
        const scale = root.displayScale(index)
        if (scale === 0.000001) return unit.trim().toUpperCase() === "V" ? "MV" : "MA"
        if (scale === 0.001) return unit.trim().toUpperCase() === "V" ? "kV" : "kA"
        return unit
    }

    function formatMagnitude(index, magnitude) {
        if (!Number.isFinite(magnitude)) return "—"
        const value = magnitude * root.displayScale(index)
        const absValue = Math.abs(value)
        const decimals = absValue >= 100 ? 1 : absValue >= 10 ? 2 : 3
        const unit = root.displayUnit(index)
        return value.toFixed(decimals) + (unit.length ? " " + unit : "")
    }

    function scaleText() {
        const list = root.effectiveChannels()
        if (!list.length || !(root.effectiveScale > 0)) return "AUTO SCALE"
        return "RMAX " + root.formatMagnitude(list[0], root.effectiveScale)
    }

    Rectangle {
        id: accent
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: header.bottom
        width: 4
        color: root.cursorAccent
    }

    Label {
        id: header
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 14
        anchors.topMargin: 9
        text: root.title + " · " + (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
        color: "#303940"
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.letterSpacing: 0.4
    }

    Label {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 12
        anchors.topMargin: 10
        text: root.scaleText() + " · 0° → · +90° ↑"
        color: "#68737b"
        font.pixelSize: 9
    }

    Item {
        id: plot
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: legendScroll.top
        anchors.margins: 8
        clip: true

        readonly property real cx: width * 0.5
        readonly property real cy: height * 0.5
        readonly property real radius: Math.max(20, Math.min(width, height) * 0.40)

        Repeater {
            model: 4
            Rectangle {
                required property int index
                width: plot.radius * 2 * (index + 1) / 4
                height: width
                radius: width * 0.5
                x: plot.cx - width * 0.5
                y: plot.cy - height * 0.5
                color: "transparent"
                border.width: index === 3 ? 1.1 : 1
                border.color: index === 3 ? "#aeb7be" : "#dde3e7"
            }
        }

        Repeater {
            model: 12
            Rectangle {
                required property int index
                width: plot.radius
                height: index % 3 === 0 ? 1.2 : 1
                x: plot.cx
                y: plot.cy - height * 0.5
                transformOrigin: Item.Left
                rotation: -index * 30
                color: index % 3 === 0 ? "#717d86" : "#d5dbe0"
                opacity: index % 3 === 0 ? 0.95 : 0.75
            }
        }

        Repeater {
            model: 12
            Label {
                required property int index
                readonly property int degree: index * 30
                readonly property int signedDegree: degree <= 180 ? degree : degree - 360
                readonly property real radians: degree * Math.PI / 180.0
                x: plot.cx + Math.cos(radians) * (plot.radius + 17) - width * 0.5
                y: plot.cy - Math.sin(radians) * (plot.radius + 17) - height * 0.5
                text: signedDegree + "°"
                color: "#4f5b64"
                font.pixelSize: 8
            }
        }

        Repeater {
            model: 4
            Label {
                required property int index
                x: plot.cx + 5
                y: plot.cy - plot.radius * (index + 1) / 4 - height * 0.5
                text: ((index + 1) * 25) + "%"
                color: "#7d878e"
                font.pixelSize: 7
            }
        }

        PhasorVectorItem {
            anchors.fill: parent
            vectors: root.vectorRows
            scaleMagnitude: root.effectiveScale
        }

        Repeater {
            model: root.vectorRows
            Label {
                required property var modelData
                readonly property real radians: Number(modelData.angle) * Math.PI / 180.0
                readonly property real fraction: root.effectiveScale > 0 ? Math.min(1.0, Number(modelData.magnitude) / root.effectiveScale) : 0.0
                readonly property real tagRadius: Math.max(22, plot.radius * 0.94 * fraction - 18)
                x: plot.cx + Math.cos(radians) * tagRadius + (Math.cos(radians) >= 0 ? 4 : -width - 4)
                y: plot.cy - Math.sin(radians) * tagRadius + (Math.sin(radians) >= 0 ? -height - 2 : 2)
                text: modelData.phase !== "Other" ? modelData.phase
                                                   : (root.document ? root.document.channelName(modelData.index) : "")
                color: modelData.color
                font.pixelSize: 9
                font.weight: Font.DemiBold
            }
        }

        Label {
            anchors.centerIn: parent
            visible: root.effectiveChannels().length > 0 && root.vectorRows.length === 0
            text: cursorSnapshotController.busyA || cursorSnapshotController.busyB
                  ? "Updating fundamental phasors…"
                  : "No valid full-cycle phasor at this cursor"
            color: "#66727a"
            font.pixelSize: 10
            font.weight: Font.DemiBold
        }
    }

    Flickable {
        id: legendScroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 5
        height: 31
        contentWidth: legend.implicitWidth
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: legend
            spacing: 18
            height: parent.height
            Repeater {
                model: root.effectiveChannels()
                Row {
                    required property int modelData
                    readonly property var row: root.snapshotRow(modelData)
                    spacing: 5
                    Rectangle {
                        width: 14; height: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.analysis ? root.analysis.phaseColor(modelData) : "#777"
                    }
                    Label {
                        text: (root.document ? root.document.channelName(modelData) : "—")
                              + (row && row.valid
                                 ? "  " + root.formatMagnitude(modelData, row.magnitude)
                                   + "  ∠" + root.wrapDegrees(row.angle + 90.0).toFixed(1) + "°"
                                 : "  —")
                        color: "#3f494f"
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }
}
