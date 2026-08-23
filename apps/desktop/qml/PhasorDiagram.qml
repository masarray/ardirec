// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#ffffff"
    border.color: "#c5cbd0"

    property var document
    property var analysis
    property string role: "Voltage"
    property string title: "VOLTAGE PHASORS"
    property real cursorTime: 0.0
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"

    function requestRepaint() { phasorCanvas.requestPaint() }
    onCursorTimeChanged: requestRepaint()
    onRoleChanged: requestRepaint()
    onValueRepresentationChanged: requestRepaint()
    Connections {
        target: root.document
        function onDocumentChanged() { root.requestRepaint() }
    }

    Label {
        id: header
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.topMargin: 8
        text: root.title + " · " + (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
        color: "#3d464d"
        font.pixelSize: 9
        font.weight: Font.DemiBold
        font.letterSpacing: 0.6
    }

    Canvas {
        id: phasorCanvas
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: legend.top
        anchors.margins: 8
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.document || !root.analysis || width < 40 || height < 40) return

            const phases = ["L1", "L2", "L3", "E"]
            let vectors = []
            let maxMag = 0
            for (let phase of phases) {
                const index = root.analysis.phaseChannel(root.role, phase)
                if (index < 0) continue
                const p = root.analysis.phasorAt(index, root.cursorTime)
                if (!p.valid) continue
                vectors.push({phase: phase, index: index, p: p})
                maxMag = Math.max(maxMag, p.magnitude)
            }
            if (maxMag <= 0) return

            const cx = width * 0.5
            const cy = height * 0.5
            const radius = Math.max(20, Math.min(width, height) * 0.38)

            ctx.lineWidth = 1
            ctx.strokeStyle = "#e0e4e7"
            for (let ring = 1; ring <= 4; ++ring) {
                ctx.beginPath()
                ctx.arc(cx, cy, radius * ring / 4, 0, Math.PI * 2)
                ctx.stroke()
            }
            ctx.strokeStyle = "#aeb6bc"
            ctx.beginPath(); ctx.moveTo(cx - radius, cy); ctx.lineTo(cx + radius, cy); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(cx, cy - radius); ctx.lineTo(cx, cy + radius); ctx.stroke()

            for (let v of vectors) {
                const a = v.p.angle * Math.PI / 180.0
                const length = radius * v.p.magnitude / maxMag
                const ex = cx + Math.cos(a) * length
                const ey = cy - Math.sin(a) * length
                const color = root.analysis.phaseColorForName(v.phase)

                ctx.strokeStyle = color
                ctx.fillStyle = color
                ctx.lineWidth = v.phase === "E" ? 1.5 : 2.2
                ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(ex, ey); ctx.stroke()

                const head = 8
                ctx.beginPath()
                ctx.moveTo(ex, ey)
                ctx.lineTo(ex - Math.cos(a - 0.45) * head, ey + Math.sin(a - 0.45) * head)
                ctx.lineTo(ex - Math.cos(a + 0.45) * head, ey + Math.sin(a + 0.45) * head)
                ctx.closePath(); ctx.fill()
            }
        }
    }

    Row {
        id: legend
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 8
        spacing: 16
        height: 22
        Repeater {
            model: ["L1", "L2", "L3", "E"]
            Row {
                required property string modelData
                spacing: 4
                visible: root.analysis && root.analysis.phaseChannel(root.role, modelData) >= 0
                Rectangle { width: 12; height: 2; anchors.verticalCenter: parent.verticalCenter; color: root.analysis ? root.analysis.phaseColorForName(modelData) : "#777" }
                Label {
                    readonly property int channelIndex: root.analysis ? root.analysis.phaseChannel(root.role, modelData) : -1
                    readonly property var phasor: {
                        const representationDependency = root.valueRepresentation
                        return channelIndex >= 0 && root.analysis ? root.analysis.phasorAt(channelIndex, root.cursorTime) : ({valid:false})
                    }
                    text: modelData + (phasor.valid ? "  " + phasor.magnitude.toFixed(2) + " " + phasor.unit + "  ∠" + phasor.angle.toFixed(1) + "°" : "")
                    color: "#4e565c"
                    font.pixelSize: 8
                }
            }
        }
    }
}
