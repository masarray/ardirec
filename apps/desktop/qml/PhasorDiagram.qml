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

    function wrapDegrees(value) {
        let angle = value
        while (angle <= -180.0) angle += 360.0
        while (angle > 180.0) angle -= 360.0
        return angle
    }

    function phasorForChannel(index) {
        const representationDependency = root.valueRepresentation
        if (index < 0 || !root.analysis) return ({valid:false})
        const row = root.analysis.phasorAt(index, root.cursorTime)
        if (!row || !row.valid || !Number.isFinite(row.magnitude) || !Number.isFinite(row.angle))
            return ({valid:false})
        // AnalysisController stores a fixed record-time cosine reference. +90 deg presents the
        // familiar sine-wave engineering phase convention without making the phasor spin as the
        // cursor advances through a stationary sinusoid.
        return ({valid:true,
                 magnitude: row.magnitude,
                 angle: root.wrapDegrees(row.angle + 90.0),
                 unit: row.unit || (root.document ? root.document.channelUnit(index) : "")})
    }

    function requestRepaint() {
        if (root.visible) phasorCanvas.requestPaint()
    }

    onCursorTimeChanged: requestRepaint()
    onRoleChanged: requestRepaint()
    onValueRepresentationChanged: requestRepaint()
    onVisibleChanged: if (visible) requestRepaint()
    onWidthChanged: requestRepaint()
    onHeightChanged: requestRepaint()

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

    Label {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 10
        anchors.topMargin: 8
        text: "0° → · +90° ↑"
        color: "#778087"
        font.pixelSize: 8
    }

    Canvas {
        id: phasorCanvas
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: legend.top
        anchors.margins: 8
        antialiasing: true
        renderStrategy: Canvas.Cooperative

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.visible || !root.document || !root.analysis || width < 80 || height < 80) return

            const phases = ["L1", "L2", "L3", "E"]
            let vectors = []
            let maxMag = 0.0
            for (let phase of phases) {
                const index = root.analysis.phaseChannel(root.role, phase)
                if (index < 0) continue
                const p = root.phasorForChannel(index)
                if (!p.valid) continue
                vectors.push({phase: phase, index: index, p: p})
                maxMag = Math.max(maxMag, p.magnitude)
            }
            if (!(maxMag > 0.0)) return

            // Use one square engineering plane inside any panel aspect ratio. This prevents an
            // optical ellipse and leaves deliberate space for degree labels instead of stretching.
            const labelMargin = 28
            const diameter = Math.max(40, Math.min(width, height) - labelMargin * 2)
            const radius = diameter * 0.5
            const cx = width * 0.5
            const cy = height * 0.5

            ctx.save()
            ctx.lineCap = "round"

            // Fine 30-degree polar reference spokes.
            ctx.lineWidth = 0.7
            ctx.strokeStyle = "#d8dde1"
            ctx.setLineDash([2, 4])
            for (let degree = 0; degree < 360; degree += 30) {
                if (degree % 90 === 0) continue
                const a = degree * Math.PI / 180.0
                ctx.beginPath()
                ctx.moveTo(cx, cy)
                ctx.lineTo(cx + Math.cos(a) * radius, cy - Math.sin(a) * radius)
                ctx.stroke()
            }

            // Magnitude rings.
            ctx.setLineDash([])
            for (let ring = 1; ring <= 4; ++ring) {
                ctx.strokeStyle = ring === 4 ? "#b9c0c6" : "#e2e6e9"
                ctx.lineWidth = ring === 4 ? 1.0 : 0.7
                ctx.beginPath()
                ctx.arc(cx, cy, radius * ring / 4.0, 0, Math.PI * 2)
                ctx.stroke()
            }

            // Strong quadrant axes.
            ctx.strokeStyle = "#7f8991"
            ctx.lineWidth = 1.0
            ctx.beginPath(); ctx.moveTo(cx - radius, cy); ctx.lineTo(cx + radius, cy); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(cx, cy - radius); ctx.lineTo(cx, cy + radius); ctx.stroke()

            // Degree labels around the outer ring. Engineering angle is positive counter-clockwise.
            ctx.font = "8px sans-serif"
            ctx.fillStyle = "#59636b"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            for (let degree = 0; degree < 360; degree += 30) {
                const signedDegree = degree <= 180 ? degree : degree - 360
                const a = degree * Math.PI / 180.0
                const labelRadius = radius + 15
                ctx.fillText(signedDegree + "°",
                             cx + Math.cos(a) * labelRadius,
                             cy - Math.sin(a) * labelRadius)
            }

            // Ring scale labels help compare magnitude change without replacing exact legend values.
            ctx.textAlign = "left"
            ctx.textBaseline = "bottom"
            ctx.fillStyle = "#90979c"
            ctx.font = "7px sans-serif"
            for (let ring = 1; ring <= 4; ++ring) {
                const percent = ring * 25
                ctx.fillText(percent + "%", cx + 4, cy - radius * ring / 4.0 + 1)
            }

            // Phasor vectors.
            for (let v of vectors) {
                const a = v.p.angle * Math.PI / 180.0
                const length = radius * 0.94 * v.p.magnitude / maxMag
                const ex = cx + Math.cos(a) * length
                const ey = cy - Math.sin(a) * length
                const color = root.analysis.phaseColorForName(v.phase)

                ctx.strokeStyle = color
                ctx.fillStyle = color
                ctx.lineWidth = v.phase === "E" ? 1.5 : 2.2
                ctx.setLineDash([])
                ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(ex, ey); ctx.stroke()

                const head = 8
                ctx.beginPath()
                ctx.moveTo(ex, ey)
                ctx.lineTo(ex - Math.cos(a - 0.45) * head, ey + Math.sin(a - 0.45) * head)
                ctx.lineTo(ex - Math.cos(a + 0.45) * head, ey + Math.sin(a + 0.45) * head)
                ctx.closePath(); ctx.fill()

                // Phase tag is offset back from the arrow head so it stays inside the circle.
                const tagRadius = Math.max(18, length - 16)
                ctx.font = "600 8px sans-serif"
                ctx.textAlign = Math.cos(a) >= 0 ? "left" : "right"
                ctx.textBaseline = Math.sin(a) >= 0 ? "bottom" : "top"
                ctx.fillText(v.phase,
                             cx + Math.cos(a) * tagRadius + (Math.cos(a) >= 0 ? 3 : -3),
                             cy - Math.sin(a) * tagRadius + (Math.sin(a) >= 0 ? -2 : 2))
            }

            ctx.fillStyle = "#5f6870"
            ctx.beginPath(); ctx.arc(cx, cy, 2.2, 0, Math.PI * 2); ctx.fill()
            ctx.restore()
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
                Rectangle {
                    width: 12
                    height: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.analysis ? root.analysis.phaseColorForName(modelData) : "#777"
                }
                Label {
                    readonly property int channelIndex: root.analysis ? root.analysis.phaseChannel(root.role, modelData) : -1
                    readonly property var phasor: root.phasorForChannel(channelIndex)
                    text: modelData + (phasor.valid
                                       ? "  " + phasor.magnitude.toFixed(2) + " " + phasor.unit
                                         + "  ∠" + phasor.angle.toFixed(1) + "°"
                                       : "")
                    color: "#4e565c"
                    font.pixelSize: 8
                }
            }
        }
    }
}
