// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#ffffff"
    border.color: "#c8cdd1"

    property var document
    property var analysis
    property int channelIndex: -1
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property int activeCursor: 1
    property int maximumOrder: 15
    property string displayMode: "percent"
    property string valueRepresentation: "secondary"

    readonly property color phaseColor: analysis && channelIndex >= 0 ? analysis.phaseColor(channelIndex) : "#6f7780"
    readonly property var spectrumA: {
        const representationDependency = valueRepresentation
        return analysis && channelIndex >= 0 ? analysis.harmonicSpectrumAt(channelIndex, cursorATime, maximumOrder) : ({valid:false, bins:[]})
    }
    readonly property var spectrumB: {
        const representationDependency = valueRepresentation
        return analysis && channelIndex >= 0 ? analysis.harmonicSpectrumAt(channelIndex, cursorBTime, maximumOrder) : ({valid:false, bins:[]})
    }

    function valueFor(bin) {
        return displayMode === "rms" ? bin.magnitude : bin.percent
    }
    function valueText(value) {
        if (!Number.isFinite(value)) return "—"
        if (displayMode === "percent") return value.toFixed(value >= 10 ? 1 : 2) + "%"
        return document ? document.formatChannelValue(channelIndex, value) : value.toFixed(3)
    }
    function h1Text(spectrum) {
        return spectrum && spectrum.valid && document ? document.formatChannelValue(channelIndex, spectrum.fundamental) : "—"
    }
    function dominantText(spectrum) {
        if (!spectrum || !spectrum.valid || spectrum.dominantOrder <= 0) return "—"
        return "H" + spectrum.dominantOrder + " · " + spectrum.dominantPercent.toFixed(2) + "%"
    }

    Rectangle {
        id: summary
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 235
        color: "#f5f6f7"
        border.color: "#d0d4d7"

        Rectangle { width: 3; height: parent.height; color: root.phaseColor }

        Label {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 8
            width: parent.width - 18
            text: root.document && root.channelIndex >= 0 ? root.document.channelName(root.channelIndex) : "—"
            color: root.phaseColor
            font.pixelSize: 10
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Label {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 25
            text: (root.analysis ? root.analysis.channelPhase(root.channelIndex) : "—")
                  + " · " + (root.valueRepresentation === "primary" ? "PRI" : "SEC")
            color: "#687078"
            font.pixelSize: 8
        }

        Grid {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 7
            anchors.top: parent.top
            anchors.topMargin: 48
            columns: 3
            columnSpacing: 6
            rowSpacing: 3

            Label { width: 52; text: ""; font.pixelSize: 7 }
            Label { width: 70; text: "C1"; color: root.activeCursor === 1 ? "#244f9e" : "#6b737a"; font.pixelSize: 7; font.weight: Font.DemiBold }
            Label { width: 70; text: "C2"; color: root.activeCursor === 2 ? "#b77900" : "#6b737a"; font.pixelSize: 7; font.weight: Font.DemiBold }

            Label { width: 52; text: "H1 RMS"; color: "#697178"; font.pixelSize: 7 }
            Label { width: 70; text: root.h1Text(root.spectrumA); color: "#252b30"; font.pixelSize: 7; elide: Text.ElideRight }
            Label { width: 70; text: root.h1Text(root.spectrumB); color: "#252b30"; font.pixelSize: 7; elide: Text.ElideRight }

            Label { width: 52; text: "THD"; color: "#697178"; font.pixelSize: 7 }
            Label { width: 70; text: root.spectrumA.valid ? root.spectrumA.thdPercent.toFixed(2) + "%" : "—"; color: "#252b30"; font.pixelSize: 7 }
            Label { width: 70; text: root.spectrumB.valid ? root.spectrumB.thdPercent.toFixed(2) + "%" : "—"; color: "#252b30"; font.pixelSize: 7 }

            Label { width: 52; text: "Dominant"; color: "#697178"; font.pixelSize: 7 }
            Label { width: 70; text: root.dominantText(root.spectrumA); color: "#252b30"; font.pixelSize: 7 }
            Label { width: 70; text: root.dominantText(root.spectrumB); color: "#252b30"; font.pixelSize: 7 }
        }
    }

    Canvas {
        id: chart
        anchors.left: summary.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 14
        anchors.topMargin: 12
        anchors.bottomMargin: 28
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const a = root.spectrumA
            const b = root.spectrumB
            if ((!a.valid && !b.valid) || width < 80 || height < 50) return
            const binsA = a.valid ? a.bins : []
            const binsB = b.valid ? b.bins : []
            const count = Math.max(binsA.length, binsB.length)
            if (count < 2) return

            let maximum = root.displayMode === "percent" ? 5.0 : 0.0
            for (let i = 1; i < count; ++i) {
                if (i < binsA.length) maximum = Math.max(maximum, root.valueFor(binsA[i]))
                if (i < binsB.length) maximum = Math.max(maximum, root.valueFor(binsB[i]))
            }
            if (maximum <= 0) maximum = 1
            maximum *= 1.12

            ctx.strokeStyle = "#e1e5e8"
            ctx.lineWidth = 1
            ctx.fillStyle = "#687078"
            ctx.font = "8px sans-serif"
            for (let g = 0; g <= 4; ++g) {
                const y = height * g / 4
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                const v = maximum * (4 - g) / 4
                ctx.fillText(root.displayMode === "percent" ? v.toFixed(1) + "%" : root.valueText(v), 3, Math.max(9, y - 3))
            }

            const distortionCount = count - 1
            const slot = width / distortionCount
            const pairWidth = Math.max(4, Math.min(18, slot * 0.72))
            const half = pairWidth * 0.46
            for (let i = 1; i < count; ++i) {
                const xCenter = slot * (i - 1) + slot * 0.5
                const binA = i < binsA.length ? binsA[i] : null
                const binB = i < binsB.length ? binsB[i] : null
                const valueA = binA ? root.valueFor(binA) : 0
                const valueB = binB ? root.valueFor(binB) : 0
                const hA = Math.max(0, Math.min(height, height * valueA / maximum))
                const hB = Math.max(0, Math.min(height, height * valueB / maximum))

                if (binA) {
                    ctx.fillStyle = root.phaseColor
                    ctx.globalAlpha = root.activeCursor === 1 ? 0.88 : 0.22
                    ctx.fillRect(xCenter - half - 1, height - hA, half, hA)
                    ctx.globalAlpha = 1
                    ctx.strokeStyle = root.phaseColor
                    ctx.lineWidth = root.activeCursor === 1 ? 1.2 : 0.8
                    ctx.strokeRect(xCenter - half - 1, height - hA, half, hA)
                }
                if (binB) {
                    ctx.fillStyle = root.phaseColor
                    ctx.globalAlpha = root.activeCursor === 2 ? 0.88 : 0.12
                    ctx.fillRect(xCenter + 1, height - hB, half, hB)
                    ctx.globalAlpha = 1
                    ctx.strokeStyle = root.phaseColor
                    ctx.lineWidth = root.activeCursor === 2 ? 1.2 : 0.8
                    ctx.strokeRect(xCenter + 1, height - hB, half, hB)
                }

                const order = i + 1
                ctx.fillStyle = "#596168"
                ctx.font = "8px sans-serif"
                if (distortionCount <= 16 || order % 2 === 1 || order === count)
                    ctx.fillText(String(order), xCenter - 3, height + 15)
            }
        }
    }

    Label {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        text: "H2 … H" + root.maximumOrder + "   ·   active cursor = solid"
        color: "#7a8288"
        font.pixelSize: 7
    }

    onSpectrumAChanged: chart.requestPaint()
    onSpectrumBChanged: chart.requestPaint()
    onDisplayModeChanged: chart.requestPaint()
    onActiveCursorChanged: chart.requestPaint()
    onWidthChanged: chart.requestPaint()
    onHeightChanged: chart.requestPaint()
}
