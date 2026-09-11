// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#ffffff"
    border.color: "#c5cbd0"

    property var document
    property var analysis
    property var channels: []
    property string role: "Voltage"
    property string title: "PHASORS"
    property real cursorTime: 0.0
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

    function phasorForChannel(index) {
        const representationDependency = root.valueRepresentation
        if (index < 0 || !root.analysis) return ({valid:false})
        const row = root.analysis.phasorAt(index, root.cursorTime)
        if (!row || !row.valid || !Number.isFinite(row.magnitude) || !Number.isFinite(row.angle))
            return ({valid:false})
        // AnalysisController uses a fixed record cosine reference. +90° displays the familiar
        // sine-wave engineering convention without making a stationary phasor rotate with time.
        return ({valid:true,
                 magnitude: row.magnitude,
                 angle: root.wrapDegrees(row.angle + 90.0),
                 unit: row.unit || (root.document ? root.document.channelUnit(index) : "")})
    }

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
        if (!list.length || !(root.scaleMagnitude > 0)) return "AUTO SCALE"
        return "RMAX " + root.formatMagnitude(list[0], root.scaleMagnitude)
    }

    function requestRepaint() {
        if (root.visible) phasorCanvas.requestPaint()
    }

    onCursorTimeChanged: requestRepaint()
    onRoleChanged: requestRepaint()
    onChannelsChanged: requestRepaint()
    onScaleMagnitudeChanged: requestRepaint()
    onValueRepresentationChanged: requestRepaint()
    onVisibleChanged: if (visible) requestRepaint()
    onWidthChanged: requestRepaint()
    onHeightChanged: requestRepaint()

    Connections {
        target: root.document
        function onDocumentChanged() { root.requestRepaint() }
        function onRepresentationChanged() { root.requestRepaint() }
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
        anchors.leftMargin: 12
        anchors.topMargin: 8
        text: root.title + " · " + (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
        color: "#3d464d"
        font.pixelSize: 9
        font.weight: Font.DemiBold
        font.letterSpacing: 0.5
    }

    Label {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 10
        anchors.topMargin: 8
        text: root.scaleText() + " · 0° → · +90° ↑"
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

            const list = root.effectiveChannels()
            let vectors = []
            let localMax = 0.0
            for (let index of list) {
                const p = root.phasorForChannel(index)
                if (!p.valid) continue
                const phase = root.analysis.channelPhase(index)
                vectors.push({phase: phase, index: index, p: p})
                localMax = Math.max(localMax, p.magnitude)
            }
            const maxMag = root.scaleMagnitude > 0 ? root.scaleMagnitude : localMax
            if (!(maxMag > 0.0)) return

            const labelMargin = 28
            const diameter = Math.max(40, Math.min(width, height) - labelMargin * 2)
            const radius = diameter * 0.5
            const cx = width * 0.5
            const cy = height * 0.5

            ctx.save()
            ctx.lineCap = "round"

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

            ctx.setLineDash([])
            for (let ring = 1; ring <= 4; ++ring) {
                ctx.strokeStyle = ring === 4 ? "#b9c0c6" : "#e2e6e9"
                ctx.lineWidth = ring === 4 ? 1.0 : 0.7
                ctx.beginPath()
                ctx.arc(cx, cy, radius * ring / 4.0, 0, Math.PI * 2)
                ctx.stroke()
            }

            ctx.strokeStyle = "#7f8991"
            ctx.lineWidth = 1.0
            ctx.beginPath(); ctx.moveTo(cx - radius, cy); ctx.lineTo(cx + radius, cy); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(cx, cy - radius); ctx.lineTo(cx, cy + radius); ctx.stroke()

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

            ctx.textAlign = "left"
            ctx.textBaseline = "bottom"
            ctx.fillStyle = "#90979c"
            ctx.font = "7px sans-serif"
            for (let ring = 1; ring <= 4; ++ring) {
                ctx.fillText((ring * 25) + "%", cx + 4, cy - radius * ring / 4.0 + 1)
            }

            for (let v of vectors) {
                const a = v.p.angle * Math.PI / 180.0
                const length = radius * 0.94 * Math.min(1.0, v.p.magnitude / maxMag)
                const ex = cx + Math.cos(a) * length
                const ey = cy - Math.sin(a) * length
                const color = root.analysis.phaseColor(v.index)

                ctx.strokeStyle = color
                ctx.fillStyle = color
                ctx.lineWidth = v.phase === "E" ? 1.6 : 2.2
                ctx.setLineDash([])
                ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(ex, ey); ctx.stroke()

                const head = 8
                ctx.beginPath()
                ctx.moveTo(ex, ey)
                ctx.lineTo(ex - Math.cos(a - 0.45) * head, ey + Math.sin(a - 0.45) * head)
                ctx.lineTo(ex - Math.cos(a + 0.45) * head, ey + Math.sin(a + 0.45) * head)
                ctx.closePath(); ctx.fill()

                const tag = v.phase !== "Other" ? v.phase : root.document.channelName(v.index)
                const tagRadius = Math.max(18, length - 16)
                ctx.font = "600 8px sans-serif"
                ctx.textAlign = Math.cos(a) >= 0 ? "left" : "right"
                ctx.textBaseline = Math.sin(a) >= 0 ? "bottom" : "top"
                ctx.fillText(tag,
                             cx + Math.cos(a) * tagRadius + (Math.cos(a) >= 0 ? 3 : -3),
                             cy - Math.sin(a) * tagRadius + (Math.sin(a) >= 0 ? -2 : 2))
            }

            ctx.fillStyle = "#5f6870"
            ctx.beginPath(); ctx.arc(cx, cy, 2.2, 0, Math.PI * 2); ctx.fill()
            ctx.restore()
        }
    }

    Flickable {
        id: legendScroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 5
        height: 27
        contentWidth: legend.implicitWidth
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: legend
            spacing: 15
            height: parent.height

            Repeater {
                model: root.effectiveChannels()
                Row {
                    required property int modelData
                    spacing: 4
                    visible: root.analysis && modelData >= 0
                    Rectangle {
                        width: 12
                        height: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.analysis ? root.analysis.phaseColor(modelData) : "#777"
                    }
                    Label {
                        readonly property var phasor: root.phasorForChannel(modelData)
                        text: (root.document ? root.document.channelName(modelData) : "—")
                              + (phasor.valid
                                 ? "  " + root.formatMagnitude(modelData, phasor.magnitude)
                                   + "  ∠" + phasor.angle.toFixed(1) + "°"
                                 : "")
                        color: "#4e565c"
                        font.pixelSize: 8
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }
}
