// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#ffffff"
    border.color: "#cbd0d4"

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

    function valueFor(bin) {
        return displayMode === "rms" ? bin.magnitude : bin.percent
    }
    function valueText(value) {
        if (!Number.isFinite(value)) return "—"
        if (displayMode === "percent") return value.toFixed(value >= 10 ? 1 : 2) + "%"
        return document ? document.formatChannelValue(channelIndex, value) : value.toFixed(3)
    }
    function magnitudeText(bin) {
        return document && bin ? document.formatChannelValue(channelIndex, bin.magnitude) : "—"
    }
    function dominantText() {
        if (!spectrum.valid || spectrum.dominantOrder <= 0) return "—"
        return "H" + spectrum.dominantOrder + " " + spectrum.dominantPercent.toFixed(1) + "%"
    }

    Rectangle {
        id: summary
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 148
        color: "#f5f6f7"
        border.color: "#d0d4d7"

        Rectangle { width: 3; height: parent.height; color: root.phaseColor }

        Label {
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.top: parent.top
            anchors.topMargin: 7
            width: parent.width - 15
            text: root.document && root.channelIndex >= 0 ? root.document.channelName(root.channelIndex) : "—"
            color: root.phaseColor
            font.pixelSize: 9
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Label {
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.top: parent.top
            anchors.topMargin: 23
            text: (root.analysis ? root.analysis.channelPhase(root.channelIndex) : "—")
                  + " · " + (root.valueRepresentation === "primary" ? "PRI" : "SEC")
            color: "#6b7379"
            font.pixelSize: 7
        }

        Grid {
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.top: parent.top
            anchors.topMargin: 42
            columns: 2
            columnSpacing: 4
            rowSpacing: 2

            Label { width: 45; text: "H0 / DC"; color: "#737b81"; font.pixelSize: 7 }
            Label { width: 78; text: root.spectrum.valid && root.document ? root.document.formatChannelValue(root.channelIndex, Math.abs(root.spectrum.dc)) : "—"; color: "#2f363b"; font.pixelSize: 7; elide: Text.ElideRight }
            Label { width: 45; text: "H1 RMS"; color: "#737b81"; font.pixelSize: 7 }
            Label { width: 78; text: root.spectrum.valid && root.document ? root.document.formatChannelValue(root.channelIndex, root.spectrum.fundamental) : "—"; color: "#2f363b"; font.pixelSize: 7; elide: Text.ElideRight }
            Label { width: 45; text: "THD"; color: "#737b81"; font.pixelSize: 7 }
            Label { width: 78; text: root.spectrum.valid ? root.spectrum.thdPercent.toFixed(2) + "%" : "—"; color: "#2f363b"; font.pixelSize: 7 }
            Label { width: 45; text: "Dominant"; color: "#737b81"; font.pixelSize: 7 }
            Label { width: 78; text: root.dominantText(); color: "#2f363b"; font.pixelSize: 7; elide: Text.ElideRight }
        }
    }

    Canvas {
        id: chart
        anchors.left: summary.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 7
        anchors.rightMargin: 10
        anchors.topMargin: 5
        anchors.bottomMargin: 20
        antialiasing: true

        property real currentMaximum: 1.0

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const bins = root.plotBins
            if (!root.spectrum.valid || !bins || bins.length === 0 || width < 100 || height < 45) return

            const annotationTop = 22
            const plotHeight = Math.max(20, height - annotationTop)
            let maximum = root.displayMode === "percent" ? (root.spectrumMode === "full" ? 100.0 : 5.0) : 0.0
            for (let bin of bins) maximum = Math.max(maximum, root.valueFor(bin))
            if (maximum <= 0) maximum = 1
            maximum *= 1.08
            currentMaximum = maximum

            ctx.strokeStyle = "#e1e5e8"
            ctx.lineWidth = 1
            ctx.fillStyle = "#737b81"
            ctx.font = "7px sans-serif"
            for (let g = 0; g <= 3; ++g) {
                const y = annotationTop + plotHeight * g / 3
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                const v = maximum * (3 - g) / 3
                const axisText = root.displayMode === "percent" ? v.toFixed(v >= 20 ? 0 : 1) + "%" : root.valueText(v)
                ctx.fillText(axisText, 2, Math.max(8, y - 2))
            }

            const slot = width / bins.length
            const barWidth = Math.max(3, Math.min(28, slot * 0.58))
            for (let i = 0; i < bins.length; ++i) {
                const bin = bins[i]
                const value = root.valueFor(bin)
                const h = Math.max(0, Math.min(plotHeight, plotHeight * value / maximum))
                const xCenter = slot * i + slot * 0.5
                const x = xCenter - barWidth * 0.5
                const y = annotationTop + plotHeight - h

                ctx.fillStyle = root.phaseColor
                ctx.globalAlpha = bin.order === 1 ? 0.92 : 0.80
                ctx.fillRect(x, y, barWidth, h)
                ctx.globalAlpha = 1.0
                ctx.strokeStyle = "#5d6469"
                ctx.lineWidth = 0.6
                ctx.strokeRect(x, y, barWidth, h)

                const annotateAll = slot >= 58 && bins.length <= 18
                const important = bin.order === 0 || bin.order === 1
                                  || bin.order === root.spectrum.dominantOrder
                                  || bin.percent >= 10.0
                if (annotateAll || important) {
                    ctx.textAlign = "center"
                    ctx.fillStyle = "#4f575d"
                    ctx.font = "7px sans-serif"
                    ctx.fillText(bin.percent.toFixed(bin.percent >= 10 ? 1 : 2) + "%", xCenter, Math.max(8, y - 10))
                    ctx.fillText(root.magnitudeText(bin), xCenter, Math.max(16, y - 2))
                    ctx.textAlign = "start"
                }

                ctx.fillStyle = "#545c62"
                ctx.font = "7px sans-serif"
                ctx.textAlign = "center"
                ctx.fillText(String(bin.order), xCenter, height + 14)
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
            ToolTip.text: {
                const bin = root.hoveredBin
                if (!bin || !root.document) return ""
                const frequency = bin.order * root.document.nominalFrequency
                return root.document.channelName(root.channelIndex)
                       + " · H" + bin.order
                       + " · " + frequency.toFixed(1) + " Hz\n"
                       + root.magnitudeText(bin)
                       + " · " + bin.percent.toFixed(2) + "% H1"
                       + (bin.order > 0 ? " · ∠" + bin.angle.toFixed(1) + "°" : " · DC")
            }
        }
    }

    Label {
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        text: root.spectrumMode === "full" ? "H0 … H" + root.maximumOrder : "H2 … H" + root.maximumOrder
        color: "#81888e"
        font.pixelSize: 6
    }

    onSpectrumChanged: chart.requestPaint()
    onDisplayModeChanged: chart.requestPaint()
    onSpectrumModeChanged: chart.requestPaint()
    onWidthChanged: chart.requestPaint()
    onHeightChanged: chart.requestPaint()
}
