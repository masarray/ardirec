// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import Ardirec.Render 1.0

Rectangle {
    id: root
    color: "#fbfbfb"
    border.color: "#c8c8c8"

    property var document
    property var analysis
    property int channelIndex: -1
    property real zoomFactor: 1.0
    property real panFraction: 0.0
    property real viewStart: 0.0
    property real visibleDuration: 1.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property string displayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property color traceColor: "#315f8d"
    property real axisWidth: 92
    property color cursorAColor: "#244f9e"
    property color cursorBColor: "#b77900"

    readonly property real rawPeak: {
        const representationDependency = root.valueRepresentation
        return document && channelIndex >= 0 ? document.channelPeak(channelIndex) : 1.0
    }
    readonly property real displayPeak: displayMode === "rms" ? rawPeak / Math.sqrt(2.0) : rawPeak

    function formatAxis(value) {
        const magnitude = Math.abs(value)
        if (magnitude >= 100000) return value.toFixed(0)
        if (magnitude >= 10000) return value.toFixed(0)
        if (magnitude >= 1000) return value.toFixed(0)
        if (magnitude >= 100) return value.toFixed(1)
        if (magnitude >= 10) return value.toFixed(2)
        if (magnitude >= 1) return value.toFixed(3)
        return value.toFixed(4)
    }

    Rectangle {
        id: axis
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.axisWidth
        color: "#f4f4f4"
        border.color: "#cccccc"

        Label {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 5
            anchors.topMargin: 4
            width: parent.width - 10
            text: root.document && root.channelIndex >= 0 ? root.document.channelName(root.channelIndex) : "—"
            color: root.traceColor
            font.pixelSize: 9
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Label {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 5
            anchors.topMargin: 19
            text: (root.document ? root.document.channelUnit(root.channelIndex) : "")
                  + (root.displayMode === "rms" ? " · RMS" : "")
                  + " · " + (root.valueRepresentation === "primary" ? "PRI" : "SEC")
            color: "#707070"
            font.pixelSize: 7
            ToolTip.visible: ratioHover.containsMouse
            ToolTip.text: root.document && root.channelIndex >= 0 ? root.document.channelRatioText(root.channelIndex) : ""
        }
        MouseArea { id: ratioHover; anchors.left: parent.left; anchors.top: parent.top; width: parent.width; height: 34; hoverEnabled: true; acceptedButtons: Qt.NoButton }

        Label {
            anchors.right: parent.right
            anchors.rightMargin: 5
            y: Math.max(30, parent.height * 0.10 - height * 0.5)
            text: root.formatAxis(root.displayPeak)
            color: "#5c5c5c"
            font.pixelSize: 8
        }
        Label {
            anchors.right: parent.right
            anchors.rightMargin: 5
            y: parent.height * 0.50 - height * 0.5
            text: root.displayMode === "rms" ? root.formatAxis(root.displayPeak * 0.5) : "0"
            color: "#777777"
            font.pixelSize: 8
        }
        Label {
            anchors.right: parent.right
            anchors.rightMargin: 5
            y: parent.height * 0.90 - height * 0.5
            text: root.displayMode === "rms" ? "0" : root.formatAxis(-root.displayPeak)
            color: "#5c5c5c"
            font.pixelSize: 8
        }
    }

    Rectangle {
        id: chart
        anchors.left: axis.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: "#ffffff"
        clip: true

        Repeater {
            model: 11
            Rectangle {
                required property int index
                x: chart.width * index / 10
                y: 0
                width: 1
                height: chart.height
                color: index === 0 || index === 10 ? "#c3c3c3" : "#e4e4e4"
            }
        }

        Repeater {
            model: [0.10, 0.30, 0.50, 0.70, 0.90]
            Rectangle {
                required property real modelData
                x: 0
                y: chart.height * modelData
                width: chart.width
                height: 1
                color: modelData === 0.50 ? "#b8b8b8" : "#e1e1e1"
            }
        }

        WaveformItem {
            anchors.fill: parent
            visible: root.displayMode !== "rms"
            document: root.document
            channelIndex: root.channelIndex
            traceColor: root.traceColor
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
        }

        RmsWaveformItem {
            anchors.fill: parent
            visible: root.displayMode === "rms"
            document: root.document
            channelIndex: root.channelIndex
            traceColor: root.traceColor
            zoomFactor: root.zoomFactor
            panFraction: root.panFraction
        }

        TriggerReference {
            visible: root.document && root.visibleDuration > 0
                     && root.document.triggerOffsetSeconds >= root.viewStart
                     && root.document.triggerOffsetSeconds <= root.viewStart + root.visibleDuration
            x: (root.document.triggerOffsetSeconds - root.viewStart) / root.visibleDuration * chart.width
            height: chart.height
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorATime >= root.viewStart
                     && root.cursorATime <= root.viewStart + root.visibleDuration
            x: (root.cursorATime - root.viewStart) / root.visibleDuration * chart.width
            y: 0
            width: 1
            height: chart.height
            color: root.cursorAColor
            opacity: 0.95
        }

        Rectangle {
            visible: root.visibleDuration > 0 && root.cursorBTime >= root.viewStart
                     && root.cursorBTime <= root.viewStart + root.visibleDuration
            x: (root.cursorBTime - root.viewStart) / root.visibleDuration * chart.width
            y: 0
            width: 1
            height: chart.height
            color: root.cursorBColor
            opacity: 0.95
        }
    }
}
