// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#ffffff"
    border.color: "#c5ccd1"
    radius: 2

    property var document
    property var analysis
    property var snapshot
    property int channelIndex: -1
    property real cursorTime: 0.0
    property int maximumOrder: 15
    property string displayMode: "percent"
    property string spectrumMode: "full"
    property string valueRepresentation: "secondary"

    readonly property color phaseColor: analysis && channelIndex >= 0 ? analysis.phaseColor(channelIndex) : "#6f7780"
    readonly property var spectrum: {
        const representationDependency = valueRepresentation
        return snapshot && channelIndex >= 0
               ? snapshot.spectrumAt(channelIndex, cursorTime, maximumOrder)
               : ({valid:false, bins:[]})
    }
    readonly property var plotBins: {
        if (!spectrum.valid) return []
        if (spectrumMode === "distortion") {
            let out = []
            for (let bin of spectrum.bins) if (bin.order >= 2) out.push(bin)
            return out
        }
        return spectrum.bins
    }
    property var hoveredBin: null

    function valueFor(bin) { return displayMode === "rms" ? bin.magnitude : bin.percent }
    function valueText(value) {
        if (!Number.isFinite(value)) return "—"
        if (displayMode === "percent") return value.toFixed(value >= 10 ? 1 : 2) + "%"
        return document ? document.formatChannelValue(channelIndex, value) : value.toFixed(3)
    }
    function magnitudeText(bin) { return document && bin ? document.formatChannelValue(channelIndex, bin.magnitude) : "—" }
    function dominantText() {
        if (!spectrum.valid || spectrum.dominantOrder <= 0) return "—"
        return "H" + spectrum.dominantOrder + "  " + spectrum.dominantPercent.toFixed(1) + "%"
    }

    Rectangle {
        id: summary
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 184
        color: "#f5f7f8"
        border.color: "#d0d5d9"

        Rectangle { width: 4; height: parent.height; color: root.phaseColor }

        Label {
            anchors.left: parent.left; anchors.leftMargin: 12; anchors.top: parent.top; anchors.topMargin: 10
            width: parent.width - 20
            text: root.document && root.channelIndex >= 0 ? root.document.channelName(root.channelIndex) : "—"
            color: root.phaseColor; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight
        }
        Label {
            anchors.left: parent.left; anchors.leftMargin: 12; anchors.top: parent.top; anchors.topMargin: 30
            text: (root.analysis ? root.analysis.channelPhase(root.channelIndex) : "—")
                  + " · " + (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
            color: "#616d75"; font.pixelSize: 9
        }

        Grid {
            anchors.left: parent.left; anchors.leftMargin: 12; anchors.right: parent.right; anchors.rightMargin: 8
            anchors.top: parent.top; anchors.topMargin: 56
            columns: 2; columnSpacing: 8; rowSpacing: 5

            Label { width: 58; text: "H0 / DC"; color: "#6a747c"; font.pixelSize: 9 }
            Label { width: 96; text: root.spectrum.valid && root.document ? root.document.formatChannelValue(root.channelIndex, Math.abs(root.spectrum.dc)) : "—"; color: "#273139"; font.pixelSize: 10; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Label { width: 58; text: "H1 RMS"; color: "#6a747c"; font.pixelSize: 9 }
            Label { width: 96; text: root.spectrum.valid && root.document ? root.document.formatChannelValue(root.channelIndex, root.spectrum.fundamental) : "—"; color: "#273139"; font.pixelSize: 10; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Label { width: 58; text: "THD"; color: "#6a747c"; font.pixelSize: 9 }
            Label { width: 96; text: root.spectrum.valid ? root.spectrum.thdPercent.toFixed(2) + "%" : "—"; color: root.spectrum.valid && root.spectrum.thdPercent >= 5 ? "#8a5b00" : "#273139"; font.pixelSize: 10; font.weight: Font.DemiBold }
            Label { width: 58; text: "Dominant"; color: "#6a747c"; font.pixelSize: 9 }
            Label { width: 96; text: root.dominantText(); color: "#273139"; font.pixelSize: 10; font.weight: Font.DemiBold; elide: Text.ElideRight }
        }
    }

    Canvas {
        id: chart
        anchors.left: summary.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 27
        antialiasing: true
        property real currentMaximum: 1.0

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const bins = root.plotBins
            if (!root.spectrum.valid || !bins || bins.length === 0 || width < 100 || height < 55) return

            const annotationTop = 28
            const plotHeight = Math.max(24, height - annotationTop)
            let maximum = root.displayMode === "percent" ? (root.spectrumMode === "full" ? 100.0 : 5.0) : 0.0
            for (let bin of bins) maximum = Math.max(maximum, root.valueFor(bin))
            if (maximum <= 0) maximum = 1
            maximum *= 1.08
            currentMaximum = maximum

            ctx.strokeStyle = "#dce2e6"
            ctx.lineWidth = 1
            ctx.fillStyle = "#5f6a72"
            ctx.font = "9px sans-serif"
            for (let g = 0; g <= 4; ++g) {
                const y = annotationTop + plotHeight * g / 4
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                const v = maximum * (4 - g) / 4
                const axisText = root.displayMode === "percent" ? v.toFixed(v >= 20 ? 0 : 1) + "%" : root.valueText(v)
                ctx.fillText(axisText, 3, Math.max(10, y - 3))
            }

            const slot = width / bins.length
            const barWidth = Math.max(4, Math.min(34, slot * 0.58))
            for (let i = 0; i < bins.length; ++i) {
                const bin = bins[i]
                const value = root.valueFor(bin)
                const h = Math.max(0, Math.min(plotHeight, plotHeight * value / maximum))
                const xCenter = slot * i + slot * 0.5
                const x = xCenter - barWidth * 0.5
                const y = annotationTop + plotHeight - h

                ctx.fillStyle = root.phaseColor
                ctx.globalAlpha = bin.order === 1 ? 0.94 : 0.80
                ctx.fillRect(x, y, barWidth, h)
                ctx.globalAlpha = 1.0
                ctx.strokeStyle = "#566069"
                ctx.lineWidth = 0.7
                ctx.strokeRect(x, y, barWidth, h)

                const annotateAll = slot >= 70 && bins.length <= 18
                const important = bin.order === 0 || bin.order === 1
                                  || bin.order === root.spectrum.dominantOrder || bin.percent >= 10.0
                if (annotateAll || important) {
                    ctx.textAlign = "center"
                    ctx.fillStyle = "#354149"
                    ctx.font = "600 9px sans-serif"
                    ctx.fillText(bin.percent.toFixed(bin.percent >= 10 ? 1 : 2) + "%", xCenter, Math.max(10, y - 13))
                    ctx.font = "9px sans-serif"
                    ctx.fillText(root.magnitudeText(bin), xCenter, Math.max(20, y - 3))
                }

                ctx.fillStyle = "#46525a"
                ctx.font = "9px sans-serif"
                ctx.textAlign = "center"
                ctx.fillText("H" + String(bin.order), xCenter, height + 18)
                ctx.textAlign = "start"
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onPositionChanged: mouse => {
                const bins = root.plotBins
                if (!bins || bins.length === 0 || chart.width <= 0) { root.hoveredBin = null; return }
                const slot = chart.width / bins.length
                const index = Math.max(0, Math.min(bins.length - 1, Math.floor(mouse.x / slot)))
                root.hoveredBin = bins[index]
            }
            onExited: root.hoveredBin = null
            ToolTip.visible: root.hoveredBin !== null
            ToolTip.delay: 200
            ToolTip.text: {
                const bin = root.hoveredBin
                if (!bin || !root.document) return ""
                const frequency = bin.order * root.document.nominalFrequency
                return root.document.channelName(root.channelIndex)
                       + " · H" + bin.order + " · " + frequency.toFixed(1) + " Hz\n"
                       + root.magnitudeText(bin) + " · " + bin.percent.toFixed(2) + "% of H1"
                       + (bin.order > 0 ? " · ∠" + bin.angle.toFixed(1) + "°" : " · DC")
            }
        }
    }

    Label {
        anchors.right: parent.right; anchors.rightMargin: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 5
        text: root.spectrumMode === "full" ? "H0 … H" + root.maximumOrder : "H2 … H" + root.maximumOrder
        color: "#6f7980"; font.pixelSize: 9
    }

    onSpectrumChanged: chart.requestPaint()
    onDisplayModeChanged: chart.requestPaint()
    onSpectrumModeChanged: chart.requestPaint()
    onWidthChanged: chart.requestPaint()
    onHeightChanged: chart.requestPaint()
}
