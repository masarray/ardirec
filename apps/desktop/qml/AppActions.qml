// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtCore

QtObject {
    id: root

    property bool hasRecord: false
    property int diagnosticCount: 0
    property string currentView: "time"
    property string timeDisplayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property bool fullScreen: false

    signal openRequested()
    signal openRecentRequested(string url)
    signal propertiesRequested()
    signal diagnosticsRequested()
    signal signalsRequested()
    signal fitRequested()
    signal triggerRequested()
    signal zoomInRequested()
    signal zoomOutRequested()
    signal viewRequested(string viewName)
    signal waveformModeRequested(string mode)
    signal valueRepresentationRequested(string representation)
    signal fullScreenRequested()
    signal aboutRequested()

    property Settings recentStore: Settings {
        category: "RecentFiles"
        property string itemsJson: "[]"
    }

    readonly property var recentFiles: root.parseRecentFiles(recentStore.itemsJson)

    function parseRecentFiles(jsonText) {
        try {
            const value = JSON.parse(jsonText || "[]")
            if (!Array.isArray(value)) return []
            let result = []
            for (let entry of value) {
                const text = String(entry || "").trim()
                if (text.length && result.indexOf(text) < 0) result.push(text)
                if (result.length >= 5) break
            }
            return result
        } catch (error) {
            return []
        }
    }

    function noteRecentFile(url) {
        const text = String(url || "").trim()
        if (!text.length) return
        let items = root.recentFiles.slice()
        items = items.filter(entry => entry !== text)
        items.unshift(text)
        if (items.length > 5) items.length = 5
        recentStore.itemsJson = JSON.stringify(items)
    }

    function clearRecentFiles() {
        recentStore.itemsJson = "[]"
    }

    function recentDisplayName(index) {
        if (index < 0 || index >= root.recentFiles.length) return ""
        let text = root.recentFiles[index]
        try { text = decodeURIComponent(text) } catch (error) { }
        text = text.replace(/\\/g, "/")
        const parts = text.split("/")
        const name = parts.length ? parts[parts.length - 1] : text
        return name.length ? name : root.recentFiles[index]
    }

    function requestRecent(index) {
        if (index < 0 || index >= root.recentFiles.length) return
        root.openRecentRequested(root.recentFiles[index])
    }

    property Action openRecord: Action {
        text: "&Open COMTRADE…"
        shortcut: StandardKey.Open
        onTriggered: root.openRequested()
    }

    property Action recordProperties: Action {
        text: "Record &Properties…"
        shortcut: "Alt+Return"
        enabled: root.hasRecord
        onTriggered: root.propertiesRequested()
    }

    property Action recordDiagnostics: Action {
        text: "Record &Diagnostics…"
        enabled: root.hasRecord && root.diagnosticCount > 0
        onTriggered: root.diagnosticsRequested()
    }

    property Action exitApplication: Action {
        text: "E&xit"
        shortcut: StandardKey.Quit
        onTriggered: Qt.quit()
    }

    property Action signalConfiguration: Action {
        text: "Signal &Configuration…"
        shortcut: "Ctrl+R"
        enabled: root.hasRecord
        onTriggered: root.signalsRequested()
    }

    property Action timeView: Action {
        text: "&Time Signals"
        shortcut: "1"
        checkable: true
        checked: root.currentView === "time"
        enabled: root.hasRecord
        onTriggered: root.viewRequested("time")
    }

    property Action phasorView: Action {
        text: "&Phasor / Vector"
        shortcut: "2"
        checkable: true
        checked: root.currentView === "phasor"
        enabled: root.hasRecord
        onTriggered: root.viewRequested("phasor")
    }

    property Action locusView: Action {
        text: "R-&X Locus"
        shortcut: "3"
        checkable: true
        checked: root.currentView === "locus"
        enabled: root.hasRecord
        onTriggered: root.viewRequested("locus")
    }

    property Action harmonicsView: Action {
        text: "&Harmonics"
        shortcut: "4"
        checkable: true
        checked: root.currentView === "harmonics"
        enabled: root.hasRecord
        onTriggered: root.viewRequested("harmonics")
    }

    property Action tableView: Action {
        text: "Engineering &Table"
        shortcut: "5"
        checkable: true
        checked: root.currentView === "table"
        enabled: root.hasRecord
        onTriggered: root.viewRequested("table")
    }

    property Action fitRecord: Action {
        text: "&Fit Complete Record"
        shortcut: "Ctrl+0"
        enabled: root.hasRecord
        onTriggered: root.fitRequested()
    }

    property Action focusTrigger: Action {
        text: "Focus &Trigger"
        enabled: root.hasRecord
        onTriggered: root.triggerRequested()
    }

    property Action zoomIn: Action {
        text: "Zoom &In"
        enabled: root.hasRecord
        onTriggered: root.zoomInRequested()
    }

    property Action zoomOut: Action {
        text: "Zoom &Out"
        enabled: root.hasRecord
        onTriggered: root.zoomOutRequested()
    }

    property Action waveformInstant: Action {
        text: "&Instantaneous"
        checkable: true
        checked: root.timeDisplayMode === "instantaneous"
        enabled: root.hasRecord
        onTriggered: root.waveformModeRequested("instantaneous")
    }

    property Action waveformRms: Action {
        text: "&R.M.S."
        checkable: true
        checked: root.timeDisplayMode === "rms"
        enabled: root.hasRecord
        onTriggered: root.waveformModeRequested("rms")
    }

    property Action secondaryValues: Action {
        text: "&Secondary Values"
        checkable: true
        checked: root.valueRepresentation === "secondary"
        enabled: root.hasRecord
        onTriggered: root.valueRepresentationRequested("secondary")
    }

    property Action primaryValues: Action {
        text: "&Primary Values"
        checkable: true
        checked: root.valueRepresentation === "primary"
        enabled: root.hasRecord
        onTriggered: root.valueRepresentationRequested("primary")
    }

    property Action toggleFullScreen: Action {
        text: root.fullScreen ? "Exit Full Screen" : "Full Screen"
        shortcut: "F11"
        checkable: true
        checked: root.fullScreen
        onTriggered: root.fullScreenRequested()
    }

    property Action about: Action {
        text: "&About ArdIREC…"
        onTriggered: root.aboutRequested()
    }
}
