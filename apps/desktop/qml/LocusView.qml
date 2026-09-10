// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f2f3f4"

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

    // P0 operator visibility state. Values default to visible when no explicit
    // state exists, so newly imported zone models remain visible automatically.
    property var loopVisibility: ({
        "L1-E": true, "L2-E": true, "L3-E": true,
        "L1-L2": true, "L2-L3": true, "L3-L1": true
    })
    property var earthZoneVisibility: []
    property var phaseZoneVisibility: []

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
        return distanceMode ? zonesForFamily(earthLoops) : []
    }
    readonly property var phaseZones: {
        const representationDependency = valueRepresentation
        return distanceMode ? zonesForFamily(phaseLoops) : []
    }
    readonly property var rawSeries: {
        const representationDependency = valueRepresentation
        return distanceMode ? [] : buildRawSeries()
    }
    readonly property color selectedLoopColor: loopColor(selectedLoop)

    function loopColor(loop) {
        if (loop === "L1-E") return "#00923f"
        if (loop === "L2-E") return "#e000d0"
        if (loop === "L3-E") return "#1769d2"
        if (loop === "L1-L2") return "#6789ee"
        if (loop === "L2-L3") return "#00a184"
        if (loop === "L3-L1") return "#b568c4"
        return "#6f7780"
    }

    function signalLegendName(loop) {
        if (loop === "L1-E") return "Z L1E*"
        if (loop === "L2-E") return "Z L2E*"
        if (loop === "L3-E") return "Z L3E*"
        if (loop === "L1-L2") return "Z L12*"
        if (loop === "L2-L3") return "Z L23*"
        if (loop === "L3-L1") return "Z L31*"
        return loop
    }

    function loopVisible(loop) {
        const value = loopVisibility ? loopVisibility[loop] : undefined
        return value === undefined ? true : value
    }

    function toggleLoopVisible(loop) {
        const next = ({})
        const current = loopVisibility || ({})
        for (let key in current) next[key] = current[key]
        next[loop] = !loopVisible(loop)
        loopVisibility = next
        requestRepaint()
    }

    function zoneVisible(earthFamily, index) {
        const values = earthFamily ? earthZoneVisibility : phaseZoneVisibility
        return index >= values.length || values[index] !== false
    }

    function toggleZoneVisible(earthFamily, index) {
        const source = earthFamily ? earthZoneVisibility : phaseZoneVisibility
        const next = source.slice(0)
        while (next.length <= index) next.push(true)
        next[index] = !zoneVisible(earthFamily, index)
        if (earthFamily) earthZoneVisibility = next
        else phaseZoneVisibility = next
        requestRepaint()
    }

    function resetVisibility() {
        loopVisibility = ({
            "L1-E": true, "L2-E": true, "L3-E": true,
            "L1-L2": true, "L2-L3": true, "L3-L1": true
        })
        earthZoneVisibility = []
        phaseZoneVisibility = []
        requestRepaint()
    }

    function fullRecordStart() {
        return document ? document.dataStartSeconds : viewStart
    }

    function fullRecordDuration() {
        if (!document) return visibleDuration
        return Math.max(0.0, document.dataEndSeconds - document.dataStartSeconds)
    }

    function buildDistanceSeries(loops) {
        if (!analysis) return []
        const start = fullRecordStart()
        const duration = fullRecordDuration()
        if (duration <= 0) return []
        let result = []
        for (let loop of loops) {
            if (!analysis.distanceLoopAvailable(loop)) continue
            result.push({
                loop: loop,
                color: loopColor(loop),
                points: analysis.distanceLocus(loop, start, duration, 4000, kLMagnitude, kLAngle),
                cursorA: analysis.distanceLoopAt(loop, cursorATime, kLMagnitude, kLAngle),
                cursorB: analysis.distanceLoopAt(loop, cursorBTime, kLMagnitude, kLAngle)
            })
        }
        return result
    }

    function isOverreachZone(zone) {
        return zone && String(zone.type).toUpperCase().indexOf("OVERREACH") >= 0
    }

    function legacyZoneNumber(zone) {
        if (!zone) return NaN
        const match = String(zone.label).toUpperCase().match(/Z\s*(\d+)/)
        return match && match.length > 1 ? Number(match[1]) : NaN
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

        if (result.some(function(zone) { return root.isOverreachZone(zone) })) {
            result.sort(function(a, b) {
                const an = root.legacyZoneNumber(a)
                const bn = root.legacyZoneNumber(b)
                if (Number.isFinite(an) && Number.isFinite(bn) && an !== bn) return an - bn
                if (Number.isFinite(an) && Number.isFinite(bn) && an === bn) {
                    const ao = root.isOverreachZone(a) ? 1 : 0
                    const bo = root.isOverreachZone(b) ? 1 : 0
                    if (ao !== bo) return ao - bo
                }
                const ai = Number(a.index)
                const bi = Number(b.index)
                if (Number.isFinite(ai) && Number.isFinite(bi)) return ai - bi
                return 0
            })
        }
        return result
    }

    function buildRawSeries() {
        if (!analysis || !document) return []
        let result = []
        const phases = ["L1", "L2", "L3"]
        const start = fullRecordStart()
        const duration = fullRecordDuration()
        const steps = 240
        for (let phase of phases) {
            const voltage = analysis.phaseChannel("Voltage", phase)
            const current = analysis.phaseChannel("Current", phase)
            if (voltage < 0 || current < 0) continue
            let points = []
            for (let n = 0; n < steps; ++n) {
                const time = start + duration * n / Math.max(1, steps - 1)
                const z = analysis.impedanceAt(voltage, current, time)
                points.push(z.valid && Number.isFinite(z.r) && Number.isFinite(z.x)
                            ? {valid:true, r:z.r, x:z.x}
                            : {valid:false})
            }
            result.push({phase:phase, color:analysis.phaseColorForName(phase), points:points})
        }
        return result
    }

    function formatOhm(value) {
        if (!Number.isFinite(value)) return "—"
        const a = Math.abs(value)
        if (a >= 100) return value.toFixed(2) + " Ω"
        if (a >= 10) return value.toFixed(3) + " Ω"
        return value.toFixed(4) + " Ω"
    }

    function compactImpedance(value) {
        if (!value || !value.valid) return "—"
        return "R " + formatOhm(value.r) + "  X " + formatOhm(value.x)
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

    function requestRepaint() {
        if (locusCanvas) locusCanvas.requestPaint()
    }

    onEarthSeriesChanged: requestRepaint()
    onPhaseSeriesChanged: requestRepaint()
    onEarthZonesChanged: requestRepaint()
    onPhaseZonesChanged: requestRepaint()
    onRawSeriesChanged: requestRepaint()
    onCursorAValueChanged: requestRepaint()
    onCursorBValueChanged: requestRepaint()
    onSelectedLoopChanged: requestRepaint()
    onLoopVisibilityChanged: requestRepaint()
    onEarthZoneVisibilityChanged: requestRepaint()
    onPhaseZoneVisibilityChanged: requestRepaint()
    onAnalysisModeChanged: { ensureAvailableLoop(); requestRepaint() }
    onWidthChanged: requestRepaint()
    onHeightChanged: requestRepaint()
    Component.onCompleted: ensureAvailableLoop()

    Connections {
        target: root.document
        function onDocumentChanged() { root.ensureAvailableLoop(); root.resetVisibility() }
        function onRepresentationChanged() { root.requestRepaint() }
    }
    Connections {
        target: root.zoneController
        function onModelChanged() {
            root.earthZoneVisibility = []
            root.phaseZoneVisibility = []
            root.requestRepaint()
        }
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
            Layout.preferredHeight: 31
            color: "#eceeef"
            border.color: "#c8cdd1"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 5

                Label {
                    text: "DISTANCE / R-X"
                    color: "#31383e"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.5
                }
                Rectangle { width: 1; height: 17; color: "#c4c9cd"; Layout.leftMargin: 3; Layout.rightMargin: 2 }
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
                }
                Rectangle { visible: root.distanceMode; width: 1; height: 17; color: "#c4c9cd"; Layout.leftMargin: 2; Layout.rightMargin: 2 }
                Label {
                    visible: root.distanceMode
                    text: "Inspect " + root.selectedLoop
                    color: root.selectedLoopColor
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
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
                ToolButton {
                    visible: root.distanceMode
                    text: "Show all"
                    font.pixelSize: 8
                    onClicked: root.resetVisibility()
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY") + " Ω"
                    color: "#566068"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.distanceMode ? 30 : 24
            color: root.distanceMode ? "#fafbfb" : "#fff8e8"
            border.color: root.distanceMode ? "#d7dbde" : "#e1c98d"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Label { visible: root.distanceMode; text: "kL"; color: "#5b646b"; font.pixelSize: 8; font.weight: Font.DemiBold }
                TextField {
                    visible: root.distanceMode
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 21
                    text: root.kLMagnitude.toFixed(4)
                    horizontalAlignment: TextInput.AlignRight
                    font.pixelSize: 8
                    validator: DoubleValidator { bottom: 0.0; notation: DoubleValidator.StandardNotation }
                    onEditingFinished: if (root.zoneController) root.zoneController.groundingFactorMagnitude = Number(text)
                }
                Label { visible: root.distanceMode; text: "∠"; color: "#646d73"; font.pixelSize: 8 }
                TextField {
                    visible: root.distanceMode
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 21
                    text: root.kLAngle.toFixed(2)
                    horizontalAlignment: TextInput.AlignRight
                    font.pixelSize: 8
                    validator: DoubleValidator { bottom: -360; top: 360; notation: DoubleValidator.StandardNotation }
                    onEditingFinished: if (root.zoneController) root.zoneController.groundingFactorAngle = Number(text)
                }
                Label {
                    visible: root.distanceMode
                    text: "° · " + (root.zoneController ? root.zoneController.groundingFactorSource : "manual")
                    color: "#697178"
                    font.pixelSize: 7
                }
                Label {
                    visible: root.distanceMode && (!root.zoneController || !root.zoneController.groundingFactorValid || Math.abs(root.kLMagnitude) < 1.0e-12)
                    text: "UNCOMPENSATED EARTH LOOPS"
                    color: "#9a5c00"
                    font.pixelSize: 7
                    font.weight: Font.DemiBold
                }
                Rectangle { visible: root.distanceMode; width: 1; height: 14; color: "#d2d6d9"; Layout.leftMargin: 2; Layout.rightMargin: 2 }
                Label { visible: root.distanceMode; text: "C1"; color: "#c47e00"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label {
                    visible: root.distanceMode
                    text: (root.document ? ((root.cursorATime - root.document.triggerOffsetSeconds) * 1000).toFixed(2) + " ms · " : "")
                          + root.compactImpedance(root.cursorAValue)
                    color: "#41494f"
                    font.pixelSize: 8
                }
                Rectangle { visible: root.distanceMode; width: 1; height: 13; color: "#d5d9dc" }
                Label { visible: root.distanceMode; text: "C2"; color: "#008db6"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label {
                    visible: root.distanceMode
                    text: (root.document ? ((root.cursorBTime - root.document.triggerOffsetSeconds) * 1000).toFixed(2) + " ms · " : "")
                          + root.compactImpedance(root.cursorBValue)
                    color: "#41494f"
                    font.pixelSize: 8
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: root.distanceMode
                    text: root.analysis ? "I floor " + (root.analysis.distanceCurrentFloor() * 1000).toFixed(2) + " mA" : ""
                    color: "#7a8186"
                    font.pixelSize: 7
                }
                Label {
                    visible: !root.distanceMode
                    text: "RAW PHASE V/I · diagnostic only"
                    color: "#785b1a"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.distanceMode && root.zoneController && root.zoneController.compatibilityWarning.length ? 19 : 0
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
            color: "#f2f3f4"
            clip: true

            Canvas {
                id: locusCanvas
                anchors.fill: parent
                anchors.margins: 5
                antialiasing: true

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (!root.analysis || width < 220 || height < 160) return

                    function finitePoint(p) {
                        return p && p.valid !== false && Number.isFinite(p.r) && Number.isFinite(p.x)
                    }

                    function percentile(values, q) {
                        if (!values || values.length === 0) return 0.0
                        values.sort(function(a, b) { return a - b })
                        if (values.length === 1) return values[0]
                        const pos = (values.length - 1) * q
                        const lo = Math.floor(pos)
                        const hi = Math.ceil(pos)
                        if (lo === hi) return values[lo]
                        const f = pos - lo
                        return values[lo] * (1.0 - f) + values[hi] * f
                    }

                    function niceCeil(value) {
                        if (!Number.isFinite(value) || value <= 0) return 1.0
                        const exponent = Math.floor(Math.log(value) / Math.LN10)
                        const base = Math.pow(10, exponent)
                        const normalized = value / base
                        const choices = [1, 1.5, 2, 2.5, 3, 4, 5, 7.5, 10]
                        for (let c of choices) if (normalized <= c + 1e-12) return c * base
                        return 10 * base
                    }

                    function niceStep(value) {
                        if (!Number.isFinite(value) || value <= 0) return 1.0
                        const exponent = Math.floor(Math.log(value) / Math.LN10)
                        const base = Math.pow(10, exponent)
                        const normalized = value / base
                        if (normalized <= 1) return 1 * base
                        if (normalized <= 2) return 2 * base
                        if (normalized <= 2.5) return 2.5 * base
                        if (normalized <= 5) return 5 * base
                        return 10 * base
                    }

                    function zoneMagnitude(zones, earthFamily) {
                        let r = 0.0
                        let x = 0.0
                        for (let zi = 0; zi < zones.length; ++zi) {
                            if (!root.zoneVisible(earthFamily, zi)) continue
                            const zone = zones[zi]
                            if (zone.kind === "circle") {
                                r = Math.max(r, Math.abs(zone.centerR) + Math.abs(zone.radius))
                                x = Math.max(x, Math.abs(zone.centerX) + Math.abs(zone.radius))
                            } else if (zone.kind === "polygon") {
                                for (let p of zone.points) {
                                    if (!Number.isFinite(p.r) || !Number.isFinite(p.x)) continue
                                    r = Math.max(r, Math.abs(p.r))
                                    x = Math.max(x, Math.abs(p.x))
                                }
                            }
                        }
                        return {r:r, x:x}
                    }

                    function coreTarget(series, zones, earthFamily) {
                        let rs = []
                        let xs = []
                        for (let item of series) {
                            if (!root.loopVisible(item.loop)) continue
                            for (let p of item.points) {
                                if (!finitePoint(p)) continue
                                rs.push(Math.abs(p.r))
                                xs.push(Math.abs(p.x))
                            }
                        }
                        const zone = zoneMagnitude(zones, earthFamily)
                        const robustR = percentile(rs, 0.80)
                        const robustX = percentile(xs, 0.80)
                        return {
                            r: niceCeil(Math.max(3.0, zone.r, robustR) * 1.10),
                            x: niceCeil(Math.max(3.0, zone.x, robustX) * 1.10)
                        }
                    }

                    function earthZoneColor(index) { return index % 2 === 0 ? "#008a2f" : "#075ec7" }
                    function phaseZoneColor(index) { return index % 2 === 0 ? "#c7a000" : "#e300cb" }

                    function zoneLegendLabel(zone, earthFamily, ordinal, zones) {
                        const legacyOrdered = zones && zones.some(function(z) { return root.isOverreachZone(z) })
                        let index = legacyOrdered ? ordinal + 1 : Number(zone.index)
                        if (!Number.isFinite(index) || index <= 0) index = ordinal + 1
                        return "Zone_" + index + (earthFamily ? "E" : "")
                    }

                    function insidePlot(px, py, left, top, right, bottom) {
                        return px >= left && px <= right && py >= top && py <= bottom
                    }

                    function drawCheckbox(x, y, checked, color) {
                        ctx.save()
                        ctx.fillStyle = "#ffffff"
                        ctx.strokeStyle = checked ? color : "#9ca3a8"
                        ctx.lineWidth = checked ? 1.0 : 0.8
                        ctx.fillRect(x, y, 9, 9)
                        ctx.strokeRect(x + 0.5, y + 0.5, 8, 8)
                        if (checked) {
                            ctx.strokeStyle = color
                            ctx.lineWidth = 1.1
                            ctx.beginPath()
                            ctx.moveTo(x + 2.0, y + 4.7)
                            ctx.lineTo(x + 4.0, y + 6.7)
                            ctx.lineTo(x + 7.2, y + 2.4)
                            ctx.stroke()
                        }
                        ctx.restore()
                    }

                    function drawZone(zone, mapX, mapY, color) {
                        ctx.save()
                        ctx.strokeStyle = color
                        ctx.lineWidth = 0.9
                        ctx.setLineDash([3, 2])
                        if (zone.kind === "circle" && Number.isFinite(zone.radius) && zone.radius > 0) {
                            const segments = 72
                            ctx.beginPath()
                            for (let n = 0; n <= segments; ++n) {
                                const a = Math.PI * 2 * n / segments
                                const px = mapX(zone.centerR + zone.radius * Math.cos(a))
                                const py = mapY(zone.centerX + zone.radius * Math.sin(a))
                                if (n === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
                            }
                            ctx.stroke()
                        } else if (zone.kind === "polygon" && zone.points.length >= 3) {
                            ctx.beginPath()
                            ctx.moveTo(mapX(zone.points[0].r), mapY(zone.points[0].x))
                            for (let n = 1; n < zone.points.length; ++n) ctx.lineTo(mapX(zone.points[n].r), mapY(zone.points[n].x))
                            ctx.closePath()
                            ctx.stroke()
                        }
                        ctx.restore()
                    }

                    function drawCursorCross(point, mapX, mapY, color, selected) {
                        if (!finitePoint(point)) return
                        const px = mapX(point.r)
                        const py = mapY(point.x)
                        const size = selected ? 7.5 : 5.5
                        const stroke = selected ? 1.9 : 1.45
                        ctx.save()
                        ctx.setLineDash([])
                        // White under-stroke preserves cursor visibility over a dense locus.
                        ctx.strokeStyle = "#ffffff"
                        ctx.lineWidth = stroke + 2.2
                        ctx.beginPath()
                        ctx.moveTo(px - size, py); ctx.lineTo(px + size, py)
                        ctx.moveTo(px, py - size); ctx.lineTo(px, py + size)
                        ctx.stroke()
                        ctx.strokeStyle = color
                        ctx.lineWidth = stroke
                        ctx.beginPath()
                        ctx.moveTo(px - size, py); ctx.lineTo(px + size, py)
                        ctx.moveTo(px, py - size); ctx.lineTo(px, py + size)
                        ctx.stroke()
                        ctx.restore()
                    }

                    function drawSampleSquare(px, py, color, selected) {
                        const half = selected ? 2.0 : 1.75
                        ctx.save()
                        ctx.setLineDash([])
                        ctx.fillStyle = "#ffffff"
                        ctx.fillRect(px - half, py - half, half * 2, half * 2)
                        ctx.strokeStyle = color
                        ctx.lineWidth = selected ? 0.95 : 0.8
                        ctx.strokeRect(px - half, py - half, half * 2, half * 2)
                        ctx.restore()
                    }

                    function drawSeries(item, mapX, mapY, left, top, right, bottom) {
                        if (!root.loopVisible(item.loop)) return
                        const selected = root.selectedLoop === item.loop
                        ctx.save()
                        ctx.setLineDash([])
                        ctx.strokeStyle = item.color
                        ctx.globalAlpha = selected ? 1.0 : 0.90
                        ctx.lineWidth = selected ? 1.25 : 0.95
                        ctx.beginPath()
                        let active = false
                        for (let p of item.points) {
                            if (!finitePoint(p)) { active = false; continue }
                            const px = mapX(p.r)
                            const py = mapY(p.x)
                            if (!active) { ctx.moveTo(px, py); active = true }
                            else ctx.lineTo(px, py)
                        }
                        ctx.stroke()
                        ctx.restore()

                        const markerGapPx = 6.5
                        let lastMarkerX = NaN
                        let lastMarkerY = NaN
                        let segmentStarted = false
                        for (let p of item.points) {
                            if (!finitePoint(p)) {
                                segmentStarted = false
                                lastMarkerX = NaN
                                lastMarkerY = NaN
                                continue
                            }
                            const px = mapX(p.r)
                            const py = mapY(p.x)
                            if (!insidePlot(px, py, left, top, right, bottom)) continue
                            if (!segmentStarted || !Number.isFinite(lastMarkerX)
                                    || Math.hypot(px - lastMarkerX, py - lastMarkerY) >= markerGapPx) {
                                drawSampleSquare(px, py, item.color, selected)
                                lastMarkerX = px
                                lastMarkerY = py
                                segmentStarted = true
                            }
                        }

                        // SIGRA semantics: each cursor exists on every visible loop at the
                        // same record time. Selected-loop crosses are slightly stronger.
                        drawCursorCross(item.cursorA, mapX, mapY, "#d88900", selected)
                        drawCursorCross(item.cursorB, mapX, mapY, "#009bc4", selected)
                    }

                    function drawLegend(px, py, pw, earthFamily, series, zones) {
                        const title = earthFamily ? "EARTH LOOPS" : "PHASE-PHASE LOOPS"
                        ctx.font = "600 8px sans-serif"
                        ctx.fillStyle = "#5c656b"
                        ctx.textAlign = "left"
                        ctx.textBaseline = "middle"
                        ctx.fillText(title, px + 8, py + 13)

                        let x = px + 94
                        const y = py + 13
                        ctx.font = "8px sans-serif"

                        for (let z = 0; z < zones.length; ++z) {
                            const color = earthFamily ? earthZoneColor(z) : phaseZoneColor(z)
                            const checked = root.zoneVisible(earthFamily, z)
                            drawCheckbox(x, y - 4.5, checked, color)
                            ctx.save()
                            ctx.strokeStyle = color
                            ctx.globalAlpha = checked ? 1.0 : 0.35
                            ctx.lineWidth = 0.9
                            ctx.setLineDash([3, 2])
                            ctx.beginPath(); ctx.moveTo(x + 13, y); ctx.lineTo(x + 23, y); ctx.stroke()
                            ctx.restore()
                            ctx.fillStyle = checked ? "#333a40" : "#9aa1a6"
                            const label = zoneLegendLabel(zones[z], earthFamily, z, zones)
                            ctx.fillText(label, x + 27, y)
                            x += 72
                        }

                        for (let item of series) {
                            const checked = root.loopVisible(item.loop)
                            drawCheckbox(x, y - 4.5, checked, item.color)
                            ctx.save()
                            ctx.setLineDash([])
                            ctx.strokeStyle = item.color
                            ctx.globalAlpha = checked ? 1.0 : 0.35
                            ctx.lineWidth = root.selectedLoop === item.loop ? 1.25 : 0.95
                            ctx.beginPath(); ctx.moveTo(x + 13, y); ctx.lineTo(x + 26, y); ctx.stroke()
                            ctx.restore()
                            drawSampleSquare(x + 19.5, y, item.color, root.selectedLoop === item.loop)
                            ctx.fillStyle = checked ? (root.selectedLoop === item.loop ? "#11161a" : "#333a40") : "#9aa1a6"
                            ctx.font = root.selectedLoop === item.loop ? "600 8px sans-serif" : "8px sans-serif"
                            ctx.fillText(root.signalLegendName(item.loop), x + 30, y)
                            x += 78
                            if (x > px + pw - 72) break
                        }
                        ctx.textBaseline = "alphabetic"
                    }

                    function drawDistancePanel(px, py, pw, ph, earthFamily, series, zones) {
                        ctx.fillStyle = "#ffffff"
                        ctx.fillRect(px, py, pw, ph)
                        ctx.strokeStyle = "#c5c9cc"
                        ctx.lineWidth = 0.8
                        ctx.strokeRect(px + 0.5, py + 0.5, pw - 1, ph - 1)

                        const legendH = 26
                        drawLegend(px, py, pw, earthFamily, series, zones)
                        ctx.strokeStyle = "#eef0f1"
                        ctx.lineWidth = 0.7
                        ctx.beginPath(); ctx.moveTo(px, py + legendH); ctx.lineTo(px + pw, py + legendH); ctx.stroke()

                        const left = px + 58
                        const right = px + pw - 14
                        const top = py + legendH + 7
                        const bottom = py + ph - 28
                        const plotW = Math.max(20, right - left)
                        const plotH = Math.max(20, bottom - top)
                        const target = coreTarget(series, zones, earthFamily)

                        // Conformal R-X mapping: one ohm is the same number of pixels on R and X.
                        const scale = Math.max(1e-9, Math.min(plotW / (2 * target.r), plotH / (2 * target.x)))
                        const rHalf = plotW / (2 * scale)
                        const xHalf = plotH / (2 * scale)
                        const cx = left + plotW * 0.5
                        const cy = top + plotH * 0.5
                        const mapX = function(r) { return cx + r * scale }
                        const mapY = function(x) { return cy - x * scale }

                        ctx.fillStyle = "#ffffff"
                        ctx.fillRect(left, top, plotW, plotH)

                        const rStep = niceStep(rHalf / 4.0)
                        const xStep = niceStep(xHalf / 3.5)
                        ctx.strokeStyle = "#f3f4f5"
                        ctx.lineWidth = 0.5
                        ctx.setLineDash([])
                        for (let rv = Math.ceil(-rHalf / rStep) * rStep; rv <= rHalf + 1e-9; rv += rStep) {
                            if (Math.abs(rv) < rStep * 0.1) continue
                            const gx = mapX(rv)
                            ctx.beginPath(); ctx.moveTo(gx, top); ctx.lineTo(gx, bottom); ctx.stroke()
                        }
                        for (let xv = Math.ceil(-xHalf / xStep) * xStep; xv <= xHalf + 1e-9; xv += xStep) {
                            if (Math.abs(xv) < xStep * 0.1) continue
                            const gy = mapY(xv)
                            ctx.beginPath(); ctx.moveTo(left, gy); ctx.lineTo(right, gy); ctx.stroke()
                        }

                        ctx.strokeStyle = "#20252a"
                        ctx.lineWidth = 0.95
                        ctx.beginPath(); ctx.moveTo(left, cy); ctx.lineTo(right, cy); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(cx, top); ctx.lineTo(cx, bottom); ctx.stroke()

                        ctx.fillStyle = "#465057"
                        ctx.font = "8px sans-serif"
                        ctx.textBaseline = "top"
                        ctx.textAlign = "center"
                        for (let rv = Math.ceil(-rHalf / rStep) * rStep; rv <= rHalf + 1e-9; rv += rStep) {
                            if (Math.abs(rv) < rStep * 0.1) continue
                            const tx = mapX(rv)
                            ctx.fillText(rv.toFixed(Math.abs(rv) < 10 ? 1 : 0), tx, bottom + 4)
                            ctx.strokeStyle = "#5e666c"
                            ctx.lineWidth = 0.7
                            ctx.beginPath(); ctx.moveTo(tx, cy - 2); ctx.lineTo(tx, cy + 2); ctx.stroke()
                        }
                        ctx.textAlign = "right"
                        ctx.textBaseline = "middle"
                        for (let xv = Math.ceil(-xHalf / xStep) * xStep; xv <= xHalf + 1e-9; xv += xStep) {
                            if (Math.abs(xv) < xStep * 0.1) continue
                            const ty = mapY(xv)
                            ctx.fillText(xv.toFixed(Math.abs(xv) < 10 ? 1 : 0), left - 5, ty)
                            ctx.strokeStyle = "#5e666c"
                            ctx.lineWidth = 0.7
                            ctx.beginPath(); ctx.moveTo(cx - 2, ty); ctx.lineTo(cx + 2, ty); ctx.stroke()
                        }
                        ctx.textAlign = "center"
                        ctx.textBaseline = "alphabetic"
                        ctx.fillText("R/Ohm(" + (root.valueRepresentation === "primary" ? "primary" : "secondary") + ")", cx, py + ph - 7)

                        ctx.save()
                        ctx.beginPath(); ctx.rect(left, top, plotW, plotH); ctx.clip()
                        for (let z = 0; z < zones.length; ++z) {
                            if (!root.zoneVisible(earthFamily, z)) continue
                            drawZone(zones[z], mapX, mapY, earthFamily ? earthZoneColor(z) : phaseZoneColor(z))
                        }
                        for (let item of series) drawSeries(item, mapX, mapY, left, top, right, bottom)
                        ctx.restore()

                        ctx.save()
                        ctx.translate(px + 12, cy)
                        ctx.rotate(-Math.PI / 2)
                        ctx.fillStyle = "#40484e"
                        ctx.font = "8px sans-serif"
                        ctx.textAlign = "center"
                        ctx.fillText("X/Ohm(" + (root.valueRepresentation === "primary" ? "primary" : "secondary") + ")", 0, 0)
                        ctx.restore()
                    }

                    function drawRawPanel(series) {
                        ctx.fillStyle = "#ffffff"
                        ctx.fillRect(0, 0, width, height)
                        let rs = []
                        let xs = []
                        for (let item of series) for (let p of item.points) if (finitePoint(p)) { rs.push(Math.abs(p.r)); xs.push(Math.abs(p.x)) }
                        const rTarget = niceCeil(Math.max(1, percentile(rs, 0.90)) * 1.1)
                        const xTarget = niceCeil(Math.max(1, percentile(xs, 0.90)) * 1.1)
                        const left = 58, right = width - 14, top = 18, bottom = height - 28
                        const plotW = right - left, plotH = bottom - top
                        const scale = Math.max(1e-9, Math.min(plotW / (2 * rTarget), plotH / (2 * xTarget)))
                        const cx = left + plotW * 0.5, cy = top + plotH * 0.5
                        const mapX = function(r) { return cx + r * scale }
                        const mapY = function(x) { return cy - x * scale }
                        ctx.strokeStyle = "#20252a"; ctx.lineWidth = 0.9
                        ctx.beginPath(); ctx.moveTo(left, cy); ctx.lineTo(right, cy); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(cx, top); ctx.lineTo(cx, bottom); ctx.stroke()
                        ctx.save(); ctx.beginPath(); ctx.rect(left, top, plotW, plotH); ctx.clip()
                        for (let item of series) {
                            ctx.strokeStyle = item.color; ctx.lineWidth = 0.9; ctx.beginPath()
                            let active = false
                            for (let p of item.points) {
                                if (!finitePoint(p)) { active = false; continue }
                                if (!active) { ctx.moveTo(mapX(p.r), mapY(p.x)); active = true }
                                else ctx.lineTo(mapX(p.r), mapY(p.x))
                            }
                            ctx.stroke()
                        }
                        ctx.restore()
                        ctx.fillStyle = "#687078"; ctx.font = "8px sans-serif"
                        ctx.fillText("RAW PHASE V/I · diagnostic only", left, bottom + 18)
                    }

                    if (root.distanceMode) {
                        const gap = 6
                        const panelH = (height - gap) * 0.5
                        drawDistancePanel(0, 0, width, panelH, true, root.earthSeries || [], root.earthZones || [])
                        drawDistancePanel(0, panelH + gap, width, panelH, false, root.phaseSeries || [], root.phaseZones || [])
                    } else {
                        drawRawPanel(root.rawSeries || [])
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.distanceMode
                    acceptedButtons: Qt.LeftButton

                    onClicked: function(mouse) {
                        const gap = 6
                        const panelH = (locusCanvas.height - gap) * 0.5
                        const earthFamily = mouse.y < panelH
                        const panelY = earthFamily ? 0 : panelH + gap
                        if (mouse.y < panelY || mouse.y > panelY + 26) return

                        const zones = earthFamily ? root.earthZones : root.phaseZones
                        const series = earthFamily ? root.earthSeries : root.phaseSeries
                        let x = 94
                        const localX = mouse.x

                        for (let z = 0; z < zones.length; ++z) {
                            if (localX >= x && localX < x + 72) {
                                root.toggleZoneVisible(earthFamily, z)
                                return
                            }
                            x += 72
                        }

                        for (let item of series) {
                            if (localX >= x && localX < x + 78) {
                                if (localX < x + 12) root.toggleLoopVisible(item.loop)
                                else {
                                    root.selectedLoop = item.loop
                                    if (!root.loopVisible(item.loop)) root.toggleLoopVisible(item.loop)
                                }
                                return
                            }
                            x += 78
                        }
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
