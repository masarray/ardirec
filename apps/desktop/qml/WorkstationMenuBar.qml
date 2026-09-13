// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

MenuBar {
    id: root
    property var actions

    Menu {
        title: "&File"

        MenuItem { action: root.actions?.openRecord ?? null }

        Menu {
            id: recentMenu
            title: "Open &Recent"
            enabled: root.actions && root.actions.recentFiles.length > 0

            MenuItem {
                visible: root.actions && root.actions.recentFiles.length > 0
                text: visible ? "&1  " + root.actions.recentDisplayName(0) : ""
                onTriggered: root.actions.requestRecent(0)
            }
            MenuItem {
                visible: root.actions && root.actions.recentFiles.length > 1
                text: visible ? "&2  " + root.actions.recentDisplayName(1) : ""
                onTriggered: root.actions.requestRecent(1)
            }
            MenuItem {
                visible: root.actions && root.actions.recentFiles.length > 2
                text: visible ? "&3  " + root.actions.recentDisplayName(2) : ""
                onTriggered: root.actions.requestRecent(2)
            }
            MenuItem {
                visible: root.actions && root.actions.recentFiles.length > 3
                text: visible ? "&4  " + root.actions.recentDisplayName(3) : ""
                onTriggered: root.actions.requestRecent(3)
            }
            MenuItem {
                visible: root.actions && root.actions.recentFiles.length > 4
                text: visible ? "&5  " + root.actions.recentDisplayName(4) : ""
                onTriggered: root.actions.requestRecent(4)
            }
            MenuSeparator { visible: root.actions && root.actions.recentFiles.length > 0 }
            MenuItem {
                text: "Clear Recent Files"
                enabled: root.actions && root.actions.recentFiles.length > 0
                onTriggered: root.actions.clearRecentFiles()
            }
        }

        MenuSeparator { }
        MenuItem { action: root.actions?.recordProperties ?? null }
        MenuItem { action: root.actions?.recordDiagnostics ?? null }
        MenuSeparator { }
        MenuItem { action: root.actions?.exitApplication ?? null }
    }

    Menu {
        title: "&Signals"
        MenuItem { action: root.actions?.signalConfiguration ?? null }
    }

    Menu {
        title: "&Analysis"
        MenuItem { action: root.actions?.timeView ?? null }
        MenuItem { action: root.actions?.phasorView ?? null }
        MenuItem { action: root.actions?.locusView ?? null }
        MenuItem { action: root.actions?.harmonicsView ?? null }
        MenuItem { action: root.actions?.tableView ?? null }
    }

    Menu {
        title: "&View"
        MenuItem { action: root.actions?.fitRecord ?? null }
        MenuItem { action: root.actions?.focusTrigger ?? null }
        MenuSeparator { }
        MenuItem { action: root.actions?.zoomIn ?? null }
        MenuItem { action: root.actions?.zoomOut ?? null }
        MenuSeparator { }

        Menu {
            title: "&Waveform"
            MenuItem { action: root.actions?.waveformInstant ?? null }
            MenuItem { action: root.actions?.waveformRms ?? null }
        }

        Menu {
            title: "&Values"
            MenuItem { action: root.actions?.secondaryValues ?? null }
            MenuItem { action: root.actions?.primaryValues ?? null }
        }

        MenuSeparator { }
        MenuItem { action: root.actions?.toggleFullScreen ?? null }
    }

    Menu {
        title: "&Help"
        MenuItem { action: root.actions?.about ?? null }
    }
}
