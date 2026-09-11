// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f4f5f6"
    border.color: "#b8bdc2"

    property var document
    property var analysis
    property var channels: []
    property int analogCount: 0
    property int maximumTracks: 24
    property var visibleChannels: []
    property var phasorVoltageChannels: []
    property var phasorCurrentChannels: []
    property var phasorResidualChannels: []
    property var reportChannels: []

    signal configurationApplied(var timeChannels,
                                var voltageChannels,
                                var currentChannels,
                                var residualChannels,
                                var reportChannels)
    signal closeRequested()

    function contains(list, value) {
        return list && list.indexOf(value) >= 0
    }

    function channelPhase(index) {
        return root.analysis ? root.analysis.channelPhase(index) : "Other"
    }

    function channelRole(index) {
        return root.document ? root.document.analogRole(index) : "Other"
    }

    function looksResidual(index) {
        if (!root.document) return false
        const name = root.document.channelName(index).trim().toUpperCase().replace(/[^A-Z0-9]/g, "")
        const phase = root.channelPhase(index)
        return phase === "E" || name.indexOf("3I0") >= 0 || name.indexOf("3U0") >= 0
                || name.indexOf("3V0") >= 0 || name === "I0" || name === "U0" || name === "V0"
                || name.endsWith("IN") || name.endsWith("UN") || name.endsWith("VN")
    }

    function reloadConfiguration() {
        rows.clear()
        if (!root.document) return
        for (let i = 0; i < root.analogCount; ++i) {
            rows.append({
                channelIndex: i,
                signalName: root.document.channelName(i),
                unitName: root.document.channelUnit(i),
                roleName: root.channelRole(i),
                phaseName: root.channelPhase(i),
                timeChecked: root.contains(root.visibleChannels, i),
                voltageChecked: root.contains(root.phasorVoltageChannels, i),
                currentChecked: root.contains(root.phasorCurrentChannels, i),
                residualChecked: root.contains(root.phasorResidualChannels, i),
                reportChecked: root.contains(root.reportChannels, i)
            })
        }
    }

    function autoAssign() {
        let timeCount = 0
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.get(i)
            const role = row.roleName
            const residual = root.looksResidual(row.channelIndex)
            const useTime = (role === "Voltage" || role === "Current") && timeCount < root.maximumTracks
            if (useTime) ++timeCount
            rows.setProperty(i, "timeChecked", useTime)
            rows.setProperty(i, "voltageChecked", role === "Voltage")
            rows.setProperty(i, "currentChecked", role === "Current")
            rows.setProperty(i, "residualChecked", residual)
            rows.setProperty(i, "reportChecked", useTime)
        }
    }

    function clearAssignments() {
        for (let i = 0; i < rows.count; ++i) {
            rows.setProperty(i, "timeChecked", false)
            rows.setProperty(i, "voltageChecked", false)
            rows.setProperty(i, "currentChecked", false)
            rows.setProperty(i, "residualChecked", false)
            rows.setProperty(i, "reportChecked", false)
        }
    }

    function collect(propertyName, limit) {
        let result = []
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.get(i)
            if (row[propertyName]) {
                if (limit > 0 && result.length >= limit) break
                result.push(row.channelIndex)
            }
        }
        return result
    }

    function applyConfiguration() {
        root.configurationApplied(root.collect("timeChecked", root.maximumTracks),
                                  root.collect("voltageChecked", 0),
                                  root.collect("currentChecked", 0),
                                  root.collect("residualChecked", 0),
                                  root.collect("reportChecked", 0))
    }

    ListModel { id: rows }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#e9ecef"
            border.color: "#c3c8cd"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 10

                ColumnLayout {
                    spacing: 1
                    Label {
                        text: "Signal Configuration"
                        color: "#20272d"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "Assign each recorded signal to Time, Phasor and Report workspaces"
                        color: "#69727a"
                        font.pixelSize: 9
                    }
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    text: "×"
                    font.pixelSize: 20
                    Layout.preferredWidth: 36
                    onClicked: root.closeRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Close signal configuration"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "#f8f9fa"
            border.color: "#d5d9dd"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                TextField {
                    id: filter
                    Layout.preferredWidth: 280
                    Layout.preferredHeight: 30
                    placeholderText: "Filter signal, unit, role or phase"
                    selectByMouse: true
                    font.pixelSize: 10
                }
                ToolButton {
                    text: "Auto Assign"
                    font.pixelSize: 9
                    onClicked: root.autoAssign()
                    ToolTip.visible: hovered
                    ToolTip.text: "Infer Time/Voltage/Current/Residual assignments from COMTRADE metadata and signal names"
                }
                ToolButton {
                    text: "Reload"
                    font.pixelSize: 9
                    onClicked: root.reloadConfiguration()
                    ToolTip.visible: hovered
                    ToolTip.text: "Discard edits and reload the active workspace assignment"
                }
                ToolButton {
                    text: "Clear"
                    font.pixelSize: 9
                    onClicked: root.clearAssignments()
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.collect("timeChecked", 0).length + " Time · "
                          + root.collect("voltageChecked", 0).length + " V · "
                          + root.collect("currentChecked", 0).length + " I"
                    color: "#5d666e"
                    font.pixelSize: 9
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#eef0f2"
            border.color: "#cdd1d5"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 0
                Label { Layout.fillWidth: true; text: "SIGNAL"; color: "#566069"; font.pixelSize: 8; font.weight: Font.DemiBold; font.letterSpacing: 0.7 }
                Label { Layout.preferredWidth: 78; text: "TIME"; horizontalAlignment: Text.AlignHCenter; color: "#566069"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 92; text: "PHASOR V"; horizontalAlignment: Text.AlignHCenter; color: "#566069"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 92; text: "PHASOR I"; horizontalAlignment: Text.AlignHCenter; color: "#566069"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 92; text: "RESIDUAL"; horizontalAlignment: Text.AlignHCenter; color: "#566069"; font.pixelSize: 8; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 78; text: "REPORT"; horizontalAlignment: Text.AlignHCenter; color: "#566069"; font.pixelSize: 8; font.weight: Font.DemiBold }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: rows
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: row
                required property int index
                required property int channelIndex
                required property string signalName
                required property string unitName
                required property string roleName
                required property string phaseName
                required property bool timeChecked
                required property bool voltageChecked
                required property bool currentChecked
                required property bool residualChecked
                required property bool reportChecked

                width: ListView.view.width
                height: rowVisible ? 42 : 0
                visible: rowVisible
                color: mouse.containsMouse ? "#eef5fd" : (index % 2 === 0 ? "#ffffff" : "#fafbfc")
                border.color: "#e3e6e8"
                property bool rowVisible: {
                    const needle = filter.text.trim().toLowerCase()
                    if (!needle.length) return true
                    return signalName.toLowerCase().includes(needle)
                            || unitName.toLowerCase().includes(needle)
                            || roleName.toLowerCase().includes(needle)
                            || phaseName.toLowerCase().includes(needle)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Rectangle {
                            width: 4
                            height: 24
                            radius: 2
                            color: root.analysis ? root.analysis.phaseColor(row.channelIndex) : "#8a9298"
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: row.signalName
                                color: "#242a2f"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: (row.unitName.length ? row.unitName + " · " : "") + row.roleName + " · " + row.phaseName + " · RECORDED"
                                color: "#7a8288"
                                font.pixelSize: 8
                                elide: Text.ElideRight
                            }
                        }
                    }

                    CheckBox {
                        Layout.preferredWidth: 78
                        checked: row.timeChecked
                        onToggled: rows.setProperty(row.index, "timeChecked", checked)
                    }
                    CheckBox {
                        Layout.preferredWidth: 92
                        checked: row.voltageChecked
                        onToggled: rows.setProperty(row.index, "voltageChecked", checked)
                    }
                    CheckBox {
                        Layout.preferredWidth: 92
                        checked: row.currentChecked
                        onToggled: rows.setProperty(row.index, "currentChecked", checked)
                    }
                    CheckBox {
                        Layout.preferredWidth: 92
                        checked: row.residualChecked
                        onToggled: rows.setProperty(row.index, "residualChecked", checked)
                    }
                    CheckBox {
                        Layout.preferredWidth: 78
                        checked: row.reportChecked
                        onToggled: rows.setProperty(row.index, "reportChecked", checked)
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: "#eceff1"
            border.color: "#c7ccd0"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8
                ColumnLayout {
                    spacing: 0
                    Label {
                        text: "Time is limited to " + root.maximumTracks + " visible analog lanes. Phasor/Report assignments are independent."
                        color: "#59636b"
                        font.pixelSize: 9
                    }
                    Label {
                        text: "Residual is intended for IN/UN/I0/3I0-style signals; derived sequence quantities will be added separately."
                        color: "#7a8288"
                        font.pixelSize: 8
                    }
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Cancel"
                    onClicked: root.closeRequested()
                }
                Button {
                    text: "Apply Workspace"
                    highlighted: true
                    onClicked: {
                        root.applyConfiguration()
                        root.closeRequested()
                    }
                }
            }
        }
    }
}
