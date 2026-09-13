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
        for (let id of activationHistory)
            if (id !== windowId && findIndexById(id) >= 0) next.push(id)
        next.push(windowId)
        activationHistory = next
    }

    function removeHistory(windowId) {
        let next = []
        for (let id of activationHistory)
            if (id !== windowId && findIndexById(id) >= 0) next.push(id)
        activationHistory = next
    }

    function setNoActiveWindow() {
        activeWindowId = -1
        recomputeRequestOwners()
        activeWindowChanged(-1, "time")
    }

    function chooseTopmostWindow() {
        let bestId = -1
        let bestZ = -1
        for (let i = 0; i < windowsModel.count; ++i) {
            const row = windowsModel.get(i)
            if (row.windowState === "minimized") continue
            if (row.zOrder > bestZ) {
                bestZ = row.zOrder
                bestId = row.windowId
            }
        }
        if (bestId >= 0) activateWindow(bestId)
        else setNoActiveWindow()
    }

    function recomputeRequestOwners() {
        const types = ["time", "phasor", "locus", "harmonics", "table"]
        for (let type of types) {
            let ownerId = -1
            const activeIndex = findIndexById(activeWindowId)
            if (activeIndex >= 0) {
                const active = windowsModel.get(activeIndex)
                if (active.viewType === type && active.windowState !== "minimized")
                    ownerId = active.windowId
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
        const w = Math.max(420, Math.min(root.width * 0.78, root.width - 24))
        const h = Math.max(300, Math.min(root.height * 0.76, root.height - 24))
        const maxX = Math.max(0, root.width - w - 8)
        const maxY = Math.max(0, root.height - h - 8)
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
            zOrder: ++nextZOrder,
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
        else recomputeRequestOwners()
        layoutMinimized()
    }

    function restoreWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        const row = windowsModel.get(index)
        const w = Math.min(Math.max(360, row.restoreWidth), Math.max(360, root.width))
        const h = Math.min(Math.max(240, row.restoreHeight), Math.max(240, root.height))
        windowsModel.setProperty(index, "windowState", "normal")
        windowsModel.setProperty(index, "windowWidth", w)
        windowsModel.setProperty(index, "windowHeight", h)
        windowsModel.setProperty(index, "windowX", clamp(row.restoreX, 0, Math.max(0, root.width - w)))
        windowsModel.setProperty(index, "windowY", clamp(row.restoreY, 0, Math.max(0, root.height - h)))
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
        windowsModel.setProperty(index, "windowWidth", root.width)
        windowsModel.setProperty(index, "windowHeight", root.height)
        activateWindow(windowId)
    }

    function toggleMaximize(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        if (windowsModel.get(index).windowState === "maximized") restoreWindow(windowId)
        else maximizeWindow(windowId)
    }

    function arrangementIndices() {
        let result = []
        for (let i = 0; i < windowsModel.count; ++i)
            if (windowsModel.get(i).windowState !== "minimized") result.push(i)
        return result
    }

    function normalizeForArrangement(indices) {
        for (let index of indices) windowsModel.setProperty(index, "windowState", "normal")
    }

    function setArrangedGeometry(index, x, y, widthValue, heightValue) {
        windowsModel.setProperty(index, "windowX", x)
        windowsModel.setProperty(index, "windowY", y)
        windowsModel.setProperty(index, "windowWidth", widthValue)
        windowsModel.setProperty(index, "windowHeight", heightValue)
        windowsModel.setProperty(index, "restoreX", x)
        windowsModel.setProperty(index, "restoreY", y)
        windowsModel.setProperty(index, "restoreWidth", widthValue)
        windowsModel.setProperty(index, "restoreHeight", heightValue)
    }

    function cascade() {
        const indices = arrangementIndices()
        if (!indices.length) return
        normalizeForArrangement(indices)
        const offset = 28
        const w = Math.max(360, Math.min(root.width * 0.78, root.width - 16))
        const h = Math.max(240, Math.min(root.height * 0.76, root.height - 16))
        for (let slot = 0; slot < indices.length; ++slot) {
            const x = Math.min(Math.max(0, root.width - w), 8 + (slot % 8) * offset)
            const y = Math.min(Math.max(0, root.height - h), 8 + (slot % 8) * offset)
            setArrangedGeometry(indices[slot], x, y, w, h)
        }
        recomputeRequestOwners()
    }

    function tileHorizontal() {
        const indices = arrangementIndices()
        if (!indices.length) return
        normalizeForArrangement(indices)
        const eachHeight = root.height / indices.length
        for (let slot = 0; slot < indices.length; ++slot)
            setArrangedGeometry(indices[slot], 0, slot * eachHeight, root.width, eachHeight)
        recomputeRequestOwners()
    }

    function tileVertical() {
        const indices = arrangementIndices()
        if (!indices.length) return
        normalizeForArrangement(indices)
        const eachWidth = root.width / indices.length
        for (let slot = 0; slot < indices.length; ++slot)
            setArrangedGeometry(indices[slot], slot * eachWidth, 0, eachWidth, root.height)
        recomputeRequestOwners()
    }

    function activateNext() {
        if (!windowsModel.count) return
        const current = findIndexById(activeWindowId)
        for (let step = 1; step <= windowsModel.count; ++step) {
            const index = ((current < 0 ? -1 : current) + step + windowsModel.count) % windowsModel.count
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
            const previousIndex = findIndexById(previous)
            if (previousIndex >= 0 && windowsModel.get(previousIndex).windowState !== "minimized") {
                activateWindow(previous)
                return
            }
        }
        const current = findIndexById(activeWindowId)
        for (let step = 1; step <= windowsModel.count; ++step) {
            const index = ((current < 0 ? 0 : current) - step + windowsModel.count * 2) % windowsModel.count
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
        const shelfWidth = Math.max(170, Math.min(230,
                              (root.width - 18) / Math.max(1, Math.min(4, minimized.length))))
        const columns = Math.max(1, Math.floor((root.width - 12) / (shelfWidth + 6)))
        for (let slot = 0; slot < minimized.length; ++slot) {
            const index = minimized[slot]
            const column = slot % columns
            const row = Math.floor(slot / columns)
            windowsModel.setProperty(index, "windowX", 6 + column * (shelfWidth + 6))
            windowsModel.setProperty(index, "windowY", Math.max(0, root.height - 34 * (row + 1)))
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
                windowsModel.setProperty(i, "windowWidth", root.width)
                windowsModel.setProperty(i, "windowHeight", root.height)
            } else if (row.windowState === "normal") {
                const w = Math.min(row.windowWidth, Math.max(360, root.width))
                const h = Math.min(row.windowHeight, Math.max(240, root.height))
                windowsModel.setProperty(i, "windowWidth", w)
                windowsModel.setProperty(i, "windowHeight", h)
                windowsModel.setProperty(i, "windowX", clamp(row.windowX, 0, Math.max(0, root.width - w)))
                windowsModel.setProperty(i, "windowY", clamp(row.windowY, 0, Math.max(0, root.height - h)))
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
