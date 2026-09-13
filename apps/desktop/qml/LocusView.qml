// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Ardirec.Render 1.0

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
    property var loopVisibility: ({
        "L1-E": true, "L2-E": true, "L3-E": true,
        "L1-L2": true, "L2-L3": true, "L3-L1": true
    })

    readonly property bool distanceMode: analysisMode === "distance"
    readonly property real kLMagnitude: zoneController ? zoneController.groundingFactorMagnitude : 0.0
    readonly property real kLAngle: zoneController ? zoneController.groundingFactorAngle : 0.0
    readonly property var earthLoops: ["L1-E", "L2-E", "L3-E"]
    readonly property var phaseLoops: ["L1-L2", "L2-L3", "L3-L1"]
    readonly property var allLoops: earthLoops.concat(phaseLoops)
    readonly property int locusRevision: locusSnapshotController.revision

    readonly property var snapshotA: cursorSnapshotController.cursorA
    readonly property var snapshotB: cursorSnapshotController.cursorB
    readonly property var displaySnapshotA: snapshotMatches(snapshotA, cursorATime) ? snapshotA : ({valid:false})
    readonly property var displaySnapshotB: snapshotMatches(snapshotB, cursorBTime) ? snapshotB : ({valid:false})
    readonly property var cursorAValues: distanceMode && displaySnapshotA.valid
                                         ? cursorSnapshotController.distanceLoopsForSnapshot(displaySnapshotA, kLMagnitude, kLAngle)
                                         : ({})
    readonly property var cursorBValues: distanceMode && displaySnapshotB.valid
                                         ? cursorSnapshotController.distanceLoopsForSnapshot(displaySnapshotB, kLMagnitude, kLAngle)
                                         : ({})
    readonly property var cursorAValue: cursorAValues[selectedLoop] || ({valid:false})
    readonly property var cursorBValue: cursorBValues[selectedLoop] || ({valid:false})

    readonly property var earthZones: distanceMode ? zonesForFamily(earthLoops) : []
    readonly property var phaseZones: distanceMode ? zonesForFamily(phaseLoops) : []

    function snapshotMatches(snapshot, timeSeconds) {
        return snapshot && snapshot.valid && Number.isFinite(snapshot.time)
               && Math.abs(Number(snapshot.time) - Number(timeSeconds)) <= 1.0e-10
    }

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
        let next = ({})
        const current = loopVisibility || ({})
        for (let key in current) next[key] = current[key]
        next[loop] = !loopVisible(loop)
        loopVisibility = next
    }

    function loopMask() {
        let mask = 0
        for (let index = 0; index < allLoops.length; ++index)
            if (loopVisible(allLoops[index])) mask |= (1 << index)
        return mask
    }

    function fullRecordStart() { return document ? document.dataStartSeconds : viewStart }
    function fullRecordDuration() {
        return document ? Math.max(0.0, document.dataEndSeconds - document.dataStartSeconds) : visibleDuration
    }

    function pointBudget() {
        return Math.max(256, Math.min(4096, Math.round(Math.max(640, graphSurface.width) * 1.5)))
    }

    function queueTrajectoryRequest() {
        if (!visible || !document || fullRecordDuration() <= 0) return
        if (!trajectoryRequest.running) trajectoryRequest.start()
        else trajectoryRequest.restart()
    }

    function requestTrajectoryNow() {
        if (!document || fullRecordDuration() <= 0) return
        locusSnapshotController.request(fullRecordStart(), fullRecordDuration(), pointBudget(),
                                        Number(kLMagnitude), Number(kLAngle))
    }

    function requestCursorSnapshots() {
        if (!visible || !document) return
        cursorSnapshotController.requestCursorA(cursorATime)
        cursorSnapshotController.requestCursorB(cursorBTime)
    }

    Timer {
        id: trajectoryRequest
        interval: 35
        repeat: false
        onTriggered: root.requestTrajectoryNow()
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

    function zoneMagnitude(zones) {
        let r = 0.0, x = 0.0
        for (let zone of zones) {
            if (zone.kind === "circle") {
                r = Math.max(r, Math.abs(zone.centerR) + Math.abs(zone.radius))
                x = Math.max(x, Math.abs(zone.centerX) + Math.abs(zone.radius))
            } else if (zone.kind === "polygon" && zone.points) {
                for (let point of zone.points) {
                    if (!Number.isFinite(point.r) || !Number.isFinite(point.x)) continue
                    r = Math.max(r, Math.abs(point.r))
                    x = Math.max(x, Math.abs(point.x))
                }
            }
        }
        return {r:r, x:x}
    }

    function niceCeil(value) {
        if (!Number.isFinite(value) || value <= 0) return 1.0
        const exponent = Math.floor(Math.log(value) / Math.LN10)
        const base = Math.pow(10, exponent)
        const normalized = value / base
        const choices = [1, 1.5, 2, 2.5, 3, 4, 5, 7.5, 10]
        for (let choice of choices) if (normalized <= choice + 1e-12) return choice * base
        return 10 * base
    }

    function niceStep(value) {
        if (!Number.isFinite(value) || value <= 0) return 1.0
        const exponent = Math.floor(Math.log(value) / Math.LN10)
        const base = Math.pow(10, exponent)
        const normalized = value / base
        if (normalized <= 1) return base
        if (normalized <= 2) return 2 * base
        if (normalized <= 2.5) return 2.5 * base
        if (normalized <= 5) return 5 * base
        return 10 * base
    }

    // Fit All is intentionally based on 100% of finite trajectory extents, never a percentile.
    function distanceTarget(zones) {
        const zone = zoneMagnitude(zones)
        return {
            r: niceCeil(Math.max(3.0, zone.r, locusSnapshotController.maxAbsR) * 1.08),
            x: niceCeil(Math.max(3.0, zone.x, locusSnapshotController.maxAbsX) * 1.08)
        }
    }

    function rawTarget() {
        return {
            r: niceCeil(Math.max(3.0, locusSnapshotController.rawMaxAbsR) * 1.08),
            x: niceCeil(Math.max(3.0, locusSnapshotController.rawMaxAbsX) * 1.08)
        }
    }

    function panelTransform(px, py, pw, ph, target, withLegend) {
        const legendH = withLegend ? 34 : 0
        const left = px + 68
        const right = px + pw - 16
        const top = py + legendH + 8
        const bottom = py + ph - 34
        const plotW = Math.max(20, right - left)
        const plotH = Math.max(20, bottom - top)
        const autoScale = Math.max(1e-9, Math.min(plotW / (2 * target.r), plotH / (2 * target.x)))
        const scale = autoScale * locusZoom
        return {valid:true, px:px, py:py, pw:pw, ph:ph,
                left:left, right:right, top:top, bottom:bottom,
                plotW:plotW, plotH:plotH, scale:scale,
                cx:left + plotW * 0.5, cy:top + plotH * 0.5,
                rHalf:plotW / (2 * scale), xHalf:plotH / (2 * scale)}
    }

    readonly property real panelGap: 8
    readonly property real panelHeight: Math.max(1, (graphSurface.height - panelGap) * 0.5)
    readonly property var earthTransform: distanceMode
                                          ? panelTransform(0, 0, graphSurface.width, panelHeight,
                                                           distanceTarget(earthZones), true)
                                          : ({valid:false})
    readonly property var phaseTransform: distanceMode
                                          ? panelTransform(0, panelHeight + panelGap, graphSurface.width, panelHeight,
                                                           distanceTarget(phaseZones), true)
                                          : ({valid:false})
    readonly property var rawTransform: !distanceMode
                                        ? panelTransform(0, 0, graphSurface.width, graphSurface.height,
                                                         rawTarget(), false)
                                        : ({valid:false})

    function pointX(point, transform) {
        return point && point.valid && transform && transform.valid ? transform.cx + point.r * transform.scale : -10000
    }
    function pointY(point, transform) {
        return point && point.valid && transform && transform.valid ? transform.cy - point.x * transform.scale : -10000
    }
    function pointInside(point, transform) {
        if (!point || !point.valid || !transform || !transform.valid) return false
        const x = pointX(point, transform), y = pointY(point, transform)
        return x >= transform.left && x <= transform.right && y >= transform.top && y <= transform.bottom
    }

    function compactImpedance(value) {
        if (!value || !value.valid) return "—"
        function fmt(number) {
            const a = Math.abs(number)
            return (a >= 100 ? number.toFixed(2) : a >= 10 ? number.toFixed(3) : number.toFixed(4)) + " Ω"
        }
        return "R " + fmt(value.r) + "  X " + fmt(value.x)
    }

    function ensureAvailableLoop() {
        if (!analysis || !distanceMode) return
        if (analysis.distanceLoopAvailable(selectedLoop)) return
        for (let loop of allLoops) {
            if (analysis.distanceLoopAvailable(loop)) { selectedLoop = loop; return }
        }
    }

    function setLocusZoom(value) {
        locusZoom = Math.max(0.50, Math.min(8.0, value))
    }

    function requestStaticPaint() { if (staticCanvas) staticCanvas.requestPaint() }

    onCursorATimeChanged: if (visible) cursorSnapshotController.requestCursorA(cursorATime)
    onCursorBTimeChanged: if (visible) cursorSnapshotController.requestCursorB(cursorBTime)
    onValueRepresentationChanged: { queueTrajectoryRequest(); requestStaticPaint() }
    onKLMagnitudeChanged: queueTrajectoryRequest()
    onKLAngleChanged: queueTrajectoryRequest()
    onLocusRevisionChanged: requestStaticPaint()
    onEarthZonesChanged: requestStaticPaint()
    onPhaseZonesChanged: requestStaticPaint()
    onLocusZoomChanged: requestStaticPaint()
    onDistanceModeChanged: { ensureAvailableLoop(); requestStaticPaint() }
    onVisibleChanged: if (visible) { queueTrajectoryRequest(); requestCursorSnapshots(); Qt.callLater(requestStaticPaint) }
    onWidthChanged: { queueTrajectoryRequest(); requestStaticPaint() }
    onHeightChanged: { queueTrajectoryRequest(); requestStaticPaint() }
    Component.onCompleted: { ensureAvailableLoop(); if (visible) { queueTrajectoryRequest(); requestCursorSnapshots() } }

    FileDialog {
        id: zoneDialog
        title: "Open protection characteristic"
        nameFilters: ["Protection characteristics (*.rio *.RIO *.xrio *.XRIO)", "All files (*)"]
        onAccepted: if (root.zoneController) root.zoneController.openFile(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#eef1f3"
            border.color: "#c7cdd2"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 7

                Label { text: "DISTANCE / R-X"; color: "#303940"; font.pixelSize: 12; font.weight: Font.DemiBold }
                Rectangle { width:1; height:26; color:"#cbd1d6" }

                ToolButton {
                    text: "Protection"
                    checkable: true
                    checked: root.distanceMode
                    onClicked: root.analysisMode = "distance"
                }
                ToolButton {
                    text: "Raw V/I"
                    checkable: true
                    checked: !root.distanceMode
                    onClicked: root.analysisMode = "raw"
                }
                ToolButton {
                    text: "Load zones"
                    visible: root.distanceMode
                    onClicked: zoneDialog.open()
                }

                Rectangle { width:1; height:26; color:"#cbd1d6" }
                ToolButton { text:"−"; onClicked:root.setLocusZoom(root.locusZoom / 1.25); ToolTip.visible:hovered; ToolTip.text:"Zoom out" }
                ToolButton { text:"Fit"; onClicked:root.setLocusZoom(1.0) }
                ToolButton { text:"+"; onClicked:root.setLocusZoom(root.locusZoom * 1.25); ToolTip.visible:hovered; ToolTip.text:"Zoom in" }
                Label { text:Math.round(root.locusZoom*100)+"%"; color:"#59656d"; font.pixelSize:9 }

                Rectangle { visible:root.distanceMode; width:1; height:26; color:"#cbd1d6" }
                Label { visible:root.distanceMode; text:"kL"; color:"#56626b"; font.pixelSize:9; font.weight:Font.DemiBold }
                Label { visible:root.distanceMode; text:Number(root.kLMagnitude).toFixed(4)+" ∠ "+Number(root.kLAngle).toFixed(2)+"°"; color:"#48545d"; font.pixelSize:9 }

                Item { Layout.fillWidth:true }
                Label {
                    visible: locusSnapshotController.busy
                    text: "Updating trajectory…"
                    color: "#6e7880"
                    font.pixelSize: 9
                }
                Label {
                    text: root.valueRepresentation === "primary" ? "PRIMARY Ω" : "SECONDARY Ω"
                    color: "#56626b"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.distanceMode ? 39 : 0
            visible: root.distanceMode
            color: "#f8f9fa"
            border.color: "#d2d7db"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                spacing: 5

                Repeater {
                    model: root.allLoops
                    ToolButton {
                        required property int index
                        required property string modelData
                        text: root.signalLegendName(modelData)
                        checkable: true
                        checked: root.loopVisible(modelData)
                        enabled: root.analysis ? root.analysis.distanceLoopAvailable(modelData) : false
                        font.pixelSize: 9
                        onClicked: root.toggleLoopVisible(modelData)
                        background: Rectangle {
                            radius: 2
                            color: parent.checked ? "#ffffff" : "#edf0f2"
                            border.color: parent.checked ? root.loopColor(parent.modelData) : "#cdd3d7"
                        }
                    }
                }

                Rectangle { width:1; height:22; color:"#d1d6da" }
                ComboBox {
                    Layout.preferredWidth: 112
                    model: root.allLoops
                    currentIndex: Math.max(0, root.allLoops.indexOf(root.selectedLoop))
                    onActivated: index => root.selectedLoop = root.allLoops[index]
                    font.pixelSize: 9
                }
                Label { text:"C1"; color:"#244f9e"; font.pixelSize:9; font.weight:Font.Bold }
                Label { text:root.compactImpedance(root.cursorAValue); color:"#354049"; font.pixelSize:9; font.family:"Consolas" }
                Label { text:"C2"; color:"#b77900"; font.pixelSize:9; font.weight:Font.Bold }
                Label { text:root.compactImpedance(root.cursorBValue); color:"#354049"; font.pixelSize:9; font.family:"Consolas" }
                Item { Layout.fillWidth:true }
                Label {
                    text: root.analysis ? "I floor " + (root.analysis.distanceCurrentFloor()*1000).toFixed(2) + " mA" : ""
                    color: "#6f7980"
                    font.pixelSize: 8
                }
            }
        }

        Rectangle {
            id: graphArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#f2f3f4"
            clip: true

            Item {
                id: graphSurface
                anchors.fill: parent
                anchors.margins: 6

                Canvas {
                    id: staticCanvas
                    anchors.fill: parent
                    antialiasing: true
                    renderStrategy: Canvas.Cooperative
                    onWidthChanged: root.requestStaticPaint()
                    onHeightChanged: root.requestStaticPaint()

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0,0,width,height)
                        if (width < 220 || height < 160) return

                        function drawZone(zone, t, color) {
                            const mapX = r => t.cx + r*t.scale
                            const mapY = x => t.cy - x*t.scale
                            ctx.save(); ctx.strokeStyle=color; ctx.lineWidth=1.05; ctx.setLineDash([4,3])
                            if (zone.kind === "circle" && Number.isFinite(zone.radius) && zone.radius > 0) {
                                ctx.beginPath()
                                for (let n=0; n<=72; ++n) {
                                    const a=Math.PI*2*n/72
                                    const px=mapX(zone.centerR+zone.radius*Math.cos(a))
                                    const py=mapY(zone.centerX+zone.radius*Math.sin(a))
                                    if (n===0) ctx.moveTo(px,py); else ctx.lineTo(px,py)
                                }
                                ctx.stroke()
                            } else if (zone.kind === "polygon" && zone.points && zone.points.length >= 3) {
                                ctx.beginPath(); ctx.moveTo(mapX(zone.points[0].r),mapY(zone.points[0].x))
                                for (let n=1; n<zone.points.length; ++n) ctx.lineTo(mapX(zone.points[n].r),mapY(zone.points[n].x))
                                ctx.closePath(); ctx.stroke()
                            }
                            ctx.restore()
                        }

                        function drawPanel(t, title, zones, zoneBaseColor) {
                            ctx.fillStyle="#ffffff"; ctx.fillRect(t.px,t.py,t.pw,t.ph)
                            ctx.strokeStyle="#c3c9cd"; ctx.lineWidth=1; ctx.strokeRect(t.px+.5,t.py+.5,t.pw-1,t.ph-1)
                            ctx.fillStyle="#4f5b64"; ctx.font="600 10px sans-serif"; ctx.textAlign="left"; ctx.textBaseline="middle"
                            ctx.fillText(title,t.px+10,t.py+17)
                            ctx.strokeStyle="#e9edef"; ctx.beginPath(); ctx.moveTo(t.px,t.py+34); ctx.lineTo(t.px+t.pw,t.py+34); ctx.stroke()
                            ctx.fillStyle="#ffffff"; ctx.fillRect(t.left,t.top,t.plotW,t.plotH)

                            const rStep=root.niceStep(t.rHalf/4), xStep=root.niceStep(t.xHalf/3.5)
                            ctx.strokeStyle="#eef1f3"; ctx.lineWidth=.7; ctx.setLineDash([])
                            for(let rv=Math.ceil(-t.rHalf/rStep)*rStep; rv<=t.rHalf+1e-9; rv+=rStep) {
                                if(Math.abs(rv)<rStep*.1) continue
                                const gx=t.cx+rv*t.scale; ctx.beginPath(); ctx.moveTo(gx,t.top); ctx.lineTo(gx,t.bottom); ctx.stroke()
                            }
                            for(let xv=Math.ceil(-t.xHalf/xStep)*xStep; xv<=t.xHalf+1e-9; xv+=xStep) {
                                if(Math.abs(xv)<xStep*.1) continue
                                const gy=t.cy-xv*t.scale; ctx.beginPath(); ctx.moveTo(t.left,gy); ctx.lineTo(t.right,gy); ctx.stroke()
                            }
                            ctx.strokeStyle="#20252a"; ctx.lineWidth=1.05
                            ctx.beginPath(); ctx.moveTo(t.left,t.cy); ctx.lineTo(t.right,t.cy); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(t.cx,t.top); ctx.lineTo(t.cx,t.bottom); ctx.stroke()

                            ctx.save(); ctx.beginPath(); ctx.rect(t.left,t.top,t.plotW,t.plotH); ctx.clip()
                            for(let z=0; z<zones.length; ++z) drawZone(zones[z],t,z%2===0?zoneBaseColor:"#7a59a8")
                            ctx.restore()

                            ctx.fillStyle="#3f4a52"; ctx.font="9px sans-serif"; ctx.textAlign="center"; ctx.textBaseline="top"
                            for(let rv=Math.ceil(-t.rHalf/rStep)*rStep; rv<=t.rHalf+1e-9; rv+=rStep) {
                                if(Math.abs(rv)<rStep*.1) continue
                                ctx.fillText(rv.toFixed(Math.abs(rv)<10?1:0),t.cx+rv*t.scale,t.bottom+5)
                            }
                            ctx.fillText("0",t.cx,t.bottom+5)
                            ctx.textAlign="right"; ctx.textBaseline="middle"
                            for(let xv=Math.ceil(-t.xHalf/xStep)*xStep; xv<=t.xHalf+1e-9; xv+=xStep) {
                                if(Math.abs(xv)<xStep*.1) continue
                                ctx.fillText(xv.toFixed(Math.abs(xv)<10?1:0),t.left-7,t.cy-xv*t.scale)
                            }
                            ctx.fillText("0",t.left-7,t.cy)
                            ctx.textAlign="center"; ctx.textBaseline="alphabetic"; ctx.font="10px sans-serif"
                            ctx.fillText("R / Ω",t.cx,t.py+t.ph-8)
                            ctx.save(); ctx.translate(t.px+14,t.cy); ctx.rotate(-Math.PI/2); ctx.fillText("X / Ω",0,0); ctx.restore()
                        }

                        if (root.distanceMode) {
                            drawPanel(root.earthTransform,"EARTH LOOPS",root.earthZones,"#008a2f")
                            drawPanel(root.phaseTransform,"PHASE-PHASE LOOPS",root.phaseZones,"#c7a000")
                        } else {
                            const t=root.rawTransform
                            ctx.fillStyle="#ffffff"; ctx.fillRect(0,0,width,height)
                            ctx.strokeStyle="#c3c9cd"; ctx.strokeRect(.5,.5,width-1,height-1)
                            ctx.strokeStyle="#20252a"; ctx.lineWidth=1
                            ctx.beginPath(); ctx.moveTo(t.left,t.cy); ctx.lineTo(t.right,t.cy); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(t.cx,t.top); ctx.lineTo(t.cx,t.bottom); ctx.stroke()
                            ctx.fillStyle="#59656d"; ctx.font="10px sans-serif"; ctx.fillText("RAW PHASE V/I · async batched diagnostic",t.left,t.bottom+22)
                        }
                    }
                }

                LocusTrajectoryItem {
                    visible: root.distanceMode && root.earthTransform.valid
                    x: root.earthTransform.left
                    y: root.earthTransform.top
                    width: root.earthTransform.plotW
                    height: root.earthTransform.plotH
                    source: locusSnapshotController
                    firstLoop: 0
                    loopCount: 3
                    visibilityMask: root.loopMask()
                    selectedLoop: root.selectedLoop
                    rHalf: root.earthTransform.rHalf
                    xHalf: root.earthTransform.xHalf
                }

                LocusTrajectoryItem {
                    visible: root.distanceMode && root.phaseTransform.valid
                    x: root.phaseTransform.left
                    y: root.phaseTransform.top
                    width: root.phaseTransform.plotW
                    height: root.phaseTransform.plotH
                    source: locusSnapshotController
                    firstLoop: 3
                    loopCount: 3
                    visibilityMask: root.loopMask()
                    selectedLoop: root.selectedLoop
                    rHalf: root.phaseTransform.rHalf
                    xHalf: root.phaseTransform.xHalf
                }

                LocusTrajectoryItem {
                    visible: !root.distanceMode && root.rawTransform.valid
                    x: root.rawTransform.left
                    y: root.rawTransform.top
                    width: root.rawTransform.plotW
                    height: root.rawTransform.plotH
                    source: locusSnapshotController
                    rawPhase: true
                    firstLoop: 0
                    loopCount: 3
                    visibilityMask: 7
                    selectedLoop: ""
                    rHalf: root.rawTransform.rHalf
                    xHalf: root.rawTransform.xHalf
                }

                Item {
                    anchors.fill: parent
                    visible: root.distanceMode
                    enabled: false

                    Repeater {
                        model: root.allLoops
                        delegate: Item {
                            required property int index
                            required property string modelData
                            anchors.fill: parent
                            readonly property string loopId: modelData
                            readonly property bool earthFamily: index < 3
                            readonly property var plotTransform: earthFamily ? root.earthTransform : root.phaseTransform
                            readonly property var pointA: root.cursorAValues[loopId] || ({valid:false})
                            readonly property var pointB: root.cursorBValues[loopId] || ({valid:false})
                            visible: root.loopVisible(loopId) && plotTransform && plotTransform.valid

                            component CursorCross: Item {
                                property var point
                                property var transform
                                property color crossColor
                                property bool strong: false
                                readonly property real arm: strong ? 10 : 8
                                readonly property real stroke: strong ? 2.3 : 1.8
                                visible: root.pointInside(point,transform)
                                width: arm*2; height: arm*2
                                x: root.pointX(point,transform)-arm
                                y: root.pointY(point,transform)-arm
                                Rectangle { anchors.centerIn:parent; width:parent.width; height:parent.stroke+2.8; radius:height/2; color:"#fff" }
                                Rectangle { anchors.centerIn:parent; width:parent.stroke+2.8; height:parent.height; radius:width/2; color:"#fff" }
                                Rectangle { anchors.centerIn:parent; width:parent.width; height:parent.stroke; radius:height/2; color:parent.crossColor }
                                Rectangle { anchors.centerIn:parent; width:parent.stroke; height:parent.height; radius:width/2; color:parent.crossColor }
                            }

                            CursorCross { point:parent.pointA; transform:parent.plotTransform; crossColor:"#244f9e"; strong:root.selectedLoop===parent.loopId }
                            CursorCross { point:parent.pointB; transform:parent.plotTransform; crossColor:"#b77900"; strong:root.selectedLoop===parent.loopId }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    onWheel: wheel => {
                        if (wheel.angleDelta.y === 0) return
                        root.setLocusZoom(root.locusZoom * (wheel.angleDelta.y > 0 ? 1.15 : 1/1.15))
                        wheel.accepted = true
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: root.distanceMode && root.analysis
                             && !root.allLoops.some(loop => root.analysis.distanceLoopAvailable(loop))
                    text: "Protection loops cannot be formed from the configured COMTRADE voltage/current channels"
                    color: "#8a5f2a"
                    font.pixelSize: 12
                }
            }
        }
    }
}
