// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f3f4f5"

    property var document
    property var analysis
    property var zoneController: distanceZoneController
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"
    property string selectedLoop: "L1-E"

    readonly property bool earthLoop: selectedLoop.indexOf("-E") >= 0
    readonly property real kLMagnitude: zoneController ? zoneController.groundingFactorMagnitude : 0.0
    readonly property real kLAngle: zoneController ? zoneController.groundingFactorAngle : 0.0
    readonly property var cursorAValue: analysis ? analysis.distanceLoopAt(selectedLoop, cursorATime, kLMagnitude, kLAngle) : ({valid:false})
    readonly property var cursorBValue: analysis ? analysis.distanceLoopAt(selectedLoop, cursorBTime, kLMagnitude, kLAngle) : ({valid:false})
    readonly property var locusPoints: analysis ? analysis.distanceLocus(selectedLoop, viewStart, visibleDuration, 140, kLMagnitude, kLAngle) : []
    readonly property var visibleZones: {
        if (!zoneController) return []
        const reactiveCount = zoneController.zoneCount
        const reactiveSource = zoneController.sourceName
        return zoneController.zonesForLoop(selectedLoop, valueRepresentation)
    }
    readonly property color loopColor: {
        if (!analysis) return "#6f7780"
        if (selectedLoop.indexOf("L1") === 0 || selectedLoop.indexOf("-L1") > 0) return analysis.phaseColorForName("L1")
        if (selectedLoop.indexOf("L2") === 0 || selectedLoop.indexOf("-L2") > 0) return analysis.phaseColorForName("L2")
        return analysis.phaseColorForName("L3")
    }

    function formatOhm(value) {
        if (!Number.isFinite(value)) return "—"
        const absolute = Math.abs(value)
        if (absolute >= 1000) return value.toFixed(1) + " Ω"
        if (absolute >= 100) return value.toFixed(2) + " Ω"
        if (absolute >= 10) return value.toFixed(3) + " Ω"
        return value.toFixed(4) + " Ω"
    }

    function cursorSummary(value) {
        if (!value || !value.valid) return "—"
        return "R " + formatOhm(value.r) + "   X " + formatOhm(value.x)
             + "   |Z| " + formatOhm(value.magnitude) + "   ∠ " + value.angle.toFixed(2) + "°"
    }

    function requestRepaint() { locusCanvas.requestPaint() }
    onLocusPointsChanged: requestRepaint()
    onVisibleZonesChanged: requestRepaint()
    onCursorAValueChanged: requestRepaint()
    onCursorBValueChanged: requestRepaint()
    onWidthChanged: requestRepaint()
    onHeightChanged: requestRepaint()

    Connections {
        target: root.document
        function onDocumentChanged() { root.requestRepaint() }
    }
    Connections {
        target: root.zoneController
        function onModelChanged() { root.requestRepaint() }
        function onGroundingFactorChanged() { root.requestRepaint() }
    }

    FileDialog {
        id: zoneDialog
        title: "Import distance relay zones"
        nameFilters: ["Distance relay files (*.rio *.RIO *.xrio *.XRIO)", "RIO files (*.rio *.RIO)", "XRIO files (*.xrio *.XRIO)"]
        onAccepted: if (root.zoneController) root.zoneController.openFile(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#e8ebed"
            border.color: "#c4c9cd"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 5

                Label {
                    text: "DISTANCE / R-X"
                    color: "#343c43"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }

                Rectangle { width: 1; height: 22; color: "#c1c6ca"; Layout.leftMargin: 4; Layout.rightMargin: 3 }
                Label { text: "LOOP"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Repeater {
                    model: ["L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"]
                    ToolButton {
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: root.selectedLoop === modelData
                        enabled: root.analysis ? root.analysis.distanceLoopAvailable(modelData) : false
                        font.pixelSize: 8
                        onClicked: root.selectedLoop = modelData
                        ToolTip.visible: hovered && !enabled
                        ToolTip.text: "Required voltage/current channels are not mapped in this record"
                    }
                }

                Rectangle { width: 1; height: 22; color: "#c1c6ca"; Layout.leftMargin: 3; Layout.rightMargin: 3 }
                ToolButton {
                    text: root.zoneController && root.zoneController.hasZones ? "Zones ✓" : "Load RIO/XRIO"
                    font.pixelSize: 8
                    onClicked: zoneDialog.open()
                }
                ToolButton {
                    visible: root.zoneController && root.zoneController.hasZones
                    text: "Clear"
                    font.pixelSize: 8
                    onClicked: root.zoneController.clearZones()
                }

                Item { Layout.fillWidth: true }
                Label {
                    text: (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY") + " impedance"
                    color: "#626b72"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#f5f6f7"
            border.color: "#d3d7da"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 7

                Label {
                    text: "kL"
                    visible: root.earthLoop
                    color: "#697178"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
                TextField {
                    visible: root.earthLoop
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 24
                    text: root.kLMagnitude.toFixed(4)
                    horizontalAlignment: TextInput.AlignRight
                    font.pixelSize: 8
                    selectByMouse: true
                    validator: DoubleValidator { bottom: 0.0; notation: DoubleValidator.StandardNotation }
                    onEditingFinished: if (root.zoneController) root.zoneController.groundingFactorMagnitude = Number(text)
                    ToolTip.visible: hovered
                    ToolTip.text: "Ground compensation magnitude |kL|"
                }
                Label { visible: root.earthLoop; text: "∠"; color: "#697178"; font.pixelSize: 8 }
                TextField {
                    visible: root.earthLoop
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 24
                    text: root.kLAngle.toFixed(2)
                    horizontalAlignment: TextInput.AlignRight
                    font.pixelSize: 8
                    selectByMouse: true
                    validator: DoubleValidator { bottom: -360; top: 360; notation: DoubleValidator.StandardNotation }
                    onEditingFinished: if (root.zoneController) root.zoneController.groundingFactorAngle = Number(text)
                }
                Label {
                    visible: root.earthLoop
                    text: "° · " + (root.zoneController ? root.zoneController.groundingFactorSource : "manual")
                    color: root.zoneController && root.zoneController.groundingFactorValid ? "#626b72" : "#9a6b1f"
                    font.pixelSize: 8
                }
                Label {
                    visible: root.earthLoop && (!root.zoneController || !root.zoneController.groundingFactorValid || root.kLMagnitude === 0)
                    text: "UNCOMPENSATED EARTH LOOP"
                    color: "#9a5c00"
                    font.pixelSize: 7
                    font.weight: Font.DemiBold
                }

                Rectangle { visible: root.earthLoop; width: 1; height: 18; color: "#ccd0d3"; Layout.leftMargin: 4; Layout.rightMargin: 3 }
                Label {
                    text: root.zoneController ? root.zoneController.status : "No zone model"
                    color: "#687078"
                    font.pixelSize: 7
                    elide: Text.ElideRight
                    Layout.maximumWidth: 450
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: root.zoneController && root.zoneController.hasZones
                    text: root.zoneController.zoneBaseConversionAvailable
                          ? "zone base conversion verified"
                          : "zone base conversion 1:1 · verify CT/VT metadata"
                    color: root.zoneController && root.zoneController.zoneBaseConversionAvailable ? "#60726a" : "#9a6b1f"
                    font.pixelSize: 7
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#ffffff"
            border.color: "#c9ced2"

            GridLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                columns: 5
                rowSpacing: 1
                columnSpacing: 12

                Label { text: "CURSOR"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "TIME"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "LOOP IMPEDANCE"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold; Layout.fillWidth: true }
                Label { text: "I MEAS"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "" }

                Label { text: "C1"; color: "#245ba7"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { text: root.document ? ((root.cursorATime - root.document.triggerOffsetSeconds) * 1000).toFixed(3) + " ms" : "—"; color: "#333b41"; font.pixelSize: 8 }
                Label { text: root.cursorSummary(root.cursorAValue); color: "#252b30"; font.pixelSize: 8; Layout.fillWidth: true }
                Label { text: root.cursorAValue.valid ? root.cursorAValue.measuringCurrent.toFixed(3) + " A" : "—"; color: "#505960"; font.pixelSize: 8 }
                Rectangle { width: 9; height: 9; radius: 5; color: root.loopColor }

                Label { text: "C2"; color: "#b77900"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { text: root.document ? ((root.cursorBTime - root.document.triggerOffsetSeconds) * 1000).toFixed(3) + " ms" : "—"; color: "#333b41"; font.pixelSize: 8 }
                Label { text: root.cursorSummary(root.cursorBValue); color: "#252b30"; font.pixelSize: 8; Layout.fillWidth: true }
                Label { text: root.cursorBValue.valid ? root.cursorBValue.measuringCurrent.toFixed(3) + " A" : "—"; color: "#505960"; font.pixelSize: 8 }
                Rectangle { width: 10; height: 10; radius: 6; color: "transparent"; border.width: 2; border.color: root.loopColor }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.zoneController && root.zoneController.compatibilityWarning.length ? 23 : 0
            visible: height > 0
            color: "#fff8e8"
            border.color: "#e1c98d"
            Label {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                verticalAlignment: Text.AlignVCenter
                text: root.zoneController ? root.zoneController.compatibilityWarning : ""
                color: "#785b1a"
                font.pixelSize: 7
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#c5cbd0"
            clip: true

            Canvas {
                id: locusCanvas
                anchors.fill: parent
                anchors.margins: 8
                antialiasing: true

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (!root.analysis || width < 160 || height < 120) return

                    const points = root.locusPoints || []
                    const zones = root.visibleZones || []
                    let extent = 1.0
                    for (let p of points) {
                        if (Number.isFinite(p.r) && Number.isFinite(p.x)) extent = Math.max(extent, Math.abs(p.r), Math.abs(p.x))
                    }
                    for (let zone of zones) {
                        if (zone.kind === "circle") {
                            extent = Math.max(extent,
                                              Math.abs(zone.centerR) + zone.radius,
                                              Math.abs(zone.centerX) + zone.radius)
                        } else if (zone.kind === "polygon") {
                            for (let p of zone.points) extent = Math.max(extent, Math.abs(p.r), Math.abs(p.x))
                        }
                    }
                    if (root.cursorAValue.valid) extent = Math.max(extent, Math.abs(root.cursorAValue.r), Math.abs(root.cursorAValue.x))
                    if (root.cursorBValue.valid) extent = Math.max(extent, Math.abs(root.cursorBValue.r), Math.abs(root.cursorBValue.x))
                    extent *= 1.14

                    const left = 62
                    const right = width - 24
                    const top = 18
                    const bottom = height - 42
                    const plotW = Math.max(20, right - left)
                    const plotH = Math.max(20, bottom - top)
                    const cx = left + plotW * 0.5
                    const cy = top + plotH * 0.5
                    const scale = Math.min(plotW, plotH) * 0.5 / extent
                    const mapX = r => cx + r * scale
                    const mapY = x => cy - x * scale

                    ctx.fillStyle = "#fbfcfc"
                    ctx.fillRect(left, top, plotW, plotH)

                    ctx.strokeStyle = "#e2e6e9"
                    ctx.lineWidth = 1
                    for (let g = -4; g <= 4; ++g) {
                        const v = extent * g / 4
                        const gx = mapX(v)
                        const gy = mapY(v)
                        ctx.beginPath(); ctx.moveTo(gx, top); ctx.lineTo(gx, bottom); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(left, gy); ctx.lineTo(right, gy); ctx.stroke()
                    }
                    ctx.strokeStyle = "#828d95"
                    ctx.lineWidth = 1.1
                    ctx.beginPath(); ctx.moveTo(left, cy); ctx.lineTo(right, cy); ctx.stroke()
                    ctx.beginPath(); ctx.moveTo(cx, top); ctx.lineTo(cx, bottom); ctx.stroke()

                    ctx.fillStyle = "#59636b"
                    ctx.font = "8px sans-serif"
                    ctx.fillText("R / Ω", right - 28, cy - 7)
                    ctx.fillText("X / Ω", cx + 7, top + 11)
                    for (let g = -4; g <= 4; g += 2) {
                        if (g === 0) continue
                        const value = extent * g / 4
                        ctx.fillText(value.toFixed(Math.abs(value) < 10 ? 2 : 1), mapX(value) - 11, cy + 14)
                        ctx.fillText(value.toFixed(Math.abs(value) < 10 ? 2 : 1), cx + 7, mapY(value) + 3)
                    }

                    const zoneStroke = ["#8e6b23", "#7b6a91", "#647f72", "#8a7272", "#6c7890", "#8a7d5e"]
                    const zoneFill = ["rgba(205,166,75,0.09)", "rgba(133,108,162,0.07)", "rgba(83,135,110,0.07)", "rgba(155,104,104,0.06)", "rgba(92,111,150,0.06)", "rgba(151,129,80,0.06)"]
                    for (let z = 0; z < zones.length; ++z) {
                        const zone = zones[z]
                        ctx.strokeStyle = zoneStroke[z % zoneStroke.length]
                        ctx.fillStyle = zoneFill[z % zoneFill.length]
                        ctx.lineWidth = 1.2
                        if (zone.kind === "circle") {
                            ctx.beginPath()
                            ctx.arc(mapX(zone.centerR), mapY(zone.centerX), Math.abs(zone.radius * scale), 0, Math.PI * 2)
                            ctx.fill(); ctx.stroke()
                            ctx.fillStyle = zoneStroke[z % zoneStroke.length]
                            const label = zone.label && zone.label.length ? zone.label : "Z" + zone.index
                            ctx.fillText(label, mapX(zone.centerR) + 5, mapY(zone.centerX) - 5)
                        } else if (zone.kind === "polygon" && zone.points.length >= 3) {
                            ctx.beginPath()
                            ctx.moveTo(mapX(zone.points[0].r), mapY(zone.points[0].x))
                            for (let p = 1; p < zone.points.length; ++p) ctx.lineTo(mapX(zone.points[p].r), mapY(zone.points[p].x))
                            ctx.closePath(); ctx.fill(); ctx.stroke()
                            ctx.fillStyle = zoneStroke[z % zoneStroke.length]
                            const label = zone.label && zone.label.length ? zone.label : "Z" + zone.index
                            ctx.fillText(label, mapX(zone.points[0].r) + 5, mapY(zone.points[0].x) - 5)
                        }
                    }

                    if (points.length >= 2) {
                        ctx.strokeStyle = root.loopColor
                        ctx.lineWidth = 2.0
                        ctx.beginPath()
                        let started = false
                        for (let p of points) {
                            if (!Number.isFinite(p.r) || !Number.isFinite(p.x)) continue
                            if (!started) {
                                ctx.moveTo(mapX(p.r), mapY(p.x)); started = true
                            } else ctx.lineTo(mapX(p.r), mapY(p.x))
                        }
                        if (started) ctx.stroke()
                    }

                    if (root.cursorAValue.valid) {
                        ctx.fillStyle = root.loopColor
                        ctx.beginPath(); ctx.arc(mapX(root.cursorAValue.r), mapY(root.cursorAValue.x), 5.0, 0, Math.PI * 2); ctx.fill()
                    }
                    if (root.cursorBValue.valid) {
                        ctx.strokeStyle = root.loopColor
                        ctx.lineWidth = 2.2
                        ctx.beginPath(); ctx.arc(mapX(root.cursorBValue.r), mapY(root.cursorBValue.x), 7.0, 0, Math.PI * 2); ctx.stroke()
                    }

                    ctx.fillStyle = "#687078"
                    ctx.font = "8px sans-serif"
                    ctx.fillText(root.selectedLoop + " · full-cycle fundamental RMS · "
                                 + (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY"), left + 4, bottom + 25)
                    ctx.fillText("● C1   ○ C2", right - 72, bottom + 25)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: root.analysis && !root.analysis.distanceLoopAvailable(root.selectedLoop)
                text: "Selected protection loop cannot be formed from mapped COMTRADE voltage/current channels"
                color: "#8a5f2a"
                font.pixelSize: 10
            }
        }
    }
}
