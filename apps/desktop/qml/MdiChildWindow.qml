// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property int windowId: -1
    property string windowTitle: "Analysis"
    property string viewType: "time"
    property string windowState: "normal" // normal | minimized | maximized
    property bool activeWindow: false
    property real workspaceWidth: parent ? parent.width : width
    property real workspaceHeight: parent ? parent.height : height
    property real minimumWindowWidth: 360
    property real minimumWindowHeight: 240

    signal activateRequested()
    signal closeRequested()
    signal minimizeRequested()
    signal restoreRequested()
    signal toggleMaximizeRequested()
    signal geometryRequested(real x, real y, real width, real height)

    default property alias contentData: contentHost.data

    function clamp(value, lo, hi) { return Math.max(lo, Math.min(hi, value)) }

    Rectangle {
        id: frame
        anchors.fill: parent
        color: "#ffffff"
        border.width: root.activeWindow ? 2 : 1
        border.color: root.activeWindow ? "#4d718f" : "#8f999f"
        radius: 2
        clip: true

        Rectangle {
            id: titleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 30
            color: root.activeWindow ? "#dbe5ed" : "#e9edef"
            border.color: root.activeWindow ? "#9fb3c3" : "#c3c9cd"

            MouseArea {
                id: titleDrag
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true
                cursorShape: root.windowState === "normal" ? Qt.SizeAllCursor : Qt.ArrowCursor
                property real pressWorkspaceX: 0
                property real pressWorkspaceY: 0
                property real startX: 0
                property real startY: 0

                onPressed: mouse => {
                    root.activateRequested()
                    const p = mapToItem(root.parent, mouse.x, mouse.y)
                    pressWorkspaceX = p.x
                    pressWorkspaceY = p.y
                    startX = root.x
                    startY = root.y
                }
                onPositionChanged: mouse => {
                    if (!pressed || root.windowState !== "normal") return
                    const p = mapToItem(root.parent, mouse.x, mouse.y)
                    const nx = root.clamp(startX + p.x - pressWorkspaceX,
                                          0, Math.max(0, root.workspaceWidth - root.width))
                    const ny = root.clamp(startY + p.y - pressWorkspaceY,
                                          0, Math.max(0, root.workspaceHeight - root.height))
                    root.geometryRequested(nx, ny, root.width, root.height)
                }
                onClicked: {
                    if (root.windowState === "minimized") root.restoreRequested()
                    else root.activateRequested()
                }
                onDoubleClicked: if (root.windowState !== "minimized") root.toggleMaximizeRequested()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 4
                spacing: 4

                Rectangle {
                    width: 7
                    height: 7
                    radius: 3.5
                    color: root.viewType === "time" ? "#326c9b"
                         : root.viewType === "phasor" ? "#4f65a5"
                         : root.viewType === "locus" ? "#2c7b69"
                         : root.viewType === "harmonics" ? "#906b2a" : "#6a647b"
                }
                Label {
                    Layout.fillWidth: true
                    text: root.windowTitle
                    color: "#29343b"
                    font.pixelSize: 10
                    font.weight: root.activeWindow ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                ToolButton {
                    visible: root.windowState !== "minimized"
                    text: "—"
                    font.pixelSize: 11
                    Layout.preferredWidth: 25
                    Layout.preferredHeight: 24
                    padding: 0
                    onClicked: root.minimizeRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Minimize analysis window"
                }
                ToolButton {
                    visible: root.windowState !== "minimized"
                    text: root.windowState === "maximized" ? "❐" : "□"
                    font.pixelSize: 11
                    Layout.preferredWidth: 25
                    Layout.preferredHeight: 24
                    padding: 0
                    onClicked: root.toggleMaximizeRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: root.windowState === "maximized" ? "Restore" : "Maximize"
                }
                ToolButton {
                    text: "×"
                    font.pixelSize: 14
                    Layout.preferredWidth: 25
                    Layout.preferredHeight: 24
                    padding: 0
                    onClicked: root.closeRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Close analysis window"
                }
            }
        }

        Item {
            id: contentHost
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.bottom: parent.bottom
            anchors.margins: root.activeWindow ? 2 : 1
            visible: root.windowState !== "minimized"
            clip: true
        }
    }

    component ResizeHandle: MouseArea {
        id: handle
        required property int edgeMask // 1 left, 2 right, 4 top, 8 bottom
        visible: root.windowState === "normal"
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        z: 30

        property real pressWorkspaceX: 0
        property real pressWorkspaceY: 0
        property real startX: 0
        property real startY: 0
        property real startWidth: 0
        property real startHeight: 0

        cursorShape: (edgeMask === 1 || edgeMask === 2) ? Qt.SizeHorCursor
                   : (edgeMask === 4 || edgeMask === 8) ? Qt.SizeVerCursor
                   : (edgeMask === 5 || edgeMask === 10) ? Qt.SizeFDiagCursor
                   : Qt.SizeBDiagCursor

        onPressed: mouse => {
            root.activateRequested()
            const p = mapToItem(root.parent, mouse.x, mouse.y)
            pressWorkspaceX = p.x
            pressWorkspaceY = p.y
            startX = root.x
            startY = root.y
            startWidth = root.width
            startHeight = root.height
        }

        onPositionChanged: mouse => {
            if (!pressed || root.windowState !== "normal") return
            const p = mapToItem(root.parent, mouse.x, mouse.y)
            const dx = p.x - pressWorkspaceX
            const dy = p.y - pressWorkspaceY
            let nx = startX
            let ny = startY
            let nw = startWidth
            let nh = startHeight

            if (edgeMask & 1) {
                nx = root.clamp(startX + dx, 0, startX + startWidth - root.minimumWindowWidth)
                nw = startWidth - (nx - startX)
            }
            if (edgeMask & 2)
                nw = root.clamp(startWidth + dx, root.minimumWindowWidth,
                                Math.max(root.minimumWindowWidth, root.workspaceWidth - startX))
            if (edgeMask & 4) {
                ny = root.clamp(startY + dy, 0, startY + startHeight - root.minimumWindowHeight)
                nh = startHeight - (ny - startY)
            }
            if (edgeMask & 8)
                nh = root.clamp(startHeight + dy, root.minimumWindowHeight,
                                Math.max(root.minimumWindowHeight, root.workspaceHeight - startY))

            root.geometryRequested(nx, ny, nw, nh)
        }
    }

    ResizeHandle { edgeMask: 1; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6 }
    ResizeHandle { edgeMask: 2; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6 }
    ResizeHandle { edgeMask: 4; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 6 }
    ResizeHandle { edgeMask: 8; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; height: 6 }
    ResizeHandle { edgeMask: 5; anchors.left: parent.left; anchors.top: parent.top; width: 11; height: 11 }
    ResizeHandle { edgeMask: 6; anchors.right: parent.right; anchors.top: parent.top; width: 11; height: 11 }
    ResizeHandle { edgeMask: 9; anchors.left: parent.left; anchors.bottom: parent.bottom; width: 11; height: 11 }
    ResizeHandle { edgeMask: 10; anchors.right: parent.right; anchors.bottom: parent.bottom; width: 11; height: 11 }
}
