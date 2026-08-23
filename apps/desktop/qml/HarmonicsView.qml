// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f6f7f8"
    property var document
    property var analysis
    property int channelIndex: -1
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property int activeCursor: 1
    property int maximumOrder: 15
    readonly property real cursorTime: activeCursor === 2 ? cursorBTime : cursorATime

    function requestRepaint() { harmonicCanvas.requestPaint() }
    onCursorTimeChanged: requestRepaint()
    onChannelIndexChanged: requestRepaint()
    onMaximumOrderChanged: requestRepaint()
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
            Label { text: "HARMONICS"; color: "#394149"; font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 0.6 }
            Label {
                text: root.document && root.channelIndex >= 0 ? root.document.channelName(root.channelIndex) : "No measuring signal"
                color: root.analysis && root.channelIndex >= 0 ? root.analysis.phaseColor(root.channelIndex) : "#6f7780"
                font.pixelSize: 9
                font.weight: Font.DemiBold
            }
            Label {
                text: "Cursor " + root.activeCursor + " · " + (root.document ? ((root.cursorTime - root.document.triggerOffsetSeconds) * 1000.0).toFixed(3) + " ms" : "—")
                color: root.activeCursor === 1 ? "#2466b3" : "#c78100"
                font.pixelSize: 8
            }
            Item { Layout.fillWidth: true }
            Label { text: "Full-cycle DFT · RMS magnitude · % fundamental"; color: "#747c83"; font.pixelSize: 8 }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#c5cbd0"

            Canvas {
                id: harmonicCanvas
                anchors.fill: parent
                anchors.leftMargin: 48
                anchors.rightMargin: 18
                anchors.topMargin: 18
                anchors.bottomMargin: 44
                antialiasing: true

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (!root.analysis || root.channelIndex < 0 || width < 80 || height < 80) return
                    const data = root.analysis.harmonicsAt(root.channelIndex, root.cursorTime, root.maximumOrder)
                    if (!data || data.length === 0) return
                    let maxPercent = 100
                    for (let d of data) maxPercent = Math.max(maxPercent, d.percent)
                    maxPercent *= 1.08

                    ctx.strokeStyle = "#dfe4e7"
                    ctx.lineWidth = 1
                    for (let g = 0; g <= 5; ++g) {
                        const y = height * g / 5
                        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                    }
                    ctx.fillStyle = "#5d666d"
                    ctx.font = "8px sans-serif"
                    for (let g = 0; g <= 5; ++g) {
                        const value = maxPercent * (5 - g) / 5
                        ctx.fillText(value.toFixed(0) + "%", -42, height * g / 5 + 3)
                    }

                    const color = root.analysis.phaseColor(root.channelIndex)
                    const slot = width / data.length
                    const barWidth = Math.max(3, slot * 0.58)
                    for (let n = 0; n < data.length; ++n) {
                        const d = data[n]
                        const h = Math.max(0, Math.min(height, height * d.percent / maxPercent))
                        const x = slot * n + (slot - barWidth) * 0.5
                        ctx.fillStyle = color
                        ctx.globalAlpha = d.order === 1 ? 0.88 : 0.72
                        ctx.fillRect(x, height - h, barWidth, h)
                        ctx.globalAlpha = 1.0
                        ctx.fillStyle = "#505960"
                        ctx.fillText(String(d.order), x + barWidth * 0.35, height + 16)
                        if (d.order <= 7 || d.percent >= 5) {
                            ctx.fillStyle = "#606970"
                            ctx.fillText(d.percent.toFixed(1) + "%", x, Math.max(10, height - h - 5))
                        }
                    }
                }
            }

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.top: parent.top
                anchors.topMargin: 8
                text: "% H1"
                color: "#6b747a"
                font.pixelSize: 8
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 7
                text: "Harmonic order"
                color: "#6b747a"
                font.pixelSize: 8
            }
        }
    }
}
