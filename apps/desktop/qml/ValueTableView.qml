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
    property string scopeMode: "visible"
    property string sortMode: "record"
    property string columnMode: "analysis"
    property bool abnormalOnly: false
    property int selectedChannel: document ? document.selectedAnalogIndex : -1

    signal signalActivated(int channelIndex)

    readonly property int signalWidth: 190
    readonly property int phaseWidth: 68
    readonly property int h1Width: 126
    readonly property int angleWidth: 92
    readonly property int extremumWidth: 132
    readonly property int instantWidth: 120
    readonly property int rmsWidth: 120
    readonly property int crestWidth: 84
    readonly property int dcAbsWidth: 116
    readonly property int percentWidth: 90
    readonly property bool detailed: columnMode === "detailed"
    readonly property int tableWidth: signalWidth + phaseWidth + h1Width + angleWidth + extremumWidth
                                      + percentWidth * 5 + 24
                                      + (detailed ? instantWidth + rmsWidth + crestWidth + dcAbsWidth : 0)

    readonly property var scopedChannels: {
        if (!document) return []
        if (scopeMode === "visible") return visibleChannels ? visibleChannels.slice() : []
        let result = []
        for (let i = 0; i < document.analogCount; ++i) {
            const role = document.analogRole(i)
            if (scopeMode === "all"
                    || (scopeMode === "electrical" && (role === "Voltage" || role === "Current"))
                    || (scopeMode === "voltage" && role === "Voltage")
                    || (scopeMode === "current" && role === "Current")) result.push(i)
        }
        return result
    }

    readonly property var displayedChannels: snapshot
        ? snapshot.sortedChannels(scopedChannels, cursorTime, sortMode, abnormalOnly)
        : scopedChannels

    readonly property var summaryData: snapshot
        ? snapshot.summaryAt(scopedChannels, cursorTime)
        : ({count:0, abnormalCount:0, maxThdChannel:-1, maxThd:0,
            maxDcChannel:-1, maxDcPercent:0, maxCrestChannel:-1, maxCrestFactor:0,
            maxVoltageRmsChannel:-1, maxVoltageRms:0,
            maxCurrentRmsChannel:-1, maxCurrentRms:0,
            maxHarmonicOrder:0, sampleRate:0})

    function formatValue(channelIndex, value) {
        return document && Number.isFinite(value) ? document.formatChannelValue(channelIndex, value) : "—"
    }
    function percentDecimals(value) {
        if (root.detailed) return 2
        return Math.abs(value - 5.0) < 0.1 ? 2 : 1
    }
    function formatPercentNumber(value) { return Number.isFinite(value) ? value.toFixed(root.percentDecimals(value)) : "—" }
    function formatPercent(value) {
        const number = root.formatPercentNumber(value)
        return number === "—" ? number : number + "%"
    }
    function relativeMs() { return document ? (cursorTime - document.triggerOffsetSeconds) * 1000.0 : 0 }
    function scopeLabel() {
        if (scopeMode === "voltage") return "Voltage"
        if (scopeMode === "current") return "Current"
        if (scopeMode === "visible") return "Configured"
        if (scopeMode === "all") return "All analog"
        return "Electrical"
    }
    function channelSummary(channelIndex, value, suffix) {
        if (channelIndex < 0 || !document) return "—"
        return document.channelName(channelIndex) + "  " + value + suffix
    }

    component HeaderCell: Label {
        height: 44
        verticalAlignment: Text.AlignVCenter
        color: "#344049"
        font.pixelSize: 11
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    component DataCell: Label {
        height: 44
        verticalAlignment: Text.AlignVCenter
        color: "#273139"
        font.pixelSize: 11
        elide: Text.ElideRight
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 78
            color: "#e7eaed"
            border.color: "#bec5ca"

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ColumnLayout {
                        spacing: 0
                        Label { text: "ENGINEERING TABLE"; color: "#29333a"; font.pixelSize: 13; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
                        Label {
                            text: root.scopeLabel() + " · " + root.displayedChannels.length
                                  + (root.abnormalOnly ? " abnormal signals" : " signals")
                            color: "#657078"; font.pixelSize: 10
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
                              + " · C1 " + root.relativeMs().toFixed(3) + " ms"
                        color: "#4d5c66"; font.pixelSize: 11; font.weight: Font.DemiBold
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7
                    ToolButton {
                        text: "Analysis"; checkable: true; checked: root.columnMode === "analysis"
                        font.pixelSize: 11; Layout.preferredHeight: 32
                        onClicked: root.columnMode = "analysis"
                    }
                    ToolButton {
                        text: "Detailed"; checkable: true; checked: root.columnMode === "detailed"
                        font.pixelSize: 11; Layout.preferredHeight: 32
                        onClicked: root.columnMode = "detailed"
                    }
                    Rectangle { width: 1; height: 24; color: "#c0c6ca"; Layout.leftMargin: 2; Layout.rightMargin: 2 }
                    Label { text: "Scope"; color: "#56626b"; font.pixelSize: 10; font.weight: Font.DemiBold }
                    ComboBox {
                        Layout.preferredWidth: 124; Layout.preferredHeight: 32; font.pixelSize: 11
                        model: ["Configured", "Electrical", "Voltage", "Current", "All analog"]
                        currentIndex: ["visible", "electrical", "voltage", "current", "all"].indexOf(root.scopeMode)
                        onActivated: root.scopeMode = ["visible", "electrical", "voltage", "current", "all"][currentIndex]
                    }
                    Label { text: "Sort"; color: "#56626b"; font.pixelSize: 10; font.weight: Font.DemiBold; Layout.leftMargin: 3 }
                    ComboBox {
                        Layout.preferredWidth: 112; Layout.preferredHeight: 32; font.pixelSize: 11
                        model: ["Record", "Signal", "RMS ↓", "THD ↓", "DC ↓", "Crest ↓"]
                        currentIndex: ["record", "signal", "rms", "thd", "dc", "crest"].indexOf(root.sortMode)
                        onActivated: root.sortMode = ["record", "signal", "rms", "thd", "dc", "crest"][currentIndex]
                    }
                    ToolButton {
                        text: "Abnormal only"; checkable: true; checked: root.abnormalOnly
                        font.pixelSize: 11; Layout.preferredHeight: 32
                        onClicked: root.abnormalOnly = !root.abnormalOnly
                        ToolTip.visible: hovered
                        ToolTip.text: "Investigation heuristic: THD ≥ 5%, |DC|/H1 ≥ 5%, or crest factor ≥ 2.0"
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "#f8f9fa"
            border.color: "#d0d5d9"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10
                Label {
                    text: "THD H2–H" + root.summaryData.maxHarmonicOrder
                          + " · " + (root.summaryData.sampleRate / 1000.0).toFixed(2) + " kHz"
                    color: "#4f5b64"; font.pixelSize: 11; font.weight: Font.DemiBold
                }
                Rectangle { width: 1; height: 20; color: "#d0d5d9" }
                Label {
                    text: "Worst THD  " + root.channelSummary(root.summaryData.maxThdChannel, root.formatPercentNumber(root.summaryData.maxThd), "%")
                    color: root.summaryData.maxThd >= 5.0 ? "#8a5b00" : "#566069"; font.pixelSize: 11; font.weight: root.summaryData.maxThd >= 5.0 ? Font.DemiBold : Font.Normal
                }
                Rectangle { width: 1; height: 20; color: "#d0d5d9" }
                Label {
                    text: "Highest DC/H1  " + root.channelSummary(root.summaryData.maxDcChannel, root.formatPercentNumber(root.summaryData.maxDcPercent), "%")
                    color: root.summaryData.maxDcPercent >= 5.0 ? "#8a5b00" : "#566069"; font.pixelSize: 11; font.weight: root.summaryData.maxDcPercent >= 5.0 ? Font.DemiBold : Font.Normal
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: root.width > 1160 && root.summaryData.maxVoltageRmsChannel >= 0
                    text: "Max V  " + root.document.channelName(root.summaryData.maxVoltageRmsChannel) + "  "
                          + root.formatValue(root.summaryData.maxVoltageRmsChannel, root.summaryData.maxVoltageRms)
                    color: "#657078"; font.pixelSize: 10
                }
                Label {
                    visible: root.width > 1380 && root.summaryData.maxCurrentRmsChannel >= 0
                    text: "Max I  " + root.document.channelName(root.summaryData.maxCurrentRmsChannel) + "  "
                          + root.formatValue(root.summaryData.maxCurrentRmsChannel, root.summaryData.maxCurrentRms)
                    color: "#657078"; font.pixelSize: 10
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
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded; height: 13 }

            Item {
                width: horizontalPan.contentWidth
                height: horizontalPan.height

                Rectangle {
                    id: header
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 44
                    color: "#dce2e6"
                    border.color: "#b9c2c8"
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        spacing: 0
                        HeaderCell { width: root.signalWidth; text: "Signal" }
                        HeaderCell { width: root.phaseWidth; text: "Phase" }
                        HeaderCell { width: root.h1Width; text: "H1 RMS" }
                        HeaderCell { width: root.angleWidth; text: "Phase ∠" }
                        HeaderCell { width: root.extremumWidth; text: "Last extremum" }
                        HeaderCell { visible: root.detailed; width: visible ? root.instantWidth : 0; text: "Instant" }
                        HeaderCell { visible: root.detailed; width: visible ? root.rmsWidth : 0; text: "True RMS" }
                        HeaderCell { visible: root.detailed; width: visible ? root.crestWidth : 0; text: "Crest" }
                        HeaderCell { visible: root.detailed; width: visible ? root.dcAbsWidth : 0; text: "DC signed" }
                        HeaderCell { width: root.percentWidth; text: "DC/H1" }
                        HeaderCell { width: root.percentWidth; text: "THD" }
                        HeaderCell { width: root.percentWidth; text: "H2/H1" }
                        HeaderCell { width: root.percentWidth; text: "H3/H1" }
                        HeaderCell { width: root.percentWidth; text: "H5/H1" }
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
                    cacheBuffer: height * 0.7
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 13 }

                    delegate: Rectangle {
                        required property int index
                        required property int modelData
                        width: tableRows.width
                        height: 46
                        readonly property var rowSnapshot: {
                            const representationDependency = root.valueRepresentation
                            return root.snapshot ? root.snapshot.snapshotAt(modelData, root.cursorTime) : ({valid:false})
                        }
                        readonly property color phaseColor: root.analysis ? root.analysis.phaseColor(modelData) : "#6f7780"
                        readonly property bool selected: root.selectedChannel === modelData
                        readonly property bool abnormal: rowSnapshot.valid && rowSnapshot.abnormal
                        color: selected ? "#e5f0f8" : rowMouse.containsMouse ? "#f0f5f8" : (index % 2 ? "#fafbfc" : "#ffffff")
                        border.color: selected ? "#9ebcd2" : "#dfe4e7"

                        Rectangle { width: 4; height: parent.height; color: parent.phaseColor }
                        Rectangle { visible: parent.abnormal; x: 4; width: 3; height: parent.height; color: "#d6a13d"; opacity: 0.75 }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 9
                            spacing: 0
                            DataCell { width: root.signalWidth; text: root.document ? root.document.channelName(modelData) : "—"; font.weight: Font.DemiBold }
                            DataCell { width: root.phaseWidth; text: rowSnapshot.phase || "—"; color: phaseColor; font.weight: Font.DemiBold }
                            DataCell { width: root.h1Width; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.fundamental) : "—" }
                            DataCell { width: root.angleWidth; text: rowSnapshot.valid ? rowSnapshot.angle.toFixed(1) + "°" : "—" }
                            DataCell { width: root.extremumWidth; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.extremum) : "—" }
                            DataCell { visible: root.detailed; width: visible ? root.instantWidth : 0; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.instant) : "—" }
                            DataCell { visible: root.detailed; width: visible ? root.rmsWidth : 0; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.rms) : "—" }
                            DataCell { visible: root.detailed; width: visible ? root.crestWidth : 0; text: rowSnapshot.valid ? rowSnapshot.crestFactor.toFixed(2) : "—"; color: rowSnapshot.valid && rowSnapshot.crestFactor >= 2.0 ? "#8a5b00" : "#273139" }
                            DataCell { visible: root.detailed; width: visible ? root.dcAbsWidth : 0; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.dc) : "—"; color: "#536069" }

                            Rectangle {
                                width: root.percentWidth; height: parent.height; color: "transparent"
                                Rectangle { visible: rowSnapshot.valid && rowSnapshot.dcPercent >= 5.0; width: 3; height: parent.height - 10; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; color: "#d6a13d" }
                                DataCell { anchors.fill: parent; anchors.leftMargin: 7; text: rowSnapshot.valid ? root.formatPercent(rowSnapshot.dcPercent) : "—"; color: rowSnapshot.valid && rowSnapshot.dcPercent >= 5.0 ? "#805600" : "#273139"; font.weight: rowSnapshot.valid && rowSnapshot.dcPercent >= 5.0 ? Font.DemiBold : Font.Normal }
                            }
                            Rectangle {
                                width: root.percentWidth; height: parent.height; color: "transparent"
                                Rectangle { visible: rowSnapshot.valid && rowSnapshot.thd >= 5.0; width: 3; height: parent.height - 10; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; color: "#d6a13d" }
                                DataCell { anchors.fill: parent; anchors.leftMargin: 7; text: rowSnapshot.valid ? root.formatPercent(rowSnapshot.thd) : "—"; color: rowSnapshot.valid && rowSnapshot.thd >= 5.0 ? "#805600" : "#273139"; font.weight: rowSnapshot.valid && rowSnapshot.thd >= 5.0 ? Font.DemiBold : Font.Normal }
                            }
                            DataCell { width: root.percentWidth; text: rowSnapshot.valid ? root.formatPercent(rowSnapshot.h2) : "—" }
                            DataCell { width: root.percentWidth; text: rowSnapshot.valid ? root.formatPercent(rowSnapshot.h3) : "—" }
                            DataCell { width: root.percentWidth; text: rowSnapshot.valid ? root.formatPercent(rowSnapshot.h5) : "—" }
                        }

                        ToolTip.visible: rowMouse.containsMouse
                        ToolTip.delay: 300
                        ToolTip.text: rowSnapshot.valid
                            ? (root.document.channelName(modelData) + " · " + rowSnapshot.role
                               + "\nC1 " + root.relativeMs().toFixed(3) + " ms · H1 " + root.formatValue(modelData, rowSnapshot.fundamental)
                               + " · ∠" + rowSnapshot.angle.toFixed(2) + "°"
                               + "\nDC/H1 " + rowSnapshot.dcPercent.toFixed(3) + "% · THD " + rowSnapshot.thd.toFixed(3) + "%")
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

        Label {
            visible: root.displayedChannels.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "No signals are assigned to Table. Open Signals → Signal Configuration…"
            color: "#657078"
            font.pixelSize: 13
        }
    }
}
