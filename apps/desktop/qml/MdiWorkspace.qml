// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Item {
    id: root

    property bool hasRecord: false
    property var document
    property var analysis
    property var locusAnalysis
    property var harmonicSnapshot
    property var tableSnapshot
    property real zoomFactor: 1.0
    property real panFraction: 0.0
    property real viewStart: 0.0
    property real visibleDuration: 0.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property var voltageChannels: []
    property var currentChannels: []
    property var otherChannels: []
    property var displayedDigitalChannels: []
    property string digitalDisplayMode: "active"
    property string timeDisplayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property var phasorVoltageChannels: []
    property var phasorCurrentChannels: []
    property var residualChannels: []
    property var harmonicChannels: []
    property var tableChannels: []
    property int selectedChannel: -1
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28

    property int activeWindowId: -1
    property int nextWindowId: 1
    property int nextZOrder: 1
    property var activationHistory: []

    readonly property int childCount: windowsModel.count
    readonly property string activeViewType: viewTypeForId(activeWindowId)
    readonly property string activeWindowTitle: titleForId(activeWindowId)

    signal activeWindowChanged(int windowId, string viewType)
    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)
    signal panRequested(real panFraction)
    signal zoomRequested(real factor, real anchorFraction)
    signal digitalDisplayModeRequested(string mode)
    signal signalActivated(int channelIndex)

    ListModel { id: windowsModel }

    function clamp(value, lo, hi) { return Math.max(lo, Math.min(hi, value)) }

    function findIndexById(windowId) {
        for (let i = 0; i < windowsModel.count; ++i)
            if (windowsModel.get(i).windowId === windowId) return i
        return -1
    }

    function viewTypeForId(windowId) {
        const index = findIndexById(windowId)
        return index >= 0 ? windowsModel.get(index).viewType : "time"
    }

    function titleForId(windowId) {
        const index = findIndexById(windowId)
        return index >= 0 ? windowsModel.get(index).windowTitle : ""
    }

    function baseTitle(viewType) {
        if (viewType === "phasor") return "Phasor / Vector"
        if (viewType === "locus") return "R-X Locus"
        if (viewType === "harmonics") return "Harmonics"
        if (viewType === "table") return "Engineering Table"
        return "Time Signals"
    }

    function countType(viewType) {
        let count = 0
        for (let i = 0; i < windowsModel.count; ++i)
            if (windowsModel.get(i).viewType === viewType) ++count
        return count
    }

    function firstIndexForType(viewType) {
        let minimized = -1
        for (let i = 0; i < windowsModel.count; ++i) {
            const row = windowsModel.get(i)
            if (row.viewType !== viewType) continue
            if (row.windowState !== "minimized") return i
            if (minimized < 0) minimized = i
        }
        return minimized
    }

    function touchHistory(windowId) {
        let next = []
        for (let id of activationHistory) if (id !== windowId && findIndexById(id) >= 0) next.push(id)
        next.push(windowId)
        activationHistory = next
    }

    function removeHistory(windowId) {
        let next = []
        for (let id of activationHistory) if (id !== windowId && findIndexById(id) >= 0) next.push(id)
        activationHistory = next
    }

    function chooseTopmostWindow() {
        let bestId = -1
        let bestZ = -1
        for (let i = 0; i < windowsModel.count; ++i) {
            const row = windowsModel.get(i)
            if (row.windowState === "minimized") continue
            if (row.zOrder > bestZ) { bestZ = row.zOrder; bestId = row.windowId }
        }
        if (bestId < 0 && windowsModel.count > 0) bestId = windowsModel.get(windowsModel.count - 1).windowId
        if (bestId >= 0) activateWindow(bestId)
        else {
            activeWindowId = -1
            activeWindowChanged(-1, "time")
        }
    }

    function recomputeRequestOwners() {
        const types = ["time", "phasor", "locus", "harmonics", "table"]
        for (let type of types) {
            let ownerId = -1
            const activeIndex = findIndexById(activeWindowId)
            if (activeIndex >= 0) {
                const active = windowsModel.get(activeIndex)
                if (active.viewType === type && active.windowState !== "minimized") ownerId = active.windowId
            }
            if (ownerId < 0) {
                for (let i = 0; i < windowsModel.count; ++i) {
                    const row = windowsModel.get(i)
                    if (row.viewType === type && row.windowState !== "minimized") {
                        ownerId = row.windowId
                        break
                    }
                }
            }
            for (let i = 0; i < windowsModel.count; ++i) {
                const row = windowsModel.get(i)
                if (row.viewType === type)
                    windowsModel.setProperty(i, "requestOwner", row.windowId === ownerId)
            }
        }
    }

    function defaultGeometry(sequence) {
        const offset = 28
        const w = Math.max(420, Math.min(width * 0.78, width - 24))
        const h = Math.max(300, Math.min(height * 0.76, height - 24))
        const maxX = Math.max(0, width - w - 8)
        const maxY = Math.max(0, height - h - 8)
        return {
            x: Math.min(maxX, 12 + (sequence % 7) * offset),
            y: Math.min(maxY, 12 + (sequence % 7) * offset),
            width: w,
            height: h
        }
    }

    function openView(viewType, forceNew) {
        if (!hasRecord) return -1
        if (!forceNew) {
            const existing = firstIndexForType(viewType)
            if (existing >= 0) {
                const id = windowsModel.get(existing).windowId
                if (windowsModel.get(existing).windowState === "minimized") restoreWindow(id)
                else activateWindow(id)
                return id
            }
        }

        const serial = countType(viewType) + 1
        const geometry = defaultGeometry(windowsModel.count)
        const id = nextWindowId++
        const title = baseTitle(viewType) + (serial > 1 ? " " + serial : "")
        const z = ++nextZOrder
        windowsModel.append({
            windowId: id,
            viewType: viewType,
            windowTitle: title,
            windowX: geometry.x,
            windowY: geometry.y,
            windowWidth: geometry.width,
            windowHeight: geometry.height,
            windowState: "normal",
            restoreX: geometry.x,
            restoreY: geometry.y,
            restoreWidth: geometry.width,
            restoreHeight: geometry.height,
            zOrder: z,
            requestOwner: false
        })
        activeWindowId = id
        touchHistory(id)
        recomputeRequestOwners()
        activeWindowChanged(id, viewType)
        return id
    }

    function activateWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        if (windowsModel.get(index).windowState === "minimized") {
            restoreWindow(windowId)
            return
        }
        windowsModel.setProperty(index, "zOrder", ++nextZOrder)
        activeWindowId = windowId
        touchHistory(windowId)
        recomputeRequestOwners()
        activeWindowChanged(windowId, windowsModel.get(index).viewType)
    }

    function updateGeometry(windowId, x, y, widthValue, heightValue) {
        const index = findIndexById(windowId)
        if (index < 0 || windowsModel.get(index).windowState !== "normal") return
        const w = clamp(widthValue, 360, Math.max(360, root.width))
        const h = clamp(heightValue, 240, Math.max(240, root.height))
        windowsModel.setProperty(index, "windowWidth", w)
        windowsModel.setProperty(index, "windowHeight", h)
        windowsModel.setProperty(index, "windowX", clamp(x, 0, Math.max(0, root.width - w)))
        windowsModel.setProperty(index, "windowY", clamp(y, 0, Math.max(0, root.height - h)))
    }

    function closeWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        const wasActive = activeWindowId === windowId
        windowsModel.remove(index)
        removeHistory(windowId)
        if (wasActive) chooseTopmostWindow()
        else recomputeRequestOwners()
        layoutMinimized()
    }

    function closeActiveWindow() {
        if (activeWindowId >= 0) closeWindow(activeWindowId)
    }

    function minimizeWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        const row = windowsModel.get(index)
        if (row.windowState === "minimized") return
        if (row.windowState === "normal") {
            windowsModel.setProperty(index, "restoreX", row.windowX)
            windowsModel.setProperty(index, "restoreY", row.windowY)
            windowsModel.setProperty(index, "restoreWidth", row.windowWidth)
            windowsModel.setProperty(index, "restoreHeight", row.windowHeight)
        }
        windowsModel.setProperty(index, "windowState", "minimized")
        if (activeWindowId === windowId) chooseTopmostWindow()
        recomputeRequestOwners()
        layoutMinimized()
    }

    function restoreWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        const row = windowsModel.get(index)
        windowsModel.setProperty(index, "windowState", "normal")
        windowsModel.setProperty(index, "windowX", clamp(row.restoreX, 0, Math.max(0, width - row.restoreWidth)))
        windowsModel.setProperty(index, "windowY", clamp(row.restoreY, 0, Math.max(0, height - row.restoreHeight)))
        windowsModel.setProperty(index, "windowWidth", Math.min(Math.max(360, row.restoreWidth), Math.max(360, width)))
        windowsModel.setProperty(index, "windowHeight", Math.min(Math.max(240, row.restoreHeight), Math.max(240, height)))
        activateWindow(windowId)
        layoutMinimized()
    }

    function maximizeWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        for (let i = 0; i < windowsModel.count; ++i) {
            const other = windowsModel.get(i)
            if (other.windowId !== windowId && other.windowState === "maximized") {
                windowsModel.setProperty(i, "windowState", "normal")
                windowsModel.setProperty(i, "windowX", other.restoreX)
                windowsModel.setProperty(i, "windowY", other.restoreY)
                windowsModel.setProperty(i, "windowWidth", other.restoreWidth)
                windowsModel.setProperty(i, "windowHeight", other.restoreHeight)
            }
        }
        const row = windowsModel.get(index)
        if (row.windowState === "normal") {
            windowsModel.setProperty(index, "restoreX", row.windowX)
            windowsModel.setProperty(index, "restoreY", row.windowY)
            windowsModel.setProperty(index, "restoreWidth", row.windowWidth)
            windowsModel.setProperty(index, "restoreHeight", row.windowHeight)
        }
        windowsModel.setProperty(index, "windowState", "maximized")
        windowsModel.setProperty(index, "windowX", 0)
        windowsModel.setProperty(index, "windowY", 0)
        windowsModel.setProperty(index, "windowWidth", width)
        windowsModel.setProperty(index, "windowHeight", height)
        activateWindow(windowId)
    }

    function toggleMaximize(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        if (windowsModel.get(index).windowState === "maximized") restoreWindow(windowId)
        else maximizeWindow(windowId)
    }

    function arrangeIndices() {
        let indices = []
        for (let i = 0; i < windowsModel.count; ++i)
            if (windowsModel.get(i).windowState !== "minimized") indices.push(i)
        return indices
    }

    function prepareArrangement(indices) {
        for (let index of indices) windowsModel.setProperty(index, "windowState", "normal")
    }

    function cascade() {
        const indices = arrangeIndices()
        if (!indices.length) return
        prepareArrangement(indices)
        const offset = 28
        const w = Math.max(360, Math.min(width * 0.78, width - 16))
        const h = Math.max(240, Math.min(height * 0.76, height - 16))
        for (let slot = 0; slot < indices.length; ++slot) {
            const index = indices[slot]
            const x = Math.min(Math.max(0, width - w), 8 + (slot % 8) * offset)
            const y = Math.min(Math.max(0, height - h), 8 + (slot % 8) * offset)
            windowsModel.setProperty(index, "windowX", x)
            windowsModel.setProperty(index, "windowY", y)
            windowsModel.setProperty(index, "windowWidth", w)
            windowsModel.setProperty(index, "windowHeight", h)
            windowsModel.setProperty(index, "restoreX", x)
            windowsModel.setProperty(index, "restoreY", y)
            windowsModel.setProperty(index, "restoreWidth", w)
            windowsModel.setProperty(index, "restoreHeight", h)
        }
        recomputeRequestOwners()
    }

    function tileHorizontal() {
        const indices = arrangeIndices()
        if (!indices.length) return
        prepareArrangement(indices)
        const eachHeight = height / indices.length
        for (let slot = 0; slot < indices.length; ++slot) {
            const index = indices[slot]
            const y = slot * eachHeight
            windowsModel.setProperty(index, "windowX", 0)
            windowsModel.setProperty(index, "windowY", y)
            windowsModel.setProperty(index, "windowWidth", width)
            windowsModel.setProperty(index, "windowHeight", eachHeight)
            windowsModel.setProperty(index, "restoreX", 0)
            windowsModel.setProperty(index, "restoreY", y)
            windowsModel.setProperty(index, "restoreWidth", width)
            windowsModel.setProperty(index, "restoreHeight", eachHeight)
        }
        recomputeRequestOwners()
    }

    function tileVertical() {
        const indices = arrangeIndices()
        if (!indices.length) return
        prepareArrangement(indices)
        const eachWidth = width / indices.length
        for (let slot = 0; slot < indices.length; ++slot) {
            const index = indices[slot]
            const x = slot * eachWidth
            windowsModel.setProperty(index, "windowX", x)
            windowsModel.setProperty(index, "windowY", 0)
            windowsModel.setProperty(index, "windowWidth", eachWidth)
            windowsModel.setProperty(index, "windowHeight", height)
            windowsModel.setProperty(index, "restoreX", x)
            windowsModel.setProperty(index, "restoreY", 0)
            windowsModel.setProperty(index, "restoreWidth", eachWidth)
            windowsModel.setProperty(index, "restoreHeight", height)
        }
        recomputeRequestOwners()
    }

    function activateNext() {
        if (!windowsModel.count) return
        const current = findIndexById(activeWindowId)
        for (let step = 1; step <= windowsModel.count; ++step) {
            const index = (Math.max(0, current) + step) % windowsModel.count
            if (windowsModel.get(index).windowState !== "minimized") {
                activateWindow(windowsModel.get(index).windowId)
                return
            }
        }
    }

    function activatePrevious() {
        if (!windowsModel.count) return
        if (activationHistory.length > 1) {
            const previous = activationHistory[activationHistory.length - 2]
            if (findIndexById(previous) >= 0) {
                activateWindow(previous)
                return
            }
        }
        const current = findIndexById(activeWindowId)
        for (let step = 1; step <= windowsModel.count; ++step) {
            const index = (current - step + windowsModel.count * 2) % windowsModel.count
            if (windowsModel.get(index).windowState !== "minimized") {
                activateWindow(windowsModel.get(index).windowId)
                return
            }
        }
    }

    function layoutMinimized() {
        let minimized = []
        for (let i = 0; i < windowsModel.count; ++i)
            if (windowsModel.get(i).windowState === "minimized") minimized.push(i)
        if (!minimized.length) return
        const shelfWidth = Math.max(170, Math.min(230, (width - 18) / Math.max(1, Math.min(4, minimized.length))))
        const columns = Math.max(1, Math.floor((width - 12) / (shelfWidth + 6)))
        for (let slot = 0; slot < minimized.length; ++slot) {
            const index = minimized[slot]
            const column = slot % columns
            const row = Math.floor(slot / columns)
            windowsModel.setProperty(index, "windowX", 6 + column * (shelfWidth + 6))
            windowsModel.setProperty(index, "windowY", Math.max(0, height - 34 * (row + 1)))
            windowsModel.setProperty(index, "windowWidth", shelfWidth)
            windowsModel.setProperty(index, "windowHeight", 30)
        }
    }

    function relayoutSpecialWindows() {
        for (let i = 0; i < windowsModel.count; ++i) {
            const row = windowsModel.get(i)
            if (row.windowState === "maximized") {
                windowsModel.setProperty(i, "windowX", 0)
                windowsModel.setProperty(i, "windowY", 0)
                windowsModel.setProperty(i, "windowWidth", width)
                windowsModel.setProperty(i, "windowHeight", height)
            } else if (row.windowState === "normal") {
                const w = Math.min(row.windowWidth, Math.max(360, width))
                const h = Math.min(row.windowHeight, Math.max(240, height))
                windowsModel.setProperty(i, "windowWidth", w)
                windowsModel.setProperty(i, "windowHeight", h)
                windowsModel.setProperty(i, "windowX", clamp(row.windowX, 0, Math.max(0, width - w)))
                windowsModel.setProperty(i, "windowY", clamp(row.windowY, 0, Math.max(0, height - h)))
            }
        }
        layoutMinimized()
    }

    function clearWindows() {
        windowsModel.clear()
        activeWindowId = -1
        activationHistory = []
        nextWindowId = 1
        nextZOrder = 1
        activeWindowChanged(-1, "time")
    }

    function resetForRecord() {
        clearWindows()
        if (hasRecord) openView("time", true)
    }

    onHasRecordChanged: if (!hasRecord) clearWindows()
    onWidthChanged: Qt.callLater(relayoutSpecialWindows)
    onHeightChanged: Qt.callLater(relayoutSpecialWindows)

    Label {
        anchors.centerIn: parent
        visible: root.hasRecord && windowsModel.count === 0
        text: "No analysis windows open · choose Analysis or Window > New"
        color: "#7a8389"
        font.pixelSize: 10
    }

    Repeater {
        model: windowsModel

        delegate: MdiChildWindow {
            id: childWindow
            required property int windowId
            required property string viewType
            required property string windowTitle
            required property real windowX
            required property real windowY
            required property real windowWidth
            required property real windowHeight
            required property string windowState
            required property int zOrder
            required property bool requestOwner

            x: windowX
            y: windowY
            width: windowWidth
            height: windowHeight
            z: zOrder
            workspaceWidth: root.width
            workspaceHeight: root.height
            activeWindow: root.activeWindowId === windowId

            onActivateRequested: root.activateWindow(windowId)
            onCloseRequested: root.closeWindow(windowId)
            onMinimizeRequested: root.minimizeWindow(windowId)
            onRestoreRequested: root.restoreWindow(windowId)
            onToggleMaximizeRequested: root.toggleMaximize(windowId)
            onGeometryRequested: (gx, gy, gw, gh) => root.updateGeometry(windowId, gx, gy, gw, gh)

            AnalysisViewHost {
                anchors.fill: parent
                hasRecord: root.hasRecord
                live: childWindow.windowState !== "minimized"
                requestOwner: childWindow.requestOwner
                viewType: childWindow.viewType
                document: root.document
                analysis: root.analysis
                locusAnalysis: root.locusAnalysis
                harmonicSnapshot: root.harmonicSnapshot
                tableSnapshot: root.tableSnapshot
                zoomFactor: root.zoomFactor
                panFraction: root.panFraction
                viewStart: root.viewStart
                visibleDuration: root.visibleDuration
                cursorATime: root.cursorATime
                cursorBTime: root.cursorBTime
                voltageChannels: root.voltageChannels
                currentChannels: root.currentChannels
                otherChannels: root.otherChannels
                displayedDigitalChannels: root.displayedDigitalChannels
                digitalDisplayMode: root.digitalDisplayMode
                timeDisplayMode: root.timeDisplayMode
                valueRepresentation: root.valueRepresentation
                phasorVoltageChannels: root.phasorVoltageChannels
                phasorCurrentChannels: root.phasorCurrentChannels
                residualChannels: root.residualChannels
                harmonicChannels: root.harmonicChannels
                tableChannels: root.tableChannels
                selectedChannel: root.selectedChannel
                axisWidth: root.axisWidth
                analogTrackHeight: root.analogTrackHeight
                digitalTrackHeight: root.digitalTrackHeight
                onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)
                onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)
                onPanRequested: value => root.panRequested(value)
                onZoomRequested: (factor, anchorFraction) => root.zoomRequested(factor, anchorFraction)
                onDigitalDisplayModeRequested: mode => root.digitalDisplayModeRequested(mode)
                onSignalActivated: channelIndex => root.signalActivated(channelIndex)
            }
        }
    }
}
