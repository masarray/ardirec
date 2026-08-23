// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f6f7f8"
    property var document
    property var analysis
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"

    function requestRepaint() { locusCanvas.requestPaint() }
    onViewStartChanged: requestRepaint()
    onVisibleDurationChanged: requestRepaint()
    onCursorATimeChanged: requestRepaint()
    onCursorBTimeChanged: requestRepaint()
    onValueRepresentationChanged: requestRepaint()
    Connections {
        target: root.document
        function onDocumentChanged() { root.requestRepaint() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Label { text: "IMPEDANCE LOCUS"; color: "#394149"; font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 0.6 }
            Label { text: "R-X plane · " + (root.valueRepresentation === "primary" ? "primary" : "secondary"); color: "#707980"; font.pixelSize: 8 }
            Item { Layout.fillWidth: true }
            Label {
                text: "Phase V/I reference trajectory · compensated loops/RIO follow in P2"
                color: "#7b8389"
                font.pixelSize: 8
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#c5cbd0"

            Canvas {
                id: locusCanvas
                anchors.fill: parent
                anchors.margins: 12
                antialiasing: true

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (!root.document || !root.analysis || root.visibleDuration <= 0 || width < 80 || height < 80) return

                    const phases = ["L1", "L2", "L3"]
                    let series = []
                    let extent = 1.0
                    const steps = 120
                    for (let phase of phases) {
                        const v = root.analysis.phaseChannel("Voltage", phase)
                        const i = root.analysis.phaseChannel("Current", phase)
                        if (v < 0 || i < 0) continue
                        let points = []
                        for (let n = 0; n < steps; ++n) {
                            const t = root.viewStart + root.visibleDuration * n / Math.max(1, steps - 1)
                            const z = root.analysis.impedanceAt(v, i, t)
                            if (!z.valid || !Number.isFinite(z.r) || !Number.isFinite(z.x)) continue
                            points.push({r:z.r, x:z.x})
                            extent = Math.max(extent, Math.abs(z.r), Math.abs(z.x))
                        }
                        series.push({phase:phase, voltage:v, current:i, points:points})
                    }
                    extent *= 1.12
                    const left = 46
                    const right = width - 20
                    const top = 18
                    const bottom = height - 38
                    const cx = (left + right) * 0.5
                    const cy = (top + bottom) * 0.5
                    const halfW = (right - left) * 0.5
                    const halfH = (bottom - top) * 0.5
                    const mapX = r => cx + r / extent * halfW
                    const mapY = x => cy - x / extent * halfH

                    ctx.lineWidth = 1
                    ctx.strokeStyle = "#e2e6e9"
                    for (let g = -4; g <= 4; ++g) {
                        const gx = cx + halfW * g / 4
                        const gy = cy + halfH * g / 4
                        ctx.beginPath(); ctx.moveTo(gx, top); ctx.lineTo(gx, bottom); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(left, gy); ctx.lineTo(right, gy); ctx.stroke()
                    }
                    ctx.strokeStyle = "#8e989f"
                    ctx.beginPath(); ctx.moveTo(left, cy); ctx.lineTo(right, cy); ctx.stroke()
                    ctx.beginPath(); ctx.moveTo(cx, top); ctx.lineTo(cx, bottom); ctx.stroke()

                    ctx.fillStyle = "#5b646b"
                    ctx.font = "9px sans-serif"
                    ctx.fillText("R [Ω]", right - 24, cy - 6)
                    ctx.fillText("X [Ω]", cx + 6, top + 10)
                    ctx.fillText((-extent).toFixed(1), left, cy + 14)
                    ctx.fillText(extent.toFixed(1), right - 28, cy + 14)
                    ctx.fillText(extent.toFixed(1), cx + 6, top + 10)
                    ctx.fillText((-extent).toFixed(1), cx + 6, bottom)

                    for (let s of series) {
                        if (s.points.length < 2) continue
                        const color = root.analysis.phaseColorForName(s.phase)
                        ctx.strokeStyle = color
                        ctx.lineWidth = 2
                        ctx.beginPath()
                        ctx.moveTo(mapX(s.points[0].r), mapY(s.points[0].x))
                        for (let p = 1; p < s.points.length; ++p) ctx.lineTo(mapX(s.points[p].r), mapY(s.points[p].x))
                        ctx.stroke()

                        const za = root.analysis.impedanceAt(s.voltage, s.current, root.cursorATime)
                        const zb = root.analysis.impedanceAt(s.voltage, s.current, root.cursorBTime)
                        if (za.valid) {
                            ctx.fillStyle = color
                            ctx.beginPath(); ctx.arc(mapX(za.r), mapY(za.x), 4.5, 0, Math.PI * 2); ctx.fill()
                        }
                        if (zb.valid) {
                            ctx.strokeStyle = color
                            ctx.lineWidth = 2
                            ctx.beginPath(); ctx.arc(mapX(zb.r), mapY(zb.x), 6.5, 0, Math.PI * 2); ctx.stroke()
                        }
                    }
                }
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                spacing: 18
                Repeater {
                    model: ["L1", "L2", "L3"]
                    Row {
                        required property string modelData
                        spacing: 5
                        visible: root.analysis && root.analysis.phaseChannel("Voltage", modelData) >= 0
                                 && root.analysis.phaseChannel("Current", modelData) >= 0
                        Rectangle { width: 14; height: 2; anchors.verticalCenter: parent.verticalCenter; color: root.analysis ? root.analysis.phaseColorForName(modelData) : "#777" }
                        Label { text: modelData; color: "#4f585f"; font.pixelSize: 8 }
                    }
                }
                Label { text: "● C1   ○ C2"; color: "#6d757c"; font.pixelSize: 8 }
            }
        }
    }
}
