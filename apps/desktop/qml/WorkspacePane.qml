// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"
    border.width: root.activePane ? 2 : 1
    border.color: root.activePane ? "#3b6f9c" : "#bcc3c8"
    clip: true

    property int paneIndex: 0
    property bool activePane: false
    property bool hasRecord: false
    property string viewMode: "time"
    property var document
    property var analysis
    property var locusAnalysis
    property var harmonicSnapshot
    property var tableSnapshot
    property var voltageChannels: []
    property var currentChannels: []
    property var otherChannels: []
    property var displayedDigitalChannels: []
    property string digitalDisplayMode: "active"
    property string displayMode: "instantaneous"
    property string valueRepresentation: "secondary"
    property real zoomFactor: 1.0
    property real panFraction: 0.0
    property real viewStart: 0.0
    property real visibleDuration: 0.0
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property real axisWidth: 170
    property real analogTrackHeight: 148
    property real digitalTrackHeight: 28
    property var phasorVoltageChannels: []
    property var phasorCurrentChannels: []
    property var residualChannels: []
    property var harmonicChannels: []
    property var tableChannels: []
    property int selectedChannel: -1

    signal activated()
    signal viewModeRequested(string viewName)
    signal cursorARequested(real timeSeconds)
    signal cursorBRequested(real timeSeconds)
    signal panRequested(real panFraction)
    signal zoomRequested(real factor, real anchorFraction)
    signal digitalDisplayModeRequested(string mode)
    signal signalActivated(int channelIndex)

    readonly property var viewIds: ["time", "phasor", "locus", "harmonics", "table"]
    readonly property var viewLabels: ["Time Signals", "Phasor / Vector", "R-X Locus", "Harmonics", "Engineering Table"]
    readonly property real paneAxisWidth: Math.max(92, Math.min(root.axisWidth, root.width * 0.24))
    readonly property real paneAnalogTrackHeight: Math.max(104, Math.min(root.analogTrackHeight, root.height * 0.34))

    function resetForRecord() {
        engineeringViews.resetForRecord()
        resetTimeScroll()
    }

    function resetTimeScroll() {
        if (timeLoader.item && timeLoader.item.resetScroll) timeLoader.item.resetScroll()
    }

    Rectangle {
        id: paneHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 32
        color: root.activePane ? "#e6eef5" : "#eef1f3"
        border.color: root.activePane ? "#8ba8bf" : "#c8cdd1"
        z: 20

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            spacing: 7

            ToolButton {
                text: "P" + String(root.paneIndex + 1)
                checkable: true
                checked: root.activePane
                Layout.preferredWidth: 34
                Layout.preferredHeight: 26
                font.pixelSize: 10
                font.weight: Font.DemiBold
                onClicked: root.activated()
                ToolTip.visible: hovered
                ToolTip.text: root.activePane ? "Active workspace pane" : "Make this the active workspace pane"
            }

            ComboBox {
                id: viewSelector
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                model: root.viewLabels
                font.pixelSize: 10
                currentIndex: Math.max(0, root.viewIds.indexOf(root.viewMode))
                onActivated: index => {
                    root.activated()
                    root.viewModeRequested(root.viewIds[index])
                }
            }

            Label {
                text: root.valueRepresentation === "primary" ? "PRI" : "SEC"
                color: "#5c6870"
                font.pixelSize: 8
                font.weight: Font.DemiBold
                visible: root.width > 420
            }
        }
    }

    Item {
        id: paneBody
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: paneHeader.bottom
        anchors.bottom: parent.bottom
        clip: true

        Loader {
            id: timeLoader
            anchors.fill: parent
            asynchronous: true
            active: root.visible && root.hasRecord && root.viewMode === "time"
            sourceComponent: Component {
                TimeSignalsView {
                    document: root.document
                    analysis: root.analysis
                    voltageChannels: root.voltageChannels
                    currentChannels: root.currentChannels
                    otherChannels: root.otherChannels
                    displayedDigitalChannels: root.displayedDigitalChannels
                    digitalDisplayMode: root.digitalDisplayMode
                    displayMode: root.displayMode
                    valueRepresentation: root.valueRepresentation
                    zoomFactor: root.zoomFactor
                    panFraction: root.panFraction
                    viewStart: root.viewStart
                    visibleDuration: root.visibleDuration
                    cursorATime: root.cursorATime
                    cursorBTime: root.cursorBTime
                    axisWidth: root.paneAxisWidth
                    analogTrackHeight: root.paneAnalogTrackHeight
                    digitalTrackHeight: root.digitalTrackHeight
                    onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)
                    onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)
                    onPanRequested: value => root.panRequested(value)
                    onZoomRequested: (factor, anchorFraction) => root.zoomRequested(factor, anchorFraction)
                    onDigitalDisplayModeRequested: mode => root.digitalDisplayModeRequested(mode)
                }
            }
        }

        DeferredEngineeringViews {
            id: engineeringViews
            anchors.fill: parent
            hasRecord: root.visible && root.hasRecord
            viewMode: root.viewMode
            document: root.document
            analysis: root.analysis
            locusAnalysis: root.locusAnalysis
            harmonicSnapshot: root.harmonicSnapshot
            tableSnapshot: root.tableSnapshot
            viewStart: root.viewStart
            visibleDuration: root.visibleDuration
            cursorATime: root.cursorATime
            cursorBTime: root.cursorBTime
            voltageChannels: root.phasorVoltageChannels
            currentChannels: root.phasorCurrentChannels
            residualChannels: root.residualChannels
            harmonicChannels: root.harmonicChannels
            tableChannels: root.tableChannels
            selectedChannel: root.selectedChannel
            valueRepresentation: root.valueRepresentation
            onSignalActivated: channelIndex => root.signalActivated(channelIndex)
        }

        Rectangle {
            anchors.fill: parent
            visible: root.hasRecord && timeLoader.active && timeLoader.status === Loader.Loading
            color: "#ffffff"
            opacity: 0.92
            z: 30
            Row {
                anchors.centerIn: parent
                spacing: 10
                BusyIndicator { width: 22; height: 22; running: parent.parent.visible }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Preparing Time Signals…"
                    color: "#56616a"
                    font.pixelSize: 10
                }
            }
        }
    }
}
