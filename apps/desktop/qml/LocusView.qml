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
    property real locusZoom: 1.0
    readonly property int locusZoomPercent: Math.round(locusZoom * 100.0)

    property var earthPanelTransform: ({valid:false})
    property var phasePanelTransform: ({valid:false})
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
    readonly property var allLoops: earthLoops.concat(phaseLoops)

    readonly property var cursorAValues: {
        const representationDependency = valueRepresentation
        return distanceMode ? buildCursorValues(cursorATime) : ({})
    }
    readonly property var cursorBValues: {
        const representationDependency = valueRepresentation
        return distanceMode ? buildCursorValues(cursorBTime) : ({})
    }
    readonly property var cursorAValue: cursorAValues[selectedLoop] || ({valid:false})
    readonly property var cursorBValue: cursorBValues[selectedLoop] || ({valid:false})

    // LocusAnalysisProxy batches all six trajectories on the first call and serves the remaining
    // loop requests from one cached result. LocusView itself remains compatible with AnalysisController.
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

    function finitePoint(point) { return point && point.valid !== false && Number.isFinite(point.r) && Number.isFinite(point.x) }
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
        requestStaticRepaint()
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
        requestStaticRepaint()
    }
    function resetVisibility() {
        loopVisibility = ({"L1-E":true, "L2-E":true, "L3-E":true, "L1-L2":true, "L2-L3":true, "L3-L1":true})
        earthZoneVisibility = []
        phaseZoneVisibility = []
        requestStaticRepaint()
    }
    function setLocusZoom(value) {
        const next = Math.max(0.50, Math.min(8.0, value))
        if (Math.abs(next - locusZoom) < 1.0e-9) return
        locusZoom = next
    }
    function zoomLocusIn() { setLocusZoom(locusZoom * 1.25) }
    function zoomLocusOut() { setLocusZoom(locusZoom / 1.25) }
    function fitLocus() { setLocusZoom(1.0) }
    function fullRecordStart() { return document ? document.dataStartSeconds : viewStart }
    function fullRecordDuration() { return document ? Math.max(0.0, document.dataEndSeconds - document.dataStartSeconds) : visibleDuration }

    function buildDistanceSeries(loops) {
        if (!analysis) return []
        const start = fullRecordStart()
        const duration = fullRecordDuration()
        if (duration <= 0) return []
        let result = []
        for (let loop of loops) {
            if (!analysis.distanceLoopAvailable(loop)) continue
            result.push({loop:loop, color:loopColor(loop), points:analysis.distanceLocus(loop, start, duration, 4000, kLMagnitude, kLAngle)})
        }
        return result
    }
    function buildCursorValues(timeSeconds) { return analysis ? analysis.distanceLoopsAt(timeSeconds, kLMagnitude, kLAngle) : ({}) }

    function isOverreachZone(zone) { return zone && String(zone.type).toUpperCase().indexOf("OVERREACH") >= 0 }
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
        if (result.some(z => root.isOverreachZone(z))) {
            result.sort((a, b) => {
                const an = root.legacyZoneNumber(a), bn = root.legacyZoneNumber(b)
                if (Number.isFinite(an) && Number.isFinite(bn) && an !== bn) return an - bn
                if (Number.isFinite(an) && Number.isFinite(bn) && an === bn) {
                    const ao = root.isOverreachZone(a) ? 1 : 0, bo = root.isOverreachZone(b) ? 1 : 0
                    if (ao !== bo) return ao - bo
                }
                const ai = Number(a.index), bi = Number(b.index)
                return Number.isFinite(ai) && Number.isFinite(bi) ? ai - bi : 0
            })
        }
        return result
    }

    function buildRawSeries() {
        if (!analysis || !document) return []
        let result = []
        const phases = ["L1", "L2", "L3"]
        const start = fullRecordStart(), duration = fullRecordDuration(), steps = 240
        for (let phase of phases) {
            const voltage = analysis.phaseChannel("Voltage", phase)
            const current = analysis.phaseChannel("Current", phase)
            if (voltage < 0 || current < 0) continue
            let points = []
            for (let n = 0; n < steps; ++n) {
                const time = start + duration * n / Math.max(1, steps - 1)
                const z = analysis.impedanceAt(voltage, current, time)
                points.push(z.valid && Number.isFinite(z.r) && Number.isFinite(z.x) ? {valid:true, r:z.r, x:z.x} : {valid:false})
            }
            result.push({phase:phase, color:analysis.phaseColorForName(phase), points:points})
        }
        return result
    }

    function percentile(values, q) {
        if (!values || values.length === 0) return 0.0
        values.sort((a, b) => a - b)
        if (values.length === 1) return values[0]
        const pos = (values.length - 1) * q, lo = Math.floor(pos), hi = Math.ceil(pos)
        if (lo === hi) return values[lo]
        const f = pos - lo
        return values[lo] * (1.0 - f) + values[hi] * f
    }
    function niceCeil(value) {
        if (!Number.isFinite(value) || value <= 0) return 1.0
        const exponent = Math.floor(Math.log(value) / Math.LN10), base = Math.pow(10, exponent), normalized = value / base
        const choices = [1, 1.5, 2, 2.5, 3, 4, 5, 7.5, 10]
        for (let c of choices) if (normalized <= c + 1e-12) return c * base
        return 10 * base
    }
    function niceStep(value) {
        if (!Number.isFinite(value) || value <= 0) return 1.0
        const exponent = Math.floor(Math.log(value) / Math.LN10), base = Math.pow(10, exponent), normalized = value / base
        if (normalized <= 1) return base
        if (normalized <= 2) return 2 * base
        if (normalized <= 2.5) return 2.5 * base
        if (normalized <= 5) return 5 * base
        return 10 * base
    }
    function zoneMagnitude(zones) {
        let r = 0.0, x = 0.0
        for (let zone of zones) {
            if (zone.kind === "circle") {
                r = Math.max(r, Math.abs(zone.centerR) + Math.abs(zone.radius))
                x = Math.max(x, Math.abs(zone.centerX) + Math.abs(zone.radius))
            } else if (zone.kind === "polygon") {
                for (let p of zone.points) {
                    if (!Number.isFinite(p.r) || !Number.isFinite(p.x)) continue
                    r = Math.max(r, Math.abs(p.r)); x = Math.max(x, Math.abs(p.x))
                }
            }
        }
        return {r:r, x:x}
    }
    function coreTarget(series, zones) {
        let rs = [], xs = []
        for (let item of series) for (let p of item.points) if (finitePoint(p)) { rs.push(Math.abs(p.r)); xs.push(Math.abs(p.x)) }
        const zone = zoneMagnitude(zones)
        return {r:niceCeil(Math.max(3.0, zone.r, percentile(rs, 0.80)) * 1.10),
                x:niceCeil(Math.max(3.0, zone.x, percentile(xs, 0.80)) * 1.10)}
    }
    function panelTransform(px, py, pw, ph, series, zones) {
        const legendH = 34, left = px + 68, right = px + pw - 16, top = py + legendH + 8, bottom = py + ph - 34
        const plotW = Math.max(20, right - left), plotH = Math.max(20, bottom - top), target = coreTarget(series, zones)
        const autoScale = Math.max(1e-9, Math.min(plotW / (2 * target.r), plotH / (2 * target.x))), scale = autoScale * locusZoom
        return {valid:true, px:px, py:py, pw:pw, ph:ph, left:left, right:right, top:top, bottom:bottom,
                plotW:plotW, plotH:plotH, scale:scale, cx:left + plotW * 0.5, cy:top + plotH * 0.5,
                rHalf:plotW / (2 * scale), xHalf:plotH / (2 * scale)}
    }
    function pointX(point, transform) { return finitePoint(point) && transform && transform.valid ? transform.cx + point.r * transform.scale : -10000 }
    function pointY(point, transform) { return finitePoint(point) && transform && transform.valid ? transform.cy - point.x * transform.scale : -10000 }
    function pointInTransform(point, transform) {
        if (!finitePoint(point) || !transform || !transform.valid) return false
        const x = pointX(point, transform), y = pointY(point, transform)
        return x >= transform.left && x <= transform.right && y >= transform.top && y <= transform.bottom
    }
    function formatOhm(value) {
        if (!Number.isFinite(value)) return "—"
        const a = Math.abs(value)
        if (a >= 100) return value.toFixed(2) + " Ω"
        if (a >= 10) return value.toFixed(3) + " Ω"
        return value.toFixed(4) + " Ω"
    }
    function compactImpedance(value) { return value && value.valid ? "R " + formatOhm(value.r) + "  X " + formatOhm(value.x) : "—" }
    function ensureAvailableLoop() {
        if (!analysis || !distanceMode) return
        if (analysis.distanceLoopAvailable(selectedLoop)) return
        for (let loop of allLoops) if (analysis.distanceLoopAvailable(loop)) { selectedLoop = loop; return }
    }
    function requestStaticRepaint() { if (locusCanvas) locusCanvas.requestPaint() }

    onEarthSeriesChanged: requestStaticRepaint()
    onPhaseSeriesChanged: requestStaticRepaint()
    onEarthZonesChanged: requestStaticRepaint()
    onPhaseZonesChanged: requestStaticRepaint()
    onRawSeriesChanged: requestStaticRepaint()
    onSelectedLoopChanged: requestStaticRepaint()
    onLoopVisibilityChanged: requestStaticRepaint()
    onEarthZoneVisibilityChanged: requestStaticRepaint()
    onPhaseZoneVisibilityChanged: requestStaticRepaint()
    onLocusZoomChanged: requestStaticRepaint()
    onAnalysisModeChanged: { ensureAvailableLoop(); requestStaticRepaint() }
    onAnalysisChanged: { ensureAvailableLoop(); requestStaticRepaint() }
    onVisibleChanged: if (visible) Qt.callLater(requestStaticRepaint)
    onWidthChanged: requestStaticRepaint()
    onHeightChanged: requestStaticRepaint()
    Component.onCompleted: ensureAvailableLoop()

    Connections {
        target: root.document
        function onDocumentChanged() { root.fitLocus(); root.ensureAvailableLoop(); root.resetVisibility() }
        function onRepresentationChanged() { root.requestStaticRepaint() }
    }
    Connections {
        target: root.zoneController
        function onModelChanged() { root.earthZoneVisibility = []; root.phaseZoneVisibility = []; root.fitLocus(); root.requestStaticRepaint() }
        function onGroundingFactorChanged() { root.requestStaticRepaint() }
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
            Layout.preferredHeight: 42
            color: "#e7eaed"
            border.color: "#bec5ca"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 7
                Label { text: "DISTANCE / R-X"; color: "#29333a"; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
                Rectangle { width:1; height:24; color:"#c0c6ca"; Layout.leftMargin:3; Layout.rightMargin:2 }
                ToolButton { text:"Protection"; checkable:true; checked:root.distanceMode; font.pixelSize:10; onClicked:root.analysisMode="distance" }
                ToolButton { text:"Raw V/I"; checkable:true; checked:!root.distanceMode; font.pixelSize:10; onClicked:root.analysisMode="raw" }
                Label { visible:root.distanceMode; text:"Inspect " + root.selectedLoop; color:root.selectedLoopColor; font.pixelSize:10; font.weight:Font.DemiBold; Layout.leftMargin:4 }
                Button { visible:root.distanceMode; text:root.zoneController && root.zoneController.hasZones ? "Zones ✓" : "Load RIO/XRIO"; font.pixelSize:10; onClicked:zoneDialog.open() }
                Button { visible:root.distanceMode && root.zoneController && root.zoneController.hasZones; text:"Clear"; font.pixelSize:10; onClicked:root.zoneController.clearZones() }
                Button { visible:root.distanceMode; text:"Show all"; font.pixelSize:10; onClicked:root.resetVisibility() }
                Rectangle { visible:root.distanceMode; width:1; height:24; color:"#c0c6ca"; Layout.leftMargin:2; Layout.rightMargin:2 }
                ToolButton { visible:root.distanceMode; text:"−"; font.pixelSize:13; Layout.preferredWidth:30; onClicked:root.zoomLocusOut(); ToolTip.visible:hovered; ToolTip.text:"Zoom out R-X plane" }
                ToolButton { visible:root.distanceMode; text:"Fit"; font.pixelSize:10; Layout.preferredWidth:42; onClicked:root.fitLocus() }
                ToolButton { visible:root.distanceMode; text:"+"; font.pixelSize:13; Layout.preferredWidth:30; onClicked:root.zoomLocusIn(); ToolTip.visible:hovered; ToolTip.text:"Zoom in R-X plane" }
                Label { visible:root.distanceMode; text:root.locusZoomPercent + "%"; color:"#53616b"; font.pixelSize:9; font.weight:Font.DemiBold; Layout.preferredWidth:38; horizontalAlignment:Text.AlignRight }
                Item { Layout.fillWidth:true }
                Label { text:(root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY") + " Ω"; color:"#4d5c66"; font.pixelSize:10; font.weight:Font.DemiBold }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.distanceMode ? 40 : 32
            color: root.distanceMode ? "#f8f9fa" : "#fff8e8"
            border.color: root.distanceMode ? "#d0d5d9" : "#e1c98d"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 7
                Label { visible:root.distanceMode; text:"kL"; color:"#56626b"; font.pixelSize:10; font.weight:Font.DemiBold }
                TextField {
                    visible: root.distanceMode
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 28
                    text: root.kLMagnitude.toFixed(4)
                    horizontalAlignment: TextInput.AlignRight
                    font.pixelSize: 10
                    validator: DoubleValidator {
                        bottom: 0.0
                        notation: DoubleValidator.StandardNotation
                    }
                    onEditingFinished: if (root.zoneController) root.zoneController.groundingFactorMagnitude = Number(text)
                }
                Label { visible:root.distanceMode; text:"∠"; color:"#56626b"; font.pixelSize:10 }
                TextField {
                    visible: root.distanceMode
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 28
                    text: root.kLAngle.toFixed(2)
                    horizontalAlignment: TextInput.AlignRight
                    font.pixelSize: 10
                    validator: DoubleValidator {
                        bottom: -360
                        top: 360
                        notation: DoubleValidator.StandardNotation
                    }
                    onEditingFinished: if (root.zoneController) root.zoneController.groundingFactorAngle = Number(text)
                }
                Label { visible:root.distanceMode; text:"° · " + (root.zoneController ? root.zoneController.groundingFactorSource : "manual"); color:"#657078"; font.pixelSize:9 }
                Label { visible:root.distanceMode && (!root.zoneController || !root.zoneController.groundingFactorValid || Math.abs(root.kLMagnitude)<1e-12); text:"UNCOMPENSATED EARTH LOOPS"; color:"#9a5c00"; font.pixelSize:9; font.weight:Font.DemiBold }
                Rectangle { visible:root.distanceMode; width:1; height:22; color:"#d0d5d9"; Layout.leftMargin:3; Layout.rightMargin:3 }
                Label { visible:root.distanceMode; text:"C1"; color:"#244f9e"; font.pixelSize:10; font.weight:Font.Bold }
                Label {
                    visible: root.distanceMode
                    text: (root.document
                           ? ((root.cursorATime - root.document.triggerOffsetSeconds) * 1000).toFixed(2) + " ms · "
                           : "") + root.compactImpedance(root.cursorAValue)
                    color: "#354049"
                    font.pixelSize: 10
                }
                Rectangle { visible:root.distanceMode; width:1; height:22; color:"#d0d5d9" }
                Label { visible:root.distanceMode; text:"C2"; color:"#b77900"; font.pixelSize:10; font.weight:Font.Bold }
                Label {
                    visible: root.distanceMode
                    text: (root.document
                           ? ((root.cursorBTime - root.document.triggerOffsetSeconds) * 1000).toFixed(2) + " ms · "
                           : "") + root.compactImpedance(root.cursorBValue)
                    color: "#354049"
                    font.pixelSize: 10
                }
                Item { Layout.fillWidth:true }
                Label { visible:root.distanceMode; text:root.analysis ? "I floor " + (root.analysis.distanceCurrentFloor()*1000).toFixed(2) + " mA" : ""; color:"#6f7980"; font.pixelSize:9 }
                Label { visible:!root.distanceMode; text:"RAW PHASE V/I · diagnostic only"; color:"#785b1a"; font.pixelSize:10; font.weight:Font.DemiBold }
            }
        }

        Rectangle {
            Layout.fillWidth:true
            Layout.preferredHeight:root.distanceMode && root.zoneController && root.zoneController.compatibilityWarning.length ? 28 : 0
            visible:height>0
            color:"#fff8e8"
            border.color:"#e1c98d"
            Label { anchors.fill:parent; anchors.leftMargin:10; anchors.rightMargin:10; verticalAlignment:Text.AlignVCenter; text:root.zoneController ? root.zoneController.compatibilityWarning : ""; color:"#785b1a"; font.pixelSize:9; elide:Text.ElideRight }
        }

        Rectangle {
            id: graphArea
            Layout.fillWidth:true
            Layout.fillHeight:true
            color:"#f2f3f4"
            clip:true

            Canvas {
                id:locusCanvas
                anchors.fill:parent
                anchors.margins:6
                antialiasing:true
                renderStrategy: Canvas.Cooperative
                onWidthChanged:root.requestStaticRepaint()
                onHeightChanged:root.requestStaticRepaint()

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0,0,width,height)
                    if (!root.analysis || width < 220 || height < 160) { root.earthPanelTransform=({valid:false}); root.phasePanelTransform=({valid:false}); return }

                    function earthZoneColor(index){ return index%2===0 ? "#008a2f" : "#075ec7" }
                    function phaseZoneColor(index){ return index%2===0 ? "#c7a000" : "#e300cb" }
                    function zoneLegendLabel(zone, earthFamily, ordinal, zones){
                        const legacyOrdered=zones && zones.some(z=>root.isOverreachZone(z)); let index=legacyOrdered?ordinal+1:Number(zone.index)
                        if(!Number.isFinite(index)||index<=0)index=ordinal+1
                        return "Zone_"+index+(earthFamily?"E":"")
                    }
                    function insidePlot(px,py,t){ return px>=t.left&&px<=t.right&&py>=t.top&&py<=t.bottom }
                    function drawCheckbox(x,y,checked,color){
                        ctx.save(); ctx.fillStyle="#fff"; ctx.strokeStyle=checked?color:"#9ca3a8"; ctx.lineWidth=checked?1.2:0.9
                        ctx.fillRect(x,y,10,10); ctx.strokeRect(x+.5,y+.5,9,9)
                        if(checked){ctx.strokeStyle=color;ctx.lineWidth=1.3;ctx.beginPath();ctx.moveTo(x+2,y+5);ctx.lineTo(x+4.2,y+7.2);ctx.lineTo(x+8,y+2.7);ctx.stroke()} ctx.restore()
                    }
                    function drawZone(zone,mapX,mapY,color){
                        ctx.save();ctx.strokeStyle=color;ctx.lineWidth=1.05;ctx.setLineDash([4,3])
                        if(zone.kind==="circle"&&Number.isFinite(zone.radius)&&zone.radius>0){const segments=72;ctx.beginPath();for(let n=0;n<=segments;++n){const a=Math.PI*2*n/segments,px=mapX(zone.centerR+zone.radius*Math.cos(a)),py=mapY(zone.centerX+zone.radius*Math.sin(a));if(n===0)ctx.moveTo(px,py);else ctx.lineTo(px,py)}ctx.stroke()}
                        else if(zone.kind==="polygon"&&zone.points.length>=3){ctx.beginPath();ctx.moveTo(mapX(zone.points[0].r),mapY(zone.points[0].x));for(let n=1;n<zone.points.length;++n)ctx.lineTo(mapX(zone.points[n].r),mapY(zone.points[n].x));ctx.closePath();ctx.stroke()}ctx.restore()
                    }
                    function drawSampleSquare(px,py,color,selected){const half=selected?2.2:1.9;ctx.save();ctx.setLineDash([]);ctx.fillStyle="#fff";ctx.fillRect(px-half,py-half,half*2,half*2);ctx.strokeStyle=color;ctx.lineWidth=selected?1.05:.85;ctx.strokeRect(px-half,py-half,half*2,half*2);ctx.restore()}
                    function drawSeries(item,t){
                        if(!root.loopVisible(item.loop))return;const selected=root.selectedLoop===item.loop,mapX=r=>t.cx+r*t.scale,mapY=x=>t.cy-x*t.scale
                        ctx.save();ctx.setLineDash([]);ctx.strokeStyle=item.color;ctx.globalAlpha=selected?1:.9;ctx.lineWidth=selected?1.4:1.05;ctx.beginPath();let active=false
                        for(let p of item.points){if(!root.finitePoint(p)){active=false;continue}const px=mapX(p.r),py=mapY(p.x);if(!active){ctx.moveTo(px,py);active=true}else ctx.lineTo(px,py)}ctx.stroke();ctx.restore()
                        const markerGapPx=7;let lx=NaN,ly=NaN,started=false
                        for(let p of item.points){if(!root.finitePoint(p)){started=false;lx=NaN;ly=NaN;continue}const px=mapX(p.r),py=mapY(p.x);if(!insidePlot(px,py,t))continue;if(!started||!Number.isFinite(lx)||Math.hypot(px-lx,py-ly)>=markerGapPx){drawSampleSquare(px,py,item.color,selected);lx=px;ly=py;started=true}}
                    }
                    function drawLegend(t,earthFamily,series,zones){
                        const title=earthFamily?"EARTH LOOPS":"PHASE-PHASE LOOPS";ctx.font="600 10px sans-serif";ctx.fillStyle="#4f5b64";ctx.textAlign="left";ctx.textBaseline="middle";ctx.fillText(title,t.px+10,t.py+17)
                        let x=t.px+112;const y=t.py+17;ctx.font="9px sans-serif"
                        for(let z=0;z<zones.length;++z){const color=earthFamily?earthZoneColor(z):phaseZoneColor(z),checked=root.zoneVisible(earthFamily,z);drawCheckbox(x,y-5,checked,color);ctx.save();ctx.strokeStyle=color;ctx.globalAlpha=checked?1:.35;ctx.lineWidth=1;ctx.setLineDash([4,3]);ctx.beginPath();ctx.moveTo(x+15,y);ctx.lineTo(x+27,y);ctx.stroke();ctx.restore();ctx.fillStyle=checked?"#303940":"#9aa1a6";ctx.fillText(zoneLegendLabel(zones[z],earthFamily,z,zones),x+31,y);x+=84}
                        for(let item of series){const checked=root.loopVisible(item.loop);drawCheckbox(x,y-5,checked,item.color);ctx.save();ctx.strokeStyle=item.color;ctx.globalAlpha=checked?1:.35;ctx.lineWidth=root.selectedLoop===item.loop?1.4:1;ctx.setLineDash([]);ctx.beginPath();ctx.moveTo(x+15,y);ctx.lineTo(x+30,y);ctx.stroke();ctx.restore();drawSampleSquare(x+22,y,item.color,root.selectedLoop===item.loop);ctx.fillStyle=checked?(root.selectedLoop===item.loop?"#11161a":"#303940"):"#9aa1a6";ctx.font=root.selectedLoop===item.loop?"600 9px sans-serif":"9px sans-serif";ctx.fillText(root.signalLegendName(item.loop),x+34,y);x+=94;if(x>t.px+t.pw-82)break}ctx.textBaseline="alphabetic"
                    }
                    function drawDistancePanel(t,earthFamily,series,zones){
                        const mapX=r=>t.cx+r*t.scale,mapY=x=>t.cy-x*t.scale;ctx.fillStyle="#fff";ctx.fillRect(t.px,t.py,t.pw,t.ph);ctx.strokeStyle="#c3c9cd";ctx.lineWidth=1;ctx.strokeRect(t.px+.5,t.py+.5,t.pw-1,t.ph-1);drawLegend(t,earthFamily,series,zones);ctx.strokeStyle="#e9edef";ctx.beginPath();ctx.moveTo(t.px,t.py+34);ctx.lineTo(t.px+t.pw,t.py+34);ctx.stroke();ctx.fillStyle="#fff";ctx.fillRect(t.left,t.top,t.plotW,t.plotH)
                        const rStep=root.niceStep(t.rHalf/4),xStep=root.niceStep(t.xHalf/3.5);ctx.strokeStyle="#eef1f3";ctx.lineWidth=.7;ctx.setLineDash([])
                        for(let rv=Math.ceil(-t.rHalf/rStep)*rStep;rv<=t.rHalf+1e-9;rv+=rStep){if(Math.abs(rv)<rStep*.1)continue;const gx=mapX(rv);ctx.beginPath();ctx.moveTo(gx,t.top);ctx.lineTo(gx,t.bottom);ctx.stroke()}
                        for(let xv=Math.ceil(-t.xHalf/xStep)*xStep;xv<=t.xHalf+1e-9;xv+=xStep){if(Math.abs(xv)<xStep*.1)continue;const gy=mapY(xv);ctx.beginPath();ctx.moveTo(t.left,gy);ctx.lineTo(t.right,gy);ctx.stroke()}
                        ctx.strokeStyle="#20252a";ctx.lineWidth=1.05;ctx.beginPath();ctx.moveTo(t.left,t.cy);ctx.lineTo(t.right,t.cy);ctx.stroke();ctx.beginPath();ctx.moveTo(t.cx,t.top);ctx.lineTo(t.cx,t.bottom);ctx.stroke()
                        ctx.fillStyle="#3f4a52";ctx.font="9px sans-serif";ctx.textBaseline="top";ctx.textAlign="center"
                        for(let rv=Math.ceil(-t.rHalf/rStep)*rStep;rv<=t.rHalf+1e-9;rv+=rStep){if(Math.abs(rv)<rStep*.1)continue;const tx=mapX(rv);ctx.fillText(rv.toFixed(Math.abs(rv)<10?1:0),tx,t.bottom+5)}ctx.fillText("0",t.cx,t.bottom+5)
                        ctx.textAlign="right";ctx.textBaseline="middle";for(let xv=Math.ceil(-t.xHalf/xStep)*xStep;xv<=t.xHalf+1e-9;xv+=xStep){if(Math.abs(xv)<xStep*.1)continue;ctx.fillText(xv.toFixed(Math.abs(xv)<10?1:0),t.left-7,mapY(xv))}ctx.fillText("0",t.left-7,t.cy)
                        ctx.textAlign="center";ctx.textBaseline="alphabetic";ctx.font="10px sans-serif";ctx.fillText("R / Ω  ("+(root.valueRepresentation==="primary"?"primary":"secondary")+")",t.cx,t.py+t.ph-8)
                        ctx.save();ctx.beginPath();ctx.rect(t.left,t.top,t.plotW,t.plotH);ctx.clip();for(let z=0;z<zones.length;++z)if(root.zoneVisible(earthFamily,z))drawZone(zones[z],mapX,mapY,earthFamily?earthZoneColor(z):phaseZoneColor(z));for(let item of series)drawSeries(item,t);ctx.restore()
                        ctx.save();ctx.translate(t.px+14,t.cy);ctx.rotate(-Math.PI/2);ctx.fillStyle="#3f4a52";ctx.font="10px sans-serif";ctx.textAlign="center";ctx.fillText("X / Ω  ("+(root.valueRepresentation==="primary"?"primary":"secondary")+")",0,0);ctx.restore()
                    }
                    function drawRawPanel(series){
                        root.earthPanelTransform=({valid:false});root.phasePanelTransform=({valid:false});ctx.fillStyle="#fff";ctx.fillRect(0,0,width,height);let rs=[],xs=[];for(let item of series)for(let p of item.points)if(root.finitePoint(p)){rs.push(Math.abs(p.r));xs.push(Math.abs(p.x))}const rTarget=root.niceCeil(Math.max(1,root.percentile(rs,.9))*1.1),xTarget=root.niceCeil(Math.max(1,root.percentile(xs,.9))*1.1),left=68,right=width-16,top=22,bottom=height-34,plotW=right-left,plotH=bottom-top,scale=Math.max(1e-9,Math.min(plotW/(2*rTarget),plotH/(2*xTarget))),cx=left+plotW*.5,cy=top+plotH*.5,mapX=r=>cx+r*scale,mapY=x=>cy-x*scale;ctx.strokeStyle="#20252a";ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(left,cy);ctx.lineTo(right,cy);ctx.stroke();ctx.beginPath();ctx.moveTo(cx,top);ctx.lineTo(cx,bottom);ctx.stroke();ctx.save();ctx.beginPath();ctx.rect(left,top,plotW,plotH);ctx.clip();for(let item of series){ctx.strokeStyle=item.color;ctx.lineWidth=1.1;ctx.beginPath();let active=false;for(let p of item.points){if(!root.finitePoint(p)){active=false;continue}if(!active){ctx.moveTo(mapX(p.r),mapY(p.x));active=true}else ctx.lineTo(mapX(p.r),mapY(p.x))}ctx.stroke()}ctx.restore();ctx.fillStyle="#59656d";ctx.font="10px sans-serif";ctx.fillText("RAW PHASE V/I · diagnostic only",left,bottom+22)
                    }

                    if(root.distanceMode){const gap=8,panelH=(height-gap)*.5,earthT=root.panelTransform(0,0,width,panelH,root.earthSeries||[],root.earthZones||[]),phaseT=root.panelTransform(0,panelH+gap,width,panelH,root.phaseSeries||[],root.phaseZones||[]);root.earthPanelTransform=earthT;root.phasePanelTransform=phaseT;drawDistancePanel(earthT,true,root.earthSeries||[],root.earthZones||[]);drawDistancePanel(phaseT,false,root.phaseSeries||[],root.phaseZones||[])}else drawRawPanel(root.rawSeries||[])
                }

                MouseArea {
                    anchors.fill:parent
                    enabled:root.distanceMode
                    acceptedButtons:Qt.LeftButton
                    onWheel:function(wheel){if(wheel.angleDelta.y===0)return;root.setLocusZoom(root.locusZoom*(wheel.angleDelta.y>0?1.15:1/1.15));wheel.accepted=true}
                    onClicked:function(mouse){
                        const gap=8,panelH=(locusCanvas.height-gap)*.5,earthFamily=mouse.y<panelH,panelY=earthFamily?0:panelH+gap
                        if(mouse.y<panelY||mouse.y>panelY+34)return
                        const zones=earthFamily?root.earthZones:root.phaseZones,series=earthFamily?root.earthSeries:root.phaseSeries;let x=112,localX=mouse.x
                        for(let z=0;z<zones.length;++z){if(localX>=x&&localX<x+84){root.toggleZoneVisible(earthFamily,z);return}x+=84}
                        for(let item of series){if(localX>=x&&localX<x+94){if(localX<x+14)root.toggleLoopVisible(item.loop);else{root.selectedLoop=item.loop;if(!root.loopVisible(item.loop))root.toggleLoopVisible(item.loop)}return}x+=94}
                    }
                }
            }

            Item {
                id:cursorOverlay
                anchors.fill:locusCanvas
                z:2
                visible:root.distanceMode
                enabled:false
                Repeater {
                    model:root.allLoops
                    delegate:Item {
                        required property int index
                        required property string modelData
                        anchors.fill:parent
                        readonly property string loopId:modelData
                        readonly property bool earthFamily:index<3
                        readonly property var panelTransform:earthFamily?root.earthPanelTransform:root.phasePanelTransform
                        readonly property var pointA:root.cursorAValues[loopId]||({valid:false})
                        readonly property var pointB:root.cursorBValues[loopId]||({valid:false})
                        readonly property bool selected:root.selectedLoop===loopId
                        visible:root.loopVisible(loopId)&&panelTransform&&panelTransform.valid

                        component CursorCross:Item {
                            property var point
                            property var plotTransform
                            property color crossColor
                            property bool strong:false
                            readonly property real arm:strong?10:8
                            readonly property real stroke:strong?2.3:1.8
                            visible:root.pointInTransform(point,plotTransform)
                            width:arm*2;height:arm*2;x:root.pointX(point,plotTransform)-arm;y:root.pointY(point,plotTransform)-arm
                            Rectangle{anchors.centerIn:parent;width:parent.width;height:parent.stroke+2.8;radius:height/2;color:"#fff"}
                            Rectangle{anchors.centerIn:parent;width:parent.stroke+2.8;height:parent.height;radius:width/2;color:"#fff"}
                            Rectangle{anchors.centerIn:parent;width:parent.width;height:parent.stroke;radius:height/2;color:parent.crossColor}
                            Rectangle{anchors.centerIn:parent;width:parent.stroke;height:parent.height;radius:width/2;color:parent.crossColor}
                        }
                        CursorCross{point:parent.pointA;plotTransform:parent.panelTransform;crossColor:"#244f9e";strong:parent.selected}
                        CursorCross{point:parent.pointB;plotTransform:parent.panelTransform;crossColor:"#b77900";strong:parent.selected}
                    }
                }
            }

            Label {
                anchors.centerIn:parent
                visible:root.distanceMode && root.analysis && root.earthSeries.length===0 && root.phaseSeries.length===0
                text:"Protection loops cannot be formed from the configured COMTRADE voltage/current channels"
                color:"#8a5f2a"
                font.pixelSize:12
            }
        }
    }
}
