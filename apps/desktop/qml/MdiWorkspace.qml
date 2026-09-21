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
    // free | tile-horizontal | tile-vertical
    // Tile is a managed split topology: adjacent panes share one boundary.
    property string arrangementMode: "free"
    property int nextWindowId: 1
    property int nextZOrder: 1
    property var activationHistory: []

    // R6.3: Free mode owns a logical desktop independent of the viewport.
    // Only UI geometry participates in this model; record/analysis work is not
    // involved in move, scroll, or resize.
    property real logicalContentWidth: width
    property real logicalContentHeight: height
    property int autoScrollWindowId: -1
    property real dragViewportX: 0
    property real dragViewportY: 0
    property real autoScrollMargin: 40
    property real autoScrollMaxStep: 18

    readonly property real workspaceContentWidth: logicalContentWidth
    readonly property real workspaceContentHeight: logicalContentHeight
    readonly property real workspaceScrollX: workspaceFlick.contentX
    readonly property real workspaceScrollY: workspaceFlick.contentY
    readonly property real horizontalScrollRange: Math.max(0, logicalContentWidth - root.width)
    readonly property real verticalScrollRange: Math.max(0, logicalContentHeight - root.height)

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

    function leaveManagedArrangement() {
        if (arrangementMode !== "free") {
            arrangementMode = "free"
            recomputeWorkspaceExtents()
        }
    }

    function sortedArrangementIndices(horizontalAxis) {
        const indices = arrangementIndices()
        indices.sort(function(a, b) {
            const left = windowsModel.get(a)
            const right = windowsModel.get(b)
            return horizontalAxis ? left.windowX - right.windowX : left.windowY - right.windowY
        })
        return indices
    }

    function findSlotById(indices, windowId) {
        for (let slot = 0; slot < indices.length; ++slot)
            if (windowsModel.get(indices[slot]).windowId === windowId) return slot
        return -1
    }

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

        // Adding a free child changes the topology. Keep the current rectangles
        // but explicitly leave Tile rather than silently creating an unmanaged
        // overlapping child inside a tiled set.
        leaveManagedArrangement()
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
        recomputeWorkspaceExtents()
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

    function recomputeWorkspaceExtents() {
        if (arrangementMode !== "free") {
            logicalContentWidth = Math.max(0, root.width)
            logicalContentHeight = Math.max(0, root.height)
            workspaceFlick.contentX = 0
            workspaceFlick.contentY = 0
            return
        }

        let right = Math.max(0, root.width)
        let bottom = Math.max(0, root.height)
        for (let i = 0; i < windowsModel.count; ++i) {
            const row = windowsModel.get(i)
            if (row.windowState === "minimized") continue
            right = Math.max(right, Math.max(0, row.windowX) + Math.max(0, row.windowWidth))
            bottom = Math.max(bottom, Math.max(0, row.windowY) + Math.max(0, row.windowHeight))
        }

        const margin = 24
        logicalContentWidth = right > root.width + 0.5 ? right + margin : Math.max(0, root.width)
        logicalContentHeight = bottom > root.height + 0.5 ? bottom + margin : Math.max(0, root.height)

        const maxX = Math.max(0, logicalContentWidth - root.width)
        const maxY = Math.max(0, logicalContentHeight - root.height)
        workspaceFlick.contentX = clamp(workspaceFlick.contentX, 0, maxX)
        workspaceFlick.contentY = clamp(workspaceFlick.contentY, 0, maxY)
    }

    function updateGeometry(windowId, x, y, widthValue, heightValue) {
        const index = findIndexById(windowId)
        if (index < 0 || windowsModel.get(index).windowState !== "normal") return
        if (arrangementMode !== "free") return
        // Keep top/left reachable, but never clamp right/bottom to the current
        // viewport. That would make the window resist the mouse at the edge.
        const w = Math.max(360, widthValue)
        const h = Math.max(240, heightValue)
        windowsModel.setProperty(index, "windowWidth", w)
        windowsModel.setProperty(index, "windowHeight", h)
        windowsModel.setProperty(index, "windowX", Math.max(0, x))
        windowsModel.setProperty(index, "windowY", Math.max(0, y))
        recomputeWorkspaceExtents()
    }

    function beginFreeDrag(windowId, viewportX, viewportY) {
        const index = findIndexById(windowId)
        if (arrangementMode !== "free" || index < 0
                || windowsModel.get(index).windowState !== "normal") return
        autoScrollWindowId = windowId
        dragViewportX = viewportX
        dragViewportY = viewportY
    }

    function updateFreeDragViewport(windowId, viewportX, viewportY, active) {
        if (!active) {
            endFreeDrag(windowId)
            return
        }
        beginFreeDrag(windowId, viewportX, viewportY)
    }

    function endFreeDrag(windowId) {
        if (windowId < 0 || autoScrollWindowId === windowId)
            autoScrollWindowId = -1
    }

    function autoScrollAxisDelta(pointer, viewportExtent, currentScroll, maxScroll) {
        if (viewportExtent <= 0 || autoScrollMargin <= 0) return 0
        const margin = Math.min(autoScrollMargin, viewportExtent * 0.25)
        if (pointer < margin && currentScroll > 0) {
            const strength = clamp((margin - pointer) / margin, 0, 1)
            return -Math.max(1, Math.round(autoScrollMaxStep * strength))
        }
        if (pointer > viewportExtent - margin && currentScroll < maxScroll) {
            const strength = clamp((pointer - (viewportExtent - margin)) / margin, 0, 1)
            return Math.max(1, Math.round(autoScrollMaxStep * strength))
        }
        return 0
    }

    function stepAutoScroll() {
        if (autoScrollWindowId < 0 || arrangementMode !== "free") return false
        const index = findIndexById(autoScrollWindowId)
        if (index < 0 || windowsModel.get(index).windowState !== "normal") {
            autoScrollWindowId = -1
            return false
        }

        const oldX = workspaceFlick.contentX
        const oldY = workspaceFlick.contentY
        const maxX = Math.max(0, logicalContentWidth - root.width)
        const maxY = Math.max(0, logicalContentHeight - root.height)
        const requestDx = autoScrollAxisDelta(dragViewportX, root.width, oldX, maxX)
        const requestDy = autoScrollAxisDelta(dragViewportY, root.height, oldY, maxY)
        const nextX = clamp(oldX + requestDx, 0, maxX)
        const nextY = clamp(oldY + requestDy, 0, maxY)
        const actualDx = nextX - oldX
        const actualDy = nextY - oldY
        if (Math.abs(actualDx) < 0.01 && Math.abs(actualDy) < 0.01) return false

        workspaceFlick.contentX = nextX
        workspaceFlick.contentY = nextY

        // Move the logical child by the same amount as the viewport origin so
        // its screen-space grab point remains under the stationary pointer.
        const row = windowsModel.get(index)
        windowsModel.setProperty(index, "windowX", Math.max(0, row.windowX + actualDx))
        windowsModel.setProperty(index, "windowY", Math.max(0, row.windowY + actualDy))
        recomputeWorkspaceExtents()
        return true
    }

    function tileBoundaryPosition(windowId, edgeMask, x, y, widthValue, heightValue) {
        if (arrangementMode === "tile-vertical") {
            if (edgeMask & 1) return x
            if (edgeMask & 2) return x + widthValue
        } else if (arrangementMode === "tile-horizontal") {
            if (edgeMask & 4) return y
            if (edgeMask & 8) return y + heightValue
        }
        return Number.NaN
    }

    function resizeTileBoundary(windowId, edgeMask, x, y, widthValue, heightValue) {
        if (arrangementMode === "free") {
            updateGeometry(windowId, x, y, widthValue, heightValue)
            return
        }

        const vertical = arrangementMode === "tile-vertical"
        const indices = sortedArrangementIndices(vertical)
        const slot = findSlotById(indices, windowId)
        if (slot < 0) return

        let leftSlot = -1
        let rightSlot = -1
        if (vertical) {
            if ((edgeMask & 2) && slot + 1 < indices.length) {
                leftSlot = slot
                rightSlot = slot + 1
            } else if ((edgeMask & 1) && slot > 0) {
                leftSlot = slot - 1
                rightSlot = slot
            } else {
                return
            }
        } else {
            if ((edgeMask & 8) && slot + 1 < indices.length) {
                leftSlot = slot
                rightSlot = slot + 1
            } else if ((edgeMask & 4) && slot > 0) {
                leftSlot = slot - 1
                rightSlot = slot
            } else {
                return
            }
        }

        const firstIndex = indices[leftSlot]
        const secondIndex = indices[rightSlot]
        const first = windowsModel.get(firstIndex)
        const second = windowsModel.get(secondIndex)
        let boundary = tileBoundaryPosition(windowId, edgeMask, x, y, widthValue, heightValue)
        if (!Number.isFinite(boundary)) return

        if (vertical) {
            const pairStart = first.windowX
            const pairEnd = second.windowX + second.windowWidth
            const pairSpan = Math.max(0, pairEnd - pairStart)
            const minimum = Math.min(360, pairSpan * 0.5)
            boundary = clamp(boundary, pairStart + minimum, pairEnd - minimum)
            setArrangedGeometry(firstIndex, pairStart, first.windowY,
                                boundary - pairStart, first.windowHeight)
            setArrangedGeometry(secondIndex, boundary, second.windowY,
                                pairEnd - boundary, second.windowHeight)
        } else {
            const pairStart = first.windowY
            const pairEnd = second.windowY + second.windowHeight
            const pairSpan = Math.max(0, pairEnd - pairStart)
            const minimum = Math.min(240, pairSpan * 0.5)
            boundary = clamp(boundary, pairStart + minimum, pairEnd - minimum)
            setArrangedGeometry(firstIndex, first.windowX, pairStart,
                                first.windowWidth, boundary - pairStart)
            setArrangedGeometry(secondIndex, second.windowX, boundary,
                                second.windowWidth, pairEnd - boundary)
        }
    }

    function closeWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        leaveManagedArrangement()
        const wasActive = activeWindowId === windowId
        windowsModel.remove(index)
        removeHistory(windowId)
        if (wasActive) chooseTopmostWindow()
        else recomputeRequestOwners()
        layoutMinimized()
        recomputeWorkspaceExtents()
    }

    function closeActiveWindow() {
        if (activeWindowId >= 0) closeWindow(activeWindowId)
    }

    function minimizeWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        leaveManagedArrangement()
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
        recomputeWorkspaceExtents()
    }

    function restoreWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        leaveManagedArrangement()
        const row = windowsModel.get(index)
        const w = Math.max(360, row.restoreWidth)
        const h = Math.max(240, row.restoreHeight)
        windowsModel.setProperty(index, "windowState", "normal")
        windowsModel.setProperty(index, "windowWidth", w)
        windowsModel.setProperty(index, "windowHeight", h)
        windowsModel.setProperty(index, "windowX", Math.max(0, row.restoreX))
        windowsModel.setProperty(index, "windowY", Math.max(0, row.restoreY))
        activateWindow(windowId)
        layoutMinimized()
        recomputeWorkspaceExtents()
    }

    function maximizeWindow(windowId) {
        const index = findIndexById(windowId)
        if (index < 0) return
        leaveManagedArrangement()
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
        workspaceFlick.contentX = 0
        workspaceFlick.contentY = 0
        recomputeWorkspaceExtents()
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
        leaveManagedArrangement()
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
        workspaceFlick.contentX = 0
        workspaceFlick.contentY = 0
        recomputeRequestOwners()
        recomputeWorkspaceExtents()
    }

    function tileHorizontal() {
        const indices = arrangementIndices()
        if (!indices.length) return
        normalizeForArrangement(indices)
        arrangementMode = "tile-horizontal"
        const eachHeight = root.height / indices.length
        for (let slot = 0; slot < indices.length; ++slot)
            setArrangedGeometry(indices[slot], 0, slot * eachHeight, root.width, eachHeight)
        workspaceFlick.contentX = 0
        workspaceFlick.contentY = 0
        recomputeRequestOwners()
        recomputeWorkspaceExtents()
    }

    function tileVertical() {
        const indices = arrangementIndices()
        if (!indices.length) return
        normalizeForArrangement(indices)
        arrangementMode = "tile-vertical"
        const eachWidth = root.width / indices.length
        for (let slot = 0; slot < indices.length; ++slot)
            setArrangedGeometry(indices[slot], slot * eachWidth, 0, eachWidth, root.height)
        workspaceFlick.contentX = 0
        workspaceFlick.contentY = 0
        recomputeRequestOwners()
        recomputeWorkspaceExtents()
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

    function relayoutManagedTiles() {
        if (arrangementMode === "free") return false
        const vertical = arrangementMode === "tile-vertical"
        const indices = sortedArrangementIndices(vertical)
        if (!indices.length) return true

        if (vertical) {
            const last = windowsModel.get(indices[indices.length - 1])
            const oldExtent = last.windowX + last.windowWidth
            const scale = oldExtent > 0 ? root.width / oldExtent : 1.0
            for (let index of indices) {
                const row = windowsModel.get(index)
                setArrangedGeometry(index, row.windowX * scale, 0,
                                    row.windowWidth * scale, root.height)
            }
        } else {
            const last = windowsModel.get(indices[indices.length - 1])
            const oldExtent = last.windowY + last.windowHeight
            const scale = oldExtent > 0 ? root.height / oldExtent : 1.0
            for (let index of indices) {
                const row = windowsModel.get(index)
                setArrangedGeometry(index, 0, row.windowY * scale,
                                    root.width, row.windowHeight * scale)
            }
        }
        return true
    }

    function relayoutSpecialWindows() {
        if (relayoutManagedTiles()) return
        for (let i = 0; i < windowsModel.count; ++i) {
            const row = windowsModel.get(i)
            if (row.windowState === "maximized") {
                windowsModel.setProperty(i, "windowX", 0)
                windowsModel.setProperty(i, "windowY", 0)
                windowsModel.setProperty(i, "windowWidth", root.width)
                windowsModel.setProperty(i, "windowHeight", root.height)
            } else if (row.windowState === "normal") {
                // A viewport resize must not pull free children back inside the
                // visible rectangle. Preserve their logical desktop geometry.
                windowsModel.setProperty(i, "windowWidth", Math.max(360, row.windowWidth))
                windowsModel.setProperty(i, "windowHeight", Math.max(240, row.windowHeight))
                windowsModel.setProperty(i, "windowX", Math.max(0, row.windowX))
                windowsModel.setProperty(i, "windowY", Math.max(0, row.windowY))
            }
        }
        layoutMinimized()
        recomputeWorkspaceExtents()
    }

    function clearWindows() {
        arrangementMode = "free"
        autoScrollWindowId = -1
        windowsModel.clear()
        activeWindowId = -1
        activationHistory = []
        nextWindowId = 1
        nextZOrder = 1
        logicalContentWidth = Math.max(0, root.width)
        logicalContentHeight = Math.max(0, root.height)
        workspaceFlick.contentX = 0
        workspaceFlick.contentY = 0
        activeWindowChanged(-1, "time")
    }

    function resetForRecord() {
        clearWindows()
        if (hasRecord) openView("time", true)
    }

    onHasRecordChanged: if (!hasRecord) clearWindows()
    onWidthChanged: Qt.callLater(relayoutSpecialWindows)
    onHeightChanged: Qt.callLater(relayoutSpecialWindows)

    Flickable {
        id: workspaceFlick
        anchors.fill: parent
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: root.logicalContentWidth
        contentHeight: root.logicalContentHeight

        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AsNeeded
            interactive: true
        }
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            interactive: true
        }

        Item {
            id: workspaceCanvas
            width: workspaceFlick.contentWidth
            height: workspaceFlick.contentHeight

            Repeater {
                model: windowsModel

                delegate: MdiChildWindow {
                    id: childWindow
                    workspaceWidth: root.logicalContentWidth
                    workspaceHeight: root.logicalContentHeight
                    workspaceViewportItem: workspaceFlick
                    activeWindow: root.activeWindowId === windowId
                    tileManaged: root.arrangementMode !== "free"

                    onActivateRequested: root.activateWindow(windowId)
                    onCloseRequested: root.closeWindow(windowId)
                    onMinimizeRequested: root.minimizeWindow(windowId)
                    onRestoreRequested: root.restoreWindow(windowId)
                    onToggleMaximizeRequested: root.toggleMaximize(windowId)
                    onGeometryRequested: (gx, gy, gw, gh) =>
                        root.updateGeometry(windowId, gx, gy, gw, gh)
                    onResizeRequested: (edgeMask, gx, gy, gw, gh) =>
                        root.resizeTileBoundary(windowId, edgeMask, gx, gy, gw, gh)
                    onDragViewportRequested: (vx, vy, active) =>
                        root.updateFreeDragViewport(windowId, vx, vy, active)

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
    }

    Timer {
        interval: 16
        repeat: true
        running: root.autoScrollWindowId >= 0
        onTriggered: root.stepAutoScroll()
    }

    Label {
        anchors.centerIn: parent
        visible: root.hasRecord && windowsModel.count === 0
        text: "No analysis windows open · choose Analysis or Window > New"
        color: "#7a8389"
        font.pixelSize: 10
        z: 100
    }
}
