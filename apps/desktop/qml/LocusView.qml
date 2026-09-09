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
    property string analysisMode: "distance"

    readonly property bool distanceMode: analysisMode === "distance"
    readonly property real kLMagnitude: zoneController ? zoneController.groundingFactorMagnitude : 0.0
    readonly property real kLAngle: zoneController ? zoneController.groundingFactorAngle : 0.0
    readonly property var earthLoops: ["L1-E", "L2-E", "L3-E"]
    readonly property var phaseLoops: ["L1-L2", "L2-L3", "L3-L1"]

    readonly property var cursorAValue: {
        const representationDependency = valueRepresentation
        return distanceMode && analysis
                ? analysis.distanceLoopAt(selectedLoop, cursorATime, kLMagnitude, kLAngle)
                : ({valid:false})
    }
    readonly property var cursorBValue: {
        const representationDependency = valueRepresentation
        return distanceMode && analysis
                ? analysis.distanceLoopAt(selectedLoop, cursorBTime, kLMagnitude, kLAngle)
                : ({valid:false})
    }
    readonly property var earthSeries: {
        const representationDependency = valueRepresentation
        return distanceMode ? buildDistanceSeries(earthLoops) : []
    }
    readonly property var phaseSeries: {
        const representationDependency = valueRepresentation
        return distanceMode ? buildDistanceSeries(phaseLoops) : []
    }
    readonly property var earthZones: {
        const representationDependency = valueRepresentation
        const reactiveCount = zoneController ? zoneController.zoneCount : 0
        const reactiveSource = zoneController ? zoneController.sourceName : ""
        return distanceMode ? zonesForFamily(earthLoops) : []
    }
    readonly property var phaseZones: {
        const representationDependency = valueRepresentation
        const reactiveCount = zoneController ? zoneController.zoneCount : 0
        const reactiveSource = zoneController ? zoneController.sourceName : ""
        return distanceMode ? zonesForFamily(phaseLoops) : []
    }
    readonly property var rawSeries: {
        const representationDependency = valueRepresentation
        if (distanceMode || !analysis || visibleDuration <= 0) return []
        let result = []
        const phases = ["L1", "L2", "L3"]
        const steps = 180
        for (let phase of phases) {
            const voltage = analysis.phaseChannel("Voltage", phase)
            const current = analysis.phaseChannel("Current", phase)
            if (voltage < 0 || current < 0) continue
            let points = []
            for (let n = 0; n < steps; ++n) {
                const time = viewStart + visibleDuration * n / Math.max(1, steps - 1)
                const z = analysis.impedanceAt(voltage, current, time)
                if (z.valid && Number.isFinite(z.r) && Number.isFinite(z.x)) points.push({valid:true, r:z.r, x:z.x})
            }
            result.push({
                phase: phase,
                color: analysis.phaseColorForName(phase),
                points: points,
                cursorA: analysis.impedanceAt(voltage, current, cursorATime),
                cursorB: analysis.impedanceAt(voltage, current, cursorBTime)
            })
        }
        return result
    }
    readonly property color selectedLoopColor: loopColor(selectedLoop)

    function loopColor(loop) {
        if (loop === "L1-E") return "#178a3a"
        if (loop === "L2-E") return "#d000c8"
        if (loop === "L3-E") return "#1769d2"
        if (loop === "L1-L2") return "#d000c8"
        if (loop === "L2-L3") return "#1769d2"
        if (loop === "L3-L1") return "#07977e"
        return "#6f7780"
    }

    function buildDistanceSeries(loops) {
        if (!analysis || visibleDuration <= 0) return []
        let result = []
        for (let loop of loops) {
            if (!analysis.distanceLoopAvailable(loop)) continue
            result.push({
                loop: loop,
                color: loopColor(loop),
                points: analysis.distanceLocus(loop, viewStart, visibleDuration, 1600, kLMagnitude, kLAngle),
                cursorA: analysis.distanceLoopAt(loop, cursorATime, kLMagnitude, kLAngle),
                cursorB: analysis.distanceLoopAt(loop, cursorBTime, kLMagnitude, kLAngle)
            })
        }
        return result
    }

    function zonesForFamily(loops) {
        if (!zoneController || !zoneController.hasZones) return []
        let result = []
        let seen = ({})
        for (let loop of loops) {
            const zones = zoneController.zonesForLoop(loop, valueRepresentation)
            for (let zone of zones) {
                const key = String(zone.index) + "|" + zone.label + "|" + zone.faultLoop + "|" + zone.kind
                if (seen[key]) continue
                seen[key] = true
                result.push(zone)
            }
        }
        return result
    }

    function formatOhm(value) {
        if (!Number.isFinite(value)) return "—"
        const absolute = Math.abs(value)
        if (absolute >= 1000) return value.toFixed(1) + " Ω"
        if (absolute >= 100) return value.toFixed(2) + " Ω"
        if (absolute >= 10) return value.toFixed(3) + " Ω"
        return value.toFixed(4) + " Ω"
    }

    function formatAmp(value) {
        if (!Number.isFinite(value)) return "—"
        if (Math.abs(value) >= 1000) return (value / 1000).toFixed(3) + " kA"
        if (Math.abs(value) >= 1) return value.toFixed(3) + " A"
        return (value * 1000).toFixed(2) + " mA"
    }

    function cursorSummary(value) {
        if (!value || !value.valid) return "—"
        return "R " + formatOhm(value.r) + "   X " + formatOhm(value.x)
             + "   |Z| " + formatOhm(value.magnitude) + "   ∠ " + value.angle.toFixed(2) + "°"
    }

    function ensureAvailableLoop() {
        if (!analysis || !distanceMode) return
        if (analysis.distanceLoopAvailable(selectedLoop)) return
        const loops = earthLoops.concat(phaseLoops)
        for (let loop of loops) {
            if (analysis.distanceLoopAvailable(loop)) {
                selectedLoop = loop
                return
            }
        }
    }

    function requestRepaint() { locusCanvas.requestPaint() }
    onEarthSeriesChanged: requestRepaint()
    onPhaseSeriesChanged: requestRepaint()
    onEarthZonesChanged: requestRepaint()
    onPhaseZonesChanged: requestRepaint()
    onRawSeriesChanged: requestRepaint()
    onCursorAValueChanged: requestRepaint()
    onCursorBValueChanged: requestRepaint()
    onSelectedLoopChanged: requestRepaint()
    onAnalysisModeChanged: {
        ensureAvailableLoop()
        requestRepaint()
    }
    onWidthChanged: requestRepaint()
    onHeightChanged: requestRepaint()
    Component.onCompleted: ensureAvailableLoop()

    Connections {
        target: root.document
        function onDocumentChanged() {
            root.ensureAvailableLoop()
            root.requestRepaint()
        }
        function onRepresentationChanged() { root.requestRepaint() }
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
                ToolButton {
                    text: "Protection"
                    checkable: true
                    checked: root.distanceMode
                    font.pixelSize: 8
                    onClicked: root.analysisMode = "distance"
                }
                ToolButton {
                    text: "Raw V/I"
                    checkable: true
                    checked: !root.distanceMode
                    font.pixelSize: 8
                    onClicked: root.analysisMode = "raw"
                    ToolTip.visible: hovered
                    ToolTip.text: "Diagnostic phase V/I only — not a compensated distance measuring loop"
                }

                Rectangle { visible: root.distanceMode; width: 1; height: 22; color: "#c1c6ca"; Layout.leftMargin: 4; Layout.rightMargin: 3 }
                Label { visible: root.distanceMode; text: "INSPECT"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Repeater {
                    model: root.earthLoops.concat(root.phaseLoops)
                    ToolButton {
                        required property string modelData
                        visible: root.distanceMode
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

                Rectangle { visible: root.distanceMode; width: 1; height: 22; color: "#c1c6ca"; Layout.leftMargin: 3; Layout.rightMargin: 3 }
                ToolButton {
                    visible: root.distanceMode
                    text: root.zoneController && root.zoneController.hasZones ? "Zones ✓" : "Load RIO/XRIO"
                    font.pixelSize: 8
                    onClicked: zoneDialog.open()
                }
                ToolButton {
                    visible: root.distanceMode && root.zoneController && root.zoneController.hasZones
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
            Layout.preferredHeight: 34
            color: root.distanceMode ? "#f5f6f7" : "#fff8e8"
            border.color: root.distanceMode ? "#d3d7da" : "#e1c98d"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 7

                Label {
                    visible: !root.distanceMode
                    text: "RAW DIAGNOSTIC · Va/Ia, Vb/Ib, Vc/Ic · relay zones hidden"
                    color: "#785b1a"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }

                Label {
                    text: "EARTH kL"
                    visible: root.distanceMode
                    color: "#697178"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
                TextField {
                    visible: root.distanceMode
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 24
                    text: root.kLMagnitude.toFixed(4)
                    horizontalAlignment: TextInput.AlignRight
                    font.pixelSize: 8
                    selectByMouse: true
                    validator: DoubleValidator { bottom: 0.0; notation: DoubleValidator.StandardNotation }
                    onEditingFinished: if (root.zoneController) root.zoneController.groundingFactorMagnitude = Number(text)
                }
                Label { visible: root.distanceMode; text: "∠"; color: "#697178"; font.pixelSize: 8 }
                TextField {
                    visible: root.distanceMode
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
                    visible: root.distanceMode
                    text: "° · " + (root.zoneController ? root.zoneController.groundingFactorSource : "manual")
                    color: root.zoneController && root.zoneController.groundingFactorValid ? "#626b72" : "#9a6b1f"
                    font.pixelSize: 8
                }
                Label {
                    visible: root.distanceMode && (!root.zoneController || !root.zoneController.groundingFactorValid || root.kLMagnitude === 0)
                    text: "UNCOMPENSATED EARTH LOOPS"
                    color: "#9a5c00"
                    font.pixelSize: 7
                    font.weight: Font.DemiBold
                }

                Rectangle { visible: root.distanceMode; width: 1; height: 18; color: "#ccd0d3"; Layout.leftMargin: 4; Layout.rightMargin: 3 }
                Label {
                    visible: root.distanceMode
                    text: "I floor " + (root.analysis ? root.formatAmp(root.analysis.distanceCurrentFloor()) : "—")
                    color: "#687078"
                    font.pixelSize: 7
                    ToolTip.visible: hovered
                    ToolTip.text: "Locus points below 0.1% of the record current peak are rejected to suppress open-breaker V/I artifacts"
                }
                Rectangle { visible: root.distanceMode; width: 1; height: 18; color: "#ccd0d3" }
                Label {
                    visible: root.distanceMode
                    text: root.zoneController ? root.zoneController.status : "No zone model"
                    color: "#687078"
                    font.pixelSize: 7
                    elide: Text.ElideRight
                    Layout.maximumWidth: 370
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: root.distanceMode && root.zoneController && root.zoneController.hasZones
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
                visible: root.distanceMode
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                columns: 6
                rowSpacing: 1
                columnSpacing: 12

                Label { text: "CURSOR"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "LOOP"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "TIME"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "LOOP IMPEDANCE"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold; Layout.fillWidth: true }
                Label { text: "I MEAS"; color: "#737b81"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "" }

                Label { text: "C1"; color: "#245ba7"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { text: root.selectedLoop; color: root.selectedLoopColor; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { text: root.document ? ((root.cursorATime - root.document.triggerOffsetSeconds) * 1000).toFixed(3) + " ms" : "—"; color: "#333b41"; font.pixelSize: 8 }
                Label { text: root.cursorSummary(root.cursorAValue); color: "#252b30"; font.pixelSize: 8; Layout.fillWidth: true }
                Label { text: root.cursorAValue.valid ? root.formatAmp(root.cursorAValue.measuringCurrent) : "—"; color: "#505960"; font.pixelSize: 8 }
                Rectangle { width: 9; height: 9; radius: 5; color: root.selectedLoopColor }

                Label { text: "C2"; color: "#b77900"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { text: root.selectedLoop; color: root.selectedLoopColor; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { text: root.document ? ((root.cursorBTime - root.document.triggerOffsetSeconds) * 1000).toFixed(3) + " ms" : "—"; color: "#333b41"; font.pixelSize: 8 }
                Label { text: root.cursorSummary(root.cursorBValue); color: "#252b30"; font.pixelSize: 8; Layout.fillWidth: true }
                Label { text: root.cursorBValue.valid ? root.formatAmp(root.cursorBValue.measuringCurrent) : "—"; color: "#505960"; font.pixelSize: 8 }
                Rectangle { width: 10; height: 10; radius: 6; color: "transparent"; border.width: 2; border.color: root.selectedLoopColor }
            }

            RowLayout {
                visible: !root.distanceMode
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 18
                Label {
                    text: "C1 " + (root.document ? ((root.cursorATime - root.document.triggerOffsetSeconds) * 1000).toFixed(3) + " ms" : "—")
                    color: "#245ba7"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
                Label {
                    text: "C2 " + (root.document ? ((root.cursorBTime - root.document.triggerOffsetSeconds) * 1000).toFixed(3) + " ms" : "—")
                    color: "#b77900"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
                Rectangle { width: 1; height: 18; color: "#d0d4d7" }
                Repeater {
                    model: root.rawSeries
                    Row {
                        required property var modelData
                        spacing: 5
                        Rectangle { width: 14; height: 2; anchors.verticalCenter: parent.verticalCenter; color: modelData.color }
                        Label { text: modelData.phase + " V/I"; color: "#4f585f"; font.pixelSize: 8 }
                    }
                }
                Item { Layout.fillWidth: true }
                Label { text: "● C1   ○ C2"; color: "#6d757c"; font.pixelSize: 8 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.distanceMode && root.zoneController && root.zoneController.compatibilityWarning.length ? 23 : 0
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
                anchors.margins: 6
                antialiasing: true

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (!root.analysis || width < 180 || height < 150) return

                    function finitePoint(point) {
                        return point && point.valid !== false && Number.isFinite(point.r) && Number.isFinite(point.x)
                    }

                    function niceExtent(value) {
                        if (!Number.isFinite(value) || value <= 0) return 1.0
                        const exponent = Math.floor(Math.log(value) / Math.LN10)
                        const base = Math.pow(10, exponent)
                        const normalized = value / base
                        let step = 10
                        if (normalized <= 1) step = 1
                        else if (normalized <= 2) step = 2
                        else if (normalized <= 5) step = 5
                        return step * base
                    }

                    function panelExtents(series, zones) {
                        let rExtent = 1.0
                        let xExtent = 1.0
                        for (let item of series) {
                            for (let p of item.points) {
                                if (!finitePoint(p)) continue
                                rExtent = Math.max(rExtent, Math.abs(p.r))
                                xExtent = Math.max(xExtent, Math.abs(p.x))
                            }
                            if (finitePoint(item.cursorA)) {
                                rExtent = Math.max(rExtent, Math.abs(item.cursorA.r))
                                xExtent = Math.max(xExtent, Math.abs(item.cursorA.x))
                            }
                            if (finitePoint(item.cursorB)) {
                                rExtent = Math.max(rExtent, Math.abs(item.cursorB.r))
                                xExtent = Math.max(xExtent, Math.abs(item.cursorB.x))
                            }
                        }
                        for (let zone of zones) {
                            if (zone.kind === "circle") {
                                rExtent = Math.max(rExtent, Math.abs(zone.centerR) + Math.abs(zone.radius))
                                xExtent = Math.max(xExtent, Math.abs(zone.centerX) + Math.abs(zone.radius))
                            } else if (zone.kind === "polygon") {
                                for (let p of zone.points) {
                                    if (!Number.isFinite(p.r) || !Number.isFinite(p.x)) continue
                                    rExtent = Math.max(rExtent, Math.abs(p.r))
                                    xExtent = Math.max(xExtent, Math.abs(p.x))
                                }
                            }
                        }
                        return {r:niceExtent(rExtent * 1.08), x:niceExtent(xExtent * 1.08)}
                    }

                    function zoneColor(index) {
                        const colors = ["#138a38", "#1263b4", "#b88900", "#bf2c8a", "#008b91", "#79589b"]
                        return colors[index % colors.length]
                    }

                    function drawZone(ctx, zone, mapX, mapY, index) {
                        const color = zoneColor(index)
                        ctx.strokeStyle = color
                        ctx.fillStyle = "rgba(90,110,120,0.025)"
                        ctx.lineWidth = 1.05
                        if (zone.kind === "circle" && Number.isFinite(zone.radius) && zone.radius > 0) {
                            ctx.beginPath()
                            const segments = 72
                            for (let n = 0; n <= segments; ++n) {
                                const angle = Math.PI * 2 * n / segments
                                const r = zone.centerR + zone.radius * Math.cos(angle)
                                const x = zone.centerX + zone.radius * Math.sin(angle)
                                if (n === 0) ctx.moveTo(mapX(r), mapY(x))
                                else ctx.lineTo(mapX(r), mapY(x))
                            }
                            ctx.closePath(); ctx.fill(); ctx.stroke()
                        } else if (zone.kind === "polygon" && zone.points.length >= 3) {
                            ctx.beginPath()
                            ctx.moveTo(mapX(zone.points[0].r), mapY(zone.points[0].x))
                            for (let n = 1; n < zone.points.length; ++n) ctx.lineTo(mapX(zone.points[n].r), mapY(zone.points[n].x))
                            ctx.closePath(); ctx.fill(); ctx.stroke()
                        }
                    }

                    function drawSeries(ctx, item, mapX, mapY) {
                        const selected = root.selectedLoop === item.loop
                        ctx.strokeStyle = item.color
                        ctx.lineWidth = selected ? 2.0 : 1.35
                        ctx.beginPath()
                        let active = false
                        for (let p of item.points) {
                            if (!finitePoint(p)) {
                                active = false
                                continue
                            }
                            if (!active) {
                                ctx.moveTo(mapX(p.r), mapY(p.x))
                                active = true
                            } else {
                                ctx.lineTo(mapX(p.r), mapY(p.x))
                            }
                        }
                        ctx.stroke()

                        if (selected && finitePoint(item.cursorA)) {
                            ctx.fillStyle = item.color
                            ctx.beginPath(); ctx.arc(mapX(item.cursorA.r), mapY(item.cursorA.x), 4.2, 0, Math.PI * 2); ctx.fill()
                        }
                        if (selected && finitePoint(item.cursorB)) {
                            ctx.strokeStyle = item.color
                            ctx.lineWidth = 2
                            ctx.beginPath(); ctx.arc(mapX(item.cursorB.r), mapY(item.cursorB.x), 6.0, 0, Math.PI * 2); ctx.stroke()
                        }
                    }

                    function drawDistancePanel(ctx, px, py, pw, ph, title, subtitle, series, zones) {
                        ctx.fillStyle = "#fbfcfc"
                        ctx.fillRect(px, py, pw, ph)
                        ctx.strokeStyle = "#bfc6cb"
                        ctx.lineWidth = 1
                        ctx.strokeRect(px + 0.5, py + 0.5, pw - 1, ph - 1)

                        const headerH = 30
                        ctx.fillStyle = "#f0f3f4"
                        ctx.fillRect(px + 1, py + 1, pw - 2, headerH - 1)
                        ctx.strokeStyle = "#d3d8dc"
                        ctx.beginPath(); ctx.moveTo(px, py + headerH); ctx.lineTo(px + pw, py + headerH); ctx.stroke()

                        ctx.fillStyle = "#2f373d"
                        ctx.font = "600 9px sans-serif"
                        ctx.fillText(title, px + 10, py + 12)
                        ctx.fillStyle = "#6a737a"
                        ctx.font = "7px sans-serif"
                        ctx.fillText(subtitle, px + 10, py + 24)

                        let legendX = px + Math.min(245, pw * 0.33)
                        ctx.font = "7px sans-serif"
                        for (let item of series) {
                            ctx.strokeStyle = item.color
                            ctx.lineWidth = root.selectedLoop === item.loop ? 2.1 : 1.4
                            ctx.beginPath(); ctx.moveTo(legendX, py + 15); ctx.lineTo(legendX + 16, py + 15); ctx.stroke()
                            ctx.fillStyle = "#4e575e"
                            ctx.fillText(item.loop, legendX + 20, py + 18)
                            legendX += 61
                        }

                        const left = px + 58
                        const right = px + pw - 18
                        const top = py + headerH + 8
                        const bottom = py + ph - 27
                        const plotW = Math.max(20, right - left)
                        const plotH = Math.max(20, bottom - top)
                        const ext = panelExtents(series, zones)
                        const mapX = r => left + ((r + ext.r) / (2 * ext.r)) * plotW
                        const mapY = x => bottom - ((x + ext.x) / (2 * ext.x)) * plotH
                        const zeroX = mapX(0)
                        const zeroY = mapY(0)

                        ctx.fillStyle = "#ffffff"
                        ctx.fillRect(left, top, plotW, plotH)
                        ctx.strokeStyle = "#e0e5e8"
                        ctx.lineWidth = 1
                        for (let g = -4; g <= 4; ++g) {
                            const rv = ext.r * g / 4
                            const xv = ext.x * g / 4
                            const gx = mapX(rv)
                            const gy = mapY(xv)
                            ctx.beginPath(); ctx.moveTo(gx, top); ctx.lineTo(gx, bottom); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(left, gy); ctx.lineTo(right, gy); ctx.stroke()
                        }

                        ctx.strokeStyle = "#5f6870"
                        ctx.lineWidth = 1.1
                        ctx.beginPath(); ctx.moveTo(left, zeroY); ctx.lineTo(right, zeroY); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(zeroX, top); ctx.lineTo(zeroX, bottom); ctx.stroke()

                        ctx.fillStyle = "#5d676e"
                        ctx.font = "7px sans-serif"
                        for (let g = -4; g <= 4; g += 2) {
                            if (g === 0) continue
                            const rv = ext.r * g / 4
                            const xv = ext.x * g / 4
                            ctx.fillText(rv.toFixed(Math.abs(rv) < 10 ? 2 : 1), mapX(rv) - 11, zeroY + 12)
                            ctx.fillText(xv.toFixed(Math.abs(xv) < 10 ? 2 : 1), zeroX + 5, mapY(xv) + 3)
                        }
                        ctx.fillText("R / Ω", right - 26, zeroY - 5)
                        ctx.fillText("X / Ω", zeroX + 5, top + 9)

                        for (let z = 0; z < zones.length; ++z) drawZone(ctx, zones[z], mapX, mapY, z)
                        for (let item of series) drawSeries(ctx, item, mapX, mapY)

                        ctx.fillStyle = "#6a737a"
                        ctx.font = "7px sans-serif"
                        ctx.fillText("R ±" + ext.r.toFixed(ext.r < 10 ? 2 : 1) + " Ω   X ±" + ext.x.toFixed(ext.x < 10 ? 2 : 1) + " Ω · independent axes", left, py + ph - 9)
                        ctx.fillText(root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY", right - 48, py + ph - 9)
                    }

                    function drawRawPanel(ctx, series) {
                        let rExtent = 1.0
                        let xExtent = 1.0
                        for (let item of series) {
                            for (let p of item.points) {
                                if (!finitePoint(p)) continue
                                rExtent = Math.max(rExtent, Math.abs(p.r))
                                xExtent = Math.max(xExtent, Math.abs(p.x))
                            }
                        }
                        rExtent = niceExtent(rExtent * 1.08)
                        xExtent = niceExtent(xExtent * 1.08)
                        const left = 60
                        const right = width - 20
                        const top = 20
                        const bottom = height - 36
                        const plotW = Math.max(20, right - left)
                        const plotH = Math.max(20, bottom - top)
                        const mapX = r => left + ((r + rExtent) / (2 * rExtent)) * plotW
                        const mapY = x => bottom - ((x + xExtent) / (2 * xExtent)) * plotH
                        ctx.fillStyle = "#ffffff"; ctx.fillRect(left, top, plotW, plotH)
                        ctx.strokeStyle = "#dfe4e7"; ctx.lineWidth = 1
                        for (let g = -4; g <= 4; ++g) {
                            const gx = mapX(rExtent * g / 4)
                            const gy = mapY(xExtent * g / 4)
                            ctx.beginPath(); ctx.moveTo(gx, top); ctx.lineTo(gx, bottom); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(left, gy); ctx.lineTo(right, gy); ctx.stroke()
                        }
                        ctx.strokeStyle = "#667078"
                        ctx.beginPath(); ctx.moveTo(left, mapY(0)); ctx.lineTo(right, mapY(0)); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(mapX(0), top); ctx.lineTo(mapX(0), bottom); ctx.stroke()
                        for (let item of series) {
                            ctx.strokeStyle = item.color; ctx.lineWidth = 1.7; ctx.beginPath()
                            if (item.points.length) {
                                ctx.moveTo(mapX(item.points[0].r), mapY(item.points[0].x))
                                for (let n = 1; n < item.points.length; ++n) ctx.lineTo(mapX(item.points[n].r), mapY(item.points[n].x))
                                ctx.stroke()
                            }
                        }
                        ctx.fillStyle = "#687078"; ctx.font = "8px sans-serif"
                        ctx.fillText("RAW PHASE V/I · diagnostic only · independent R/X scale", left, bottom + 22)
                    }

                    if (root.distanceMode) {
                        const gap = 7
                        const panelH = (height - gap) * 0.5
                        drawDistancePanel(ctx, 0, 0, width, panelH,
                                          "EARTH LOOPS",
                                          "L1-E · L2-E · L3-E · residual-current compensated",
                                          root.earthSeries || [], root.earthZones || [])
                        drawDistancePanel(ctx, 0, panelH + gap, width, panelH,
                                          "PHASE-PHASE LOOPS",
                                          "L1-L2 · L2-L3 · L3-L1",
                                          root.phaseSeries || [], root.phaseZones || [])
                    } else {
                        drawRawPanel(ctx, root.rawSeries || [])
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: root.distanceMode && root.analysis && root.earthSeries.length === 0 && root.phaseSeries.length === 0
                text: "Protection loops cannot be formed from mapped COMTRADE voltage/current channels"
                color: "#8a5f2a"
                font.pixelSize: 10
            }
        }
    }
}
