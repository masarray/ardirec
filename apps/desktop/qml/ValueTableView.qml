// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f4f5f6"

    property var document
    property var analysis
    property var snapshot
    property var visibleChannels: []
    property real cursorTime: 0.0
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"
    property string scopeMode: "electrical"
    property string sortMode: "record"
    property string columnMode: "analysis"
    property bool abnormalOnly: false
    property int selectedChannel: document ? document.selectedAnalogIndex : -1

    signal signalActivated(int channelIndex)

    readonly property int signalWidth: 160
    readonly property int phaseWidth: 48
    readonly property int h1Width: 98
    readonly property int angleWidth: 72
    readonly property int extremumWidth: 106
    readonly property int instantWidth: 98
    readonly property int rmsWidth: 94
    readonly property int crestWidth: 66
    readonly property int dcAbsWidth: 94
    readonly property int percentWidth: 72
    readonly property bool detailed: columnMode === "detailed"
    readonly property int tableWidth: signalWidth + phaseWidth + h1Width + angleWidth + extremumWidth
                                      + percentWidth * 5 + 20
                                      + (detailed ? instantWidth + rmsWidth + crestWidth + dcAbsWidth : 0)

    readonly property var scopedChannels: {
        if (!document) return []
        if (scopeMode === "visible") return visibleChannels.slice()
        let voltage = []
        let current = []
        let other = []
        for (let i = 0; i < document.analogCount; ++i) {
            const role = document.analogRole(i)
            if (role === "Voltage") voltage.push(i)
            else if (role === "Current") current.push(i)
            else other.push(i)
        }
        if (scopeMode === "voltage") return voltage
        if (scopeMode === "current") return current
        if (scopeMode === "all") return voltage.concat(current).concat(other)
        return voltage.concat(current)
    }

    readonly property var displayedChannels: snapshot
        ? snapshot.sortedChannels(scopedChannels, cursorTime, sortMode, abnormalOnly)
        : scopedChannels

    readonly property var summaryData: snapshot
        ? snapshot.summaryAt(scopedChannels, cursorTime)
        : ({count: 0, abnormalCount: 0, maxThdChannel: -1, maxThd: 0,
            maxDcChannel: -1, maxDcPercent: 0, maxCrestChannel: -1, maxCrestFactor: 0,
            maxVoltageRmsChannel: -1, maxVoltageRms: 0,
            maxCurrentRmsChannel: -1, maxCurrentRms: 0,
            maxHarmonicOrder: 0, sampleRate: 0})

    function formatValue(channelIndex, value) {
        return document && Number.isFinite(value) ? document.formatChannelValue(channelIndex, value) : "—"
    }
    function relativeMs() {
        return document ? (cursorTime - document.triggerOffsetSeconds) * 1000.0 : 0
    }
    function scopeLabel() {
        if (scopeMode === "voltage") return "VOLTAGE"
        if (scopeMode === "current") return "CURRENT"
        if (scopeMode === "visible") return "VISIBLE"
        if (scopeMode === "all") return "ALL ANALOG"
        return "ELECTRICAL"
    }
    function channelSummary(channelIndex, value, suffix) {
        if (channelIndex < 0 || !document) return "—"
        return document.channelName(channelIndex) + "  " + value + suffix
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#e7eaed"
            border.color: "#c3c8cd"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                Label {
                    text: "ENGINEERING TABLE"
                    color: "#343c43"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }
                Label {
                    text: root.scopeLabel() + " · " + root.displayedChannels.length
                          + (root.abnormalOnly ? " abnormal shown" : " signals")
                    color: "#6d757b"
                    font.pixelSize: 8
                }

                Rectangle { width: 1; height: 20; color: "#c0c5c9"; Layout.leftMargin: 4; Layout.rightMargin: 3 }
                Label { text: "COLUMNS"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                ToolButton {
                    text: "Analysis"
                    checkable: true
                    checked: root.columnMode === "analysis"
                    font.pixelSize: 8
                    onClicked: root.columnMode = "analysis"
                }
                ToolButton {
                    text: "Detailed"
                    checkable: true
                    checked: root.columnMode === "detailed"
                    font.pixelSize: 8
                    onClicked: root.columnMode = "detailed"
                    ToolTip.visible: hovered
                    ToolTip.text: "Adds nearest sample, true cycle RMS, crest factor and signed DC magnitude"
                }

                Rectangle { width: 1; height: 20; color: "#c0c5c9"; Layout.leftMargin: 3; Layout.rightMargin: 3 }
                Label { text: "SCOPE"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                ComboBox {
                    Layout.preferredWidth: 92
                    font.pixelSize: 8
                    model: ["Electrical", "Voltage", "Current", "Visible", "All Analog"]
                    onActivated: {
                        const keys = ["electrical", "voltage", "current", "visible", "all"]
                        root.scopeMode = keys[currentIndex]
                    }
                }

                Label { text: "SORT"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold; Layout.leftMargin: 4 }
                ComboBox {
                    Layout.preferredWidth: 86
                    font.pixelSize: 8
                    model: ["Record", "Signal", "RMS ↓", "THD ↓", "DC ↓", "Crest ↓"]
                    onActivated: {
                        const keys = ["record", "signal", "rms", "thd", "dc", "crest"]
                        root.sortMode = keys[currentIndex]
                    }
                }

                ToolButton {
                    text: "Only abnormal"
                    checkable: true
                    checked: root.abnormalOnly
                    font.pixelSize: 8
                    onClicked: root.abnormalOnly = !root.abnormalOnly
                    ToolTip.visible: hovered
                    ToolTip.text: "Investigation heuristic only: THD ≥ 5%, |DC|/H1 ≥ 5%, or crest factor ≥ 2.0"
                }

                Item { Layout.fillWidth: true }
                Label {
                    text: (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
                          + " · cursor " + root.relativeMs().toFixed(3) + " ms · trailing 1-cycle"
                    color: "#4e5961"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#f7f8f9"
            border.color: "#d0d4d7"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                spacing: 9

                Label {
                    text: "Harmonic limit  H" + root.summaryData.maxHarmonicOrder
                          + " · " + (root.summaryData.sampleRate / 1000.0).toFixed(2) + " kHz sample rate"
                    color: "#566069"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                    ToolTip.visible: limitMouse.containsMouse
                    ToolTip.text: "Nyquist-safe maximum order. Harmonics above this limit are not calculated or included in THD."
                    MouseArea { id: limitMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                }
                Rectangle { width: 1; height: 15; color: "#d0d4d7" }
                Label {
                    text: "THD  " + root.channelSummary(root.summaryData.maxThdChannel,
                                                        root.summaryData.maxThd.toFixed(2), "%")
                    color: root.summaryData.maxThd >= 5.0 ? "#8a5b00" : "#566069"
                    font.pixelSize: 8
                }
                Rectangle { width: 1; height: 15; color: "#d0d4d7" }
                Label {
                    text: "DC  " + root.channelSummary(root.summaryData.maxDcChannel,
                                                       root.summaryData.maxDcPercent.toFixed(1), "% H1")
                    color: root.summaryData.maxDcPercent >= 5.0 ? "#8a5b00" : "#566069"
                    font.pixelSize: 8
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: root.width > 1220 && root.summaryData.maxVoltageRmsChannel >= 0
                    text: "Max V RMS  " + root.document.channelName(root.summaryData.maxVoltageRmsChannel) + "  "
                          + root.formatValue(root.summaryData.maxVoltageRmsChannel, root.summaryData.maxVoltageRms)
                    color: "#657078"
                    font.pixelSize: 7
                }
                Label {
                    visible: root.width > 1420 && root.summaryData.maxCurrentRmsChannel >= 0
                    text: "Max I RMS  " + root.document.channelName(root.summaryData.maxCurrentRmsChannel) + "  "
                          + root.formatValue(root.summaryData.maxCurrentRmsChannel, root.summaryData.maxCurrentRms)
                    color: "#657078"
                    font.pixelSize: 7
                }
            }
        }

        Flickable {
            id: horizontalPan
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: Math.max(width, root.tableWidth)
            contentHeight: height
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

            Item {
                width: horizontalPan.contentWidth
                height: horizontalPan.height

                Rectangle {
                    id: header
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 32
                    color: "#dde2e6"
                    border.color: "#bcc4ca"

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        spacing: 0

                        Label { width: root.signalWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Signal"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.phaseWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Phase"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.h1Width; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "H1 RMS"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.angleWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Phase ∠"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.extremumWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Last extremum"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { visible: root.detailed; width: visible ? root.instantWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Instant"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { visible: root.detailed; width: visible ? root.rmsWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "True RMS"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { visible: root.detailed; width: visible ? root.crestWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Crest"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { visible: root.detailed; width: visible ? root.dcAbsWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "DC abs"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "DC %"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "THD ≤H" + root.summaryData.maxHarmonicOrder; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "H2/H1"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "H3/H1"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "H5/H1"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                    }
                }

                ListView {
                    id: tableRows
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: header.bottom
                    anchors.bottom: parent.bottom
                    clip: true
                    model: root.displayedChannels
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 10 }

                    delegate: Rectangle {
                        required property int index
                        required property int modelData
                        width: tableRows.width
                        height: 30
                        readonly property var rowSnapshot: {
                            const representationDependency = root.valueRepresentation
                            return root.snapshot ? root.snapshot.snapshotAt(modelData, root.cursorTime) : ({valid:false})
                        }
                        readonly property color phaseColor: root.analysis ? root.analysis.phaseColor(modelData) : "#6f7780"
                        readonly property bool selected: root.selectedChannel === modelData
                        readonly property bool abnormal: rowSnapshot.valid && rowSnapshot.abnormal
                        color: selected ? "#e8f0f8"
                                        : rowMouse.containsMouse ? "#f1f5f8"
                                        : abnormal ? "#fffaf0"
                                        : (index % 2 ? "#fafbfc" : "#ffffff")
                        border.color: selected ? "#a7bfd5" : "#e1e4e7"

                        Rectangle { width: 3; height: parent.height; color: parent.phaseColor }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            spacing: 0

                            Label {
                                width: root.signalWidth; height: parent.height; verticalAlignment: Text.AlignVCenter
                                text: root.document ? root.document.channelName(modelData) : "—"
                                color: "#2f363b"; font.pixelSize: 8; font.weight: Font.DemiBold; elide: Text.ElideRight
                            }
                            Label { width: root.phaseWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.phase || "—"; color: phaseColor; font.pixelSize: 8; font.weight: Font.DemiBold }
                            Label { width: root.h1Width; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.fundamental) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.angleWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.angle.toFixed(1) + "°" : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.extremumWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.extremum) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { visible: root.detailed; width: visible ? root.instantWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.instant) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { visible: root.detailed; width: visible ? root.rmsWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.rms) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { visible: root.detailed; width: visible ? root.crestWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.crestFactor.toFixed(2) : "—"; color: rowSnapshot.valid && rowSnapshot.crestFactor >= 2.0 ? "#8a5b00" : "#343b40"; font.pixelSize: 8 }
                            Label { visible: root.detailed; width: visible ? root.dcAbsWidth : 0; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.dc) : "—"; color: "#596168"; font.pixelSize: 8 }

                            Rectangle {
                                width: root.percentWidth; height: parent.height
                                color: rowSnapshot.valid && rowSnapshot.dcPercent >= 5.0 ? "#fff0cc" : "transparent"
                                Label { anchors.fill: parent; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.dcPercent.toFixed(1) + "%" : "—"; color: rowSnapshot.valid && rowSnapshot.dcPercent >= 5.0 ? "#8a5b00" : "#343b40"; font.pixelSize: 8; font.weight: rowSnapshot.valid && rowSnapshot.dcPercent >= 5.0 ? Font.DemiBold : Font.Normal }
                            }
                            Rectangle {
                                width: root.percentWidth; height: parent.height
                                color: rowSnapshot.valid && rowSnapshot.thd >= 5.0 ? "#fff0cc" : "transparent"
                                Label { anchors.fill: parent; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.thd.toFixed(2) + "%" : "—"; color: rowSnapshot.valid && rowSnapshot.thd >= 5.0 ? "#8a5b00" : "#343b40"; font.pixelSize: 8; font.weight: rowSnapshot.valid && rowSnapshot.thd >= 5.0 ? Font.DemiBold : Font.Normal }
                            }
                            Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.h2.toFixed(2) + "%" : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.h3.toFixed(2) + "%" : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.h5.toFixed(2) + "%" : "—"; color: "#343b40"; font.pixelSize: 8 }
                        }

                        ToolTip.visible: rowMouse.containsMouse
                        ToolTip.text: rowSnapshot.valid
                            ? (root.document.channelName(modelData) + " · " + rowSnapshot.role + " · "
                               + (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
                               + "\nCursor " + root.relativeMs().toFixed(3) + " ms"
                               + " · H1 " + root.formatValue(modelData, rowSnapshot.fundamental)
                               + " · ∠ " + rowSnapshot.angle.toFixed(1) + "°"
                               + " · DC " + rowSnapshot.dcPercent.toFixed(1) + "% H1"
                               + " · THD≤H" + rowSnapshot.maxHarmonicOrder + " " + rowSnapshot.thd.toFixed(2) + "%")
                            : "No valid trailing-cycle snapshot"

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            onClicked: {
                                if (root.document) root.document.selectChannel(modelData)
                                root.signalActivated(modelData)
                            }
                        }
                    }
                }
            }
        }
    }
}
