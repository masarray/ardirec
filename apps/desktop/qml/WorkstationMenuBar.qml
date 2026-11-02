// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

MenuBar {
    id: root
    property var actions
    implicitHeight: 30
    background: Rectangle {
        color: "#f7f9fb"
        border.color: "#c5d0d8"
    }

    delegate: MenuBarItem {
        id: menuItem
        implicitHeight: 29
        implicitWidth: Math.max(52, contentItem.implicitWidth + 22)
        contentItem: Text {
            // Strip the mnemonic marker from the custom-painted caption;
            // MenuBarItem still owns Alt-key and keyboard navigation.
            text: menuItem.text.replace("&", "")
            color: menuItem.highlighted ? "#173e59" : "#283b49"
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            font.weight: menuItem.highlighted ? Font.DemiBold : Font.Normal
        }
        background: Rectangle {
            color: menuItem.highlighted ? "#dce8f1" : "transparent"
            border.color: menuItem.highlighted ? "#a4b8c8" : "transparent"
            radius: 3
        }
    }

    Menu {
        title: "&File"

        MenuItem { action: root.actions?.openRecord ?? null; icon.source: "material/folder_open.svg"; icon.color: "#337fa7" }

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

        MenuItem { action: root.actions?.closeRecord ?? null }
        MenuSeparator { }
        MenuItem { action: root.actions?.recordProperties ?? null }
        MenuItem { action: root.actions?.recordDiagnostics ?? null }
        MenuSeparator { }
        MenuItem { action: root.actions?.exitApplication ?? null }
    }

    Menu {
        title: "&Signals"
        MenuItem { action: root.actions?.signalConfiguration ?? null; icon.source: "material/tune.svg"; icon.color: "#5b6c7b" }
    }

    Menu {
        title: "&Analysis"
        MenuItem { action: root.actions?.timeView ?? null; icon.source: "material/show_chart.svg"; icon.color: "#287ba9" }
        MenuSeparator { }
        MenuItem { action: root.actions?.phasorView ?? null; icon.source: "material/adjust.svg"; icon.color: "#5066b0" }
        MenuItem { action: root.actions?.locusView ?? null; icon.source: "material/my_location.svg"; icon.color: "#2c826b" }
        MenuItem { action: root.actions?.harmonicsView ?? null; icon.source: "material/bar_chart.svg"; icon.color: "#a0782a" }
        MenuItem { action: root.actions?.tableView ?? null; icon.source: "material/table_chart.svg"; icon.color: "#5b657d" }
    }

    Menu {
        title: "&Window"

        Menu {
            title: "&New Analysis Window"
            MenuItem { action: root.actions?.newTimeView ?? null }
            MenuItem { action: root.actions?.newPhasorView ?? null }
            MenuItem { action: root.actions?.newLocusView ?? null }
            MenuItem { action: root.actions?.newHarmonicsView ?? null }
            MenuItem { action: root.actions?.newTableView ?? null }
        }

        MenuItem { action: root.actions?.closeAnalysisWindow ?? null }
        MenuSeparator { }
        MenuItem { action: root.actions?.nextAnalysisWindow ?? null }
        MenuItem { action: root.actions?.previousAnalysisWindow ?? null }
        MenuSeparator { }
        MenuItem { action: root.actions?.freeArrangement ?? null }
        MenuItem { action: root.actions?.cascadeWindows ?? null }
        MenuItem { action: root.actions?.tileHorizontalWindows ?? null }
        MenuItem { action: root.actions?.tileVerticalWindows ?? null }
    }

    Menu {
        title: "&View"
        MenuItem { action: root.actions?.fitRecord ?? null }
        MenuItem { action: root.actions?.focusTrigger ?? null }
        MenuSeparator { }
        MenuItem { action: root.actions?.zoomIn ?? null; icon.source: "material/zoom_in.svg" }
        MenuItem { action: root.actions?.zoomOut ?? null; icon.source: "material/zoom_out.svg" }
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
