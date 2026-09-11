// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f4f5f6"
    border.color: "#aeb5bb"

    property var document
    property var analysis
    property int analogCount: 0
    property int digitalCount: document ? document.digitalCount : 0
    property int maximumTracks: 24
    property var visibleChannels: []
    property var configuredDigitalChannels: []
    property var phasorVoltageChannels: []
    property var phasorCurrentChannels: []
    property var phasorResidualChannels: []
    property var harmonicChannels: []
    property var tableChannels: []

    signal configurationApplied(var timeChannels,
                                var digitalChannels,
                                var voltageChannels,
                                var currentChannels,
                                var residualChannels,
                                var harmonicsChannels,
                                var engineeringTableChannels)
    signal closeRequested()

    function contains(list, value) { return list && list.indexOf(value) >= 0 }
    function channelPhase(index) { return root.analysis ? root.analysis.channelPhase(index) : "Other" }
    function channelRole(index) { return root.document ? root.document.analogRole(index) : "Other" }

    function looksResidual(index) {
        if (!root.document) return false
        const name = root.document.channelName(index).trim().toUpperCase().replace(/[^A-Z0-9]/g, "")
        return name.indexOf("3I0") >= 0 || name.indexOf("3U0") >= 0 || name.indexOf("3V0") >= 0
                || name === "I0" || name === "U0" || name === "V0"
                || name.indexOf("RESIDUAL") >= 0 || name.indexOf("RESID") >= 0
                || name.endsWith("IN") || name.endsWith("UN") || name.endsWith("VN")
    }

    function reloadConfiguration() {
        rows.clear()
        if (!root.document) return
        for (let i = 0; i < root.analogCount; ++i) {
            rows.append({
                sectionName: "ANALOG · RECORDED",
                isDigital: false,
                channelIndex: i,
                signalName: root.document.channelName(i),
                unitName: root.document.channelUnit(i),
                roleName: root.channelRole(i),
                phaseName: root.channelPhase(i),
                timeChecked: root.contains(root.visibleChannels, i),
                voltageChecked: root.contains(root.phasorVoltageChannels, i),
                currentChecked: root.contains(root.phasorCurrentChannels, i),
                residualChecked: root.contains(root.phasorResidualChannels, i),
                harmonicChecked: root.contains(root.harmonicChannels, i),
                tableChecked: root.contains(root.tableChannels, i)
            })
        }
        for (let d = 0; d < root.digitalCount; ++d) {
            rows.append({
                sectionName: "BINARY · RECORDED",
                isDigital: true,
                channelIndex: d,
                signalName: root.document.digitalName(d),
                unitName: "STATE",
                roleName: root.document.digitalIsActive(d) ? "Active in record" : "Static in record",
                phaseName: "Binary",
                timeChecked: root.contains(root.configuredDigitalChannels, d),
                voltageChecked: false,
                currentChecked: false,
                residualChecked: false,
                harmonicChecked: false,
                tableChecked: false
            })
        }
    }

    function autoAssign() {
        let timeCount = 0
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.get(i)
            if (row.isDigital) {
                rows.setProperty(i, "timeChecked", true)
                continue
            }
            const role = row.roleName
            const electrical = role === "Voltage" || role === "Current"
            const useTime = electrical && timeCount < root.maximumTracks
            if (useTime) ++timeCount
            rows.setProperty(i, "timeChecked", useTime)
            rows.setProperty(i, "voltageChecked", role === "Voltage")
            rows.setProperty(i, "currentChecked", role === "Current")
            rows.setProperty(i, "residualChecked", root.looksResidual(row.channelIndex))
            rows.setProperty(i, "harmonicChecked", electrical)
            rows.setProperty(i, "tableChecked", electrical)
        }
    }

    function clearAssignments() {
        for (let i = 0; i < rows.count; ++i) {
            for (let key of ["timeChecked", "voltageChecked", "currentChecked", "residualChecked", "harmonicChecked", "tableChecked"])
                rows.setProperty(i, key, false)
        }
    }

    function collect(propertyName, digitalOnly, limit) {
        let result = []
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.get(i)
            if (row.isDigital !== digitalOnly) continue
            if (row[propertyName]) {
                if (limit > 0 && result.length >= limit) break
                result.push(row.channelIndex)
            }
        }
        return result
    }

    function count(propertyName, digitalOnly) { return collect(propertyName, digitalOnly, 0).length }

    function moveWithinSection(rowIndex, delta) {
        const next = rowIndex + delta
        if (next < 0 || next >= rows.count || filter.text.length !== 0) return
        if (rows.get(rowIndex).sectionName !== rows.get(next).sectionName) return
        rows.move(rowIndex, next, 1)
    }

    function applyConfiguration() {
        root.configurationApplied(root.collect("timeChecked", false, root.maximumTracks),
                                  root.collect("timeChecked", true, 0),
                                  root.collect("voltageChecked", false, 0),
                                  root.collect("currentChecked", false, 0),
                                  root.collect("residualChecked", false, 0),
                                  root.collect("harmonicChecked", false, 0),
                                  root.collect("tableChecked", false, 0))
    }

    ListModel { id: rows }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "#e7ebee"
            border.color: "#c0c7cc"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 10
                spacing: 12
                ColumnLayout {
                    spacing: 2
                    Label { text: "Signal Configuration"; color: "#1f282f"; font.pixelSize: 16; font.weight: Font.DemiBold }
                    Label {
                        text: "One matrix controls visible Time signals, vector groups and analysis scopes"
                        color: "#5f6a73"; font.pixelSize: 10
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.analogCount + " analog  ·  " + root.digitalCount + " binary"
                    color: "#53616b"; font.pixelSize: 10; font.weight: Font.DemiBold
                }
                ToolButton {
                    text: "×"; font.pixelSize: 22; Layout.preferredWidth: 38; Layout.preferredHeight: 34
                    onClicked: root.closeRequested(); ToolTip.visible: hovered; ToolTip.text: "Close"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#f8f9fa"
            border.color: "#d1d6da"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8
                TextField {
                    id: filter
                    Layout.preferredWidth: 320
                    Layout.preferredHeight: 34
                    placeholderText: "Search signal, unit, role or phase"
                    selectByMouse: true
                    font.pixelSize: 11
                }
                Button { text: "Auto Assign"; font.pixelSize: 10; onClicked: root.autoAssign(); ToolTip.visible: hovered; ToolTip.text: "Infer sensible defaults from COMTRADE metadata" }
                Button { text: "Reload"; font.pixelSize: 10; onClicked: root.reloadConfiguration() }
                Button { text: "Clear"; font.pixelSize: 10; onClicked: root.clearAssignments() }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.count("timeChecked", false) + " analog Time  ·  "
                          + root.count("timeChecked", true) + " binary Time  ·  "
                          + root.count("harmonicChecked", false) + " Harmonics  ·  "
                          + root.count("tableChecked", false) + " Table"
                    color: "#4f5c65"; font.pixelSize: 10
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "#dde3e7"
            border.color: "#bfc7cd"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 0
                Label { Layout.fillWidth: true; text: "SIGNAL / WORKSPACE ORDER"; color: "#3f4b54"; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
                Label { Layout.preferredWidth: 74; text: "TIME"; horizontalAlignment: Text.AlignHCenter; color: "#3f4b54"; font.pixelSize: 10; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 86; text: "VECTOR V"; horizontalAlignment: Text.AlignHCenter; color: "#3f4b54"; font.pixelSize: 10; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 86; text: "VECTOR I"; horizontalAlignment: Text.AlignHCenter; color: "#3f4b54"; font.pixelSize: 10; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 88; text: "RESIDUAL"; horizontalAlignment: Text.AlignHCenter; color: "#3f4b54"; font.pixelSize: 10; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 94; text: "HARMONICS"; horizontalAlignment: Text.AlignHCenter; color: "#3f4b54"; font.pixelSize: 10; font.weight: Font.DemiBold }
                Label { Layout.preferredWidth: 82; text: "TABLE"; horizontalAlignment: Text.AlignHCenter; color: "#3f4b54"; font.pixelSize: 10; font.weight: Font.DemiBold }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: rows
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            cacheBuffer: height * 0.5
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 12 }
            section.property: "sectionName"
            section.criteria: ViewSection.FullString
            section.delegate: Rectangle {
                required property string section
                width: list.width
                height: 34
                color: "#edf0f2"
                border.color: "#cbd1d5"
                Label {
                    anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter
                    text: section; color: "#4a555d"; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 0.7
                }
            }

            delegate: Rectangle {
                id: row
                required property int index
                required property string sectionName
                required property bool isDigital
                required property int channelIndex
                required property string signalName
                required property string unitName
                required property string roleName
                required property string phaseName
                required property bool timeChecked
                required property bool voltageChecked
                required property bool currentChecked
                required property bool residualChecked
                required property bool harmonicChecked
                required property bool tableChecked

                width: ListView.view.width
                height: rowVisible ? 48 : 0
                visible: rowVisible
                color: mouse.containsMouse ? "#eaf3fb" : (index % 2 === 0 ? "#ffffff" : "#fafbfc")
                border.color: "#e1e5e8"
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
                        spacing: 6
                        ToolButton {
                            visible: !row.isDigital
                            text: "↑"; enabled: row.index > 0 && filter.text.length === 0 && rows.get(row.index - 1).sectionName === row.sectionName
                            Layout.preferredWidth: 24; Layout.preferredHeight: 28; font.pixelSize: 11
                            onClicked: root.moveWithinSection(row.index, -1)
                        }
                        ToolButton {
                            visible: !row.isDigital
                            text: "↓"; enabled: row.index < rows.count - 1 && filter.text.length === 0 && rows.get(row.index + 1).sectionName === row.sectionName
                            Layout.preferredWidth: 24; Layout.preferredHeight: 28; font.pixelSize: 11
                            onClicked: root.moveWithinSection(row.index, 1)
                        }
                        Rectangle {
                            width: 4; height: 28; radius: 2
                            color: row.isDigital ? "#d18a19" : (root.analysis ? root.analysis.phaseColor(row.channelIndex) : "#8a9298")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                Layout.fillWidth: true; text: row.signalName; color: "#20282e"
                                font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: row.unitName + " · " + row.roleName + (row.isDigital ? "" : " · " + row.phaseName) + " · RECORDED"
                                color: "#6c767e"; font.pixelSize: 9; elide: Text.ElideRight
                            }
                        }
                    }

                    CheckBox { Layout.preferredWidth: 74; checked: row.timeChecked; onToggled: rows.setProperty(row.index, "timeChecked", checked) }
                    CheckBox { Layout.preferredWidth: 86; enabled: !row.isDigital; checked: row.voltageChecked; onToggled: rows.setProperty(row.index, "voltageChecked", checked) }
                    CheckBox { Layout.preferredWidth: 86; enabled: !row.isDigital; checked: row.currentChecked; onToggled: rows.setProperty(row.index, "currentChecked", checked) }
                    CheckBox { Layout.preferredWidth: 88; enabled: !row.isDigital; checked: row.residualChecked; onToggled: rows.setProperty(row.index, "residualChecked", checked) }
                    CheckBox { Layout.preferredWidth: 94; enabled: !row.isDigital; checked: row.harmonicChecked; onToggled: rows.setProperty(row.index, "harmonicChecked", checked) }
                    CheckBox { Layout.preferredWidth: 82; enabled: !row.isDigital; checked: row.tableChecked; onToggled: rows.setProperty(row.index, "tableChecked", checked) }
                }

                MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 66
            color: "#eceff1"
            border.color: "#c4cbd0"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10
                ColumnLayout {
                    spacing: 2
                    Label {
                        text: "Time supports up to " + root.maximumTracks + " analog lanes. Binary channels are assigned independently."
                        color: "#4f5b64"; font.pixelSize: 10
                    }
                    Label {
                        text: "Only real, working quantities are exposed. Derived sequence components appear only after they have explicit DERIVED provenance."
                        color: "#707980"; font.pixelSize: 9
                    }
                }
                Item { Layout.fillWidth: true }
                Button { text: "Cancel"; font.pixelSize: 10; onClicked: root.closeRequested() }
                Button {
                    text: "Apply Workspace"; highlighted: true; font.pixelSize: 10
                    onClicked: { root.applyConfiguration(); root.closeRequested() }
                }
            }
        }
    }
}
