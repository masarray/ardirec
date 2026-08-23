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

    readonly property int signalWidth: 178
    readonly property int phaseWidth: 50
    readonly property int instantWidth: 106
    readonly property int rmsWidth: 104
    readonly property int h1Width: 104
    readonly property int angleWidth: 78
    readonly property int extremumWidth: 108
    readonly property int dcWidth: 98
    readonly property int percentWidth: 74
    readonly property int tableWidth: signalWidth + phaseWidth + instantWidth + rmsWidth + h1Width
                                      + angleWidth + extremumWidth + dcWidth + percentWidth * 4 + 18

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
                                                   ? snapshot.sortedChannels(scopedChannels, cursorTime, sortMode)
                                                   : scopedChannels
    readonly property var summaryData: snapshot
                                           ? snapshot.summaryAt(scopedChannels, cursorTime)
                                           : ({count: 0, maxThdChannel: -1, maxThd: 0, maxRmsChannel: -1, maxRms: 0})

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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#e7eaed"
            border.color: "#c3c8cd"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                Label {
                    text: "ENGINEERING SNAPSHOT"
                    color: "#343c43"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }
                Label {
                    text: root.scopeLabel() + " · " + root.displayedChannels.length + " signals"
                    color: "#6d757b"
                    font.pixelSize: 8
                }

                Rectangle { width: 1; height: 20; color: "#c0c5c9"; Layout.leftMargin: 4; Layout.rightMargin: 3 }
                Label { text: "SCOPE"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Repeater {
                    model: [
                        {key:"electrical", label:"Electrical"},
                        {key:"voltage", label:"Voltage"},
                        {key:"current", label:"Current"},
                        {key:"visible", label:"Visible"},
                        {key:"all", label:"All"}
                    ]
                    ToolButton {
                        required property var modelData
                        text: modelData.label
                        checkable: true
                        checked: root.scopeMode === modelData.key
                        font.pixelSize: 8
                        onClicked: root.scopeMode = modelData.key
                    }
                }

                Rectangle { width: 1; height: 20; color: "#c0c5c9"; Layout.leftMargin: 3; Layout.rightMargin: 3 }
                Label { text: "SORT"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Repeater {
                    model: [
                        {key:"record", label:"Record"},
                        {key:"signal", label:"Signal"},
                        {key:"rms", label:"RMS ↓"},
                        {key:"thd", label:"THD ↓"}
                    ]
                    ToolButton {
                        required property var modelData
                        text: modelData.label
                        checkable: true
                        checked: root.sortMode === modelData.key
                        font.pixelSize: 8
                        onClicked: root.sortMode = modelData.key
                    }
                }

                Item { Layout.fillWidth: true }
                Label {
                    text: (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY") + " · 1-cycle snapshot"
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
                spacing: 12

                Label {
                    text: "Cursor  " + root.relativeMs().toFixed(3) + " ms"
                    color: "#244f9e"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
                Rectangle { width: 1; height: 15; color: "#d0d4d7" }
                Label {
                    text: "Highest THD  "
                          + (root.summaryData.maxThdChannel >= 0 && root.document
                             ? root.document.channelName(root.summaryData.maxThdChannel) + "  " + root.summaryData.maxThd.toFixed(2) + "%"
                             : "—")
                    color: root.summaryData.maxThd >= 5.0 ? "#9a6500" : "#566069"
                    font.pixelSize: 8
                    font.weight: root.summaryData.maxThd >= 5.0 ? Font.DemiBold : Font.Normal
                }
                Rectangle { width: 1; height: 15; color: "#d0d4d7" }
                Label {
                    text: "Highest RMS  "
                          + (root.summaryData.maxRmsChannel >= 0 && root.document
                             ? root.document.channelName(root.summaryData.maxRmsChannel) + "  "
                               + root.formatValue(root.summaryData.maxRmsChannel, root.summaryData.maxRms)
                             : "—")
                    color: "#566069"
                    font.pixelSize: 8
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: "Extremum = signed max |sample| in trailing cycle · H2/H3/H5 = %H1"
                    color: "#747c82"
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
                    height: 30
                    color: "#dde2e6"
                    border.color: "#bcc4ca"

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        spacing: 0

                        Label { width: root.signalWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Signal"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.phaseWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Phase"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.instantWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Instant"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.rmsWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "RMS"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.h1Width; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "H1 RMS"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.angleWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "H1 ∠"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.extremumWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Cycle extremum"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.dcWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "DC"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
                        Label { width: root.percentWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "THD"; color: "#424b52"; font.pixelSize: 8; font.weight: Font.DemiBold }
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
                        color: index % 2 ? "#fafbfc" : "#ffffff"
                        border.color: "#e1e4e7"

                        readonly property var rowSnapshot: {
                            const representationDependency = root.valueRepresentation
                            return root.snapshot ? root.snapshot.snapshotAt(modelData, root.cursorTime) : ({valid:false})
                        }
                        readonly property color phaseColor: root.analysis ? root.analysis.phaseColor(modelData) : "#6f7780"

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
                            Label { width: root.instantWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.instant) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.rmsWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.rms) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.h1Width; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.fundamental) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.angleWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? rowSnapshot.angle.toFixed(1) + "°" : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.extremumWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.extremum) : "—"; color: "#343b40"; font.pixelSize: 8 }
                            Label { width: root.dcWidth; height: parent.height; verticalAlignment: Text.AlignVCenter; text: rowSnapshot.valid ? root.formatValue(modelData, rowSnapshot.dc) : "—"; color: Math.abs(rowSnapshot.dc || 0) > Math.max(1e-12, Math.abs(rowSnapshot.fundamental || 0) * 0.05) ? "#8a5b00" : "#343b40"; font.pixelSize: 8 }

                            Rectangle {
                                width: root.percentWidth; height: parent.height
                                color: rowSnapshot.valid && rowSnapshot.thd >= 5.0 ? "#fff2cf" : "transparent"
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
                                         + "\nCursor " + root.relativeMs().toFixed(3) + " ms · THD " + rowSnapshot.thd.toFixed(2) + "%")
                                      : "No valid one-cycle snapshot"
                        MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                    }
                }
            }
        }
    }
}
