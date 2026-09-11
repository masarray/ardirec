// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f3f4f5"

    property var document
    property var analysis
    property var snapshot
    property var visibleChannels: []
    property real cursorTime: 0.0
    property int maximumOrder: 15
    property string displayMode: "percent"
    property string spectrumMode: "full"
    property string scopeMode: "visible"
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"

    function phaseRank(channelIndex) {
        if (!analysis) return 9
        const phase = analysis.channelPhase(channelIndex)
        if (phase === "L1") return 0
        if (phase === "L2") return 1
        if (phase === "L3") return 2
        if (phase === "E") return 3
        return 9
    }
    function orderedRole(role) {
        let channels = []
        if (!document) return channels
        for (let i = 0; i < document.analogCount; ++i)
            if (document.analogRole(i) === role) channels.push(i)
        channels.sort((a, b) => {
            const phaseDelta = phaseRank(a) - phaseRank(b)
            return phaseDelta !== 0 ? phaseDelta : a - b
        })
        return channels
    }
    function buildChannels() {
        const configured = root.visibleChannels ? root.visibleChannels.slice() : []
        if (!document) return []
        if (scopeMode === "voltage") return orderedRole("Voltage")
        if (scopeMode === "current") return orderedRole("Current")
        if (scopeMode === "all") {
            let all = []
            for (let i = 0; i < document.analogCount; ++i) all.push(i)
            return all
        }
        if (scopeMode === "electrical") return orderedRole("Voltage").concat(orderedRole("Current"))
        return configured
    }

    readonly property var displayedChannels: buildChannels()
    readonly property int voltageCount: orderedRole("Voltage").length
    readonly property int currentCount: orderedRole("Current").length

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#e7eaed"
            border.color: "#bec5ca"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 7

                ColumnLayout {
                    spacing: 0
                    Label { text: "HARMONICS"; color: "#29333a"; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
                    Label {
                        text: displayedChannels.length + " displayed · " + voltageCount + " voltage · " + currentCount + " current"
                        color: "#657078"; font.pixelSize: 9
                    }
                }
                Rectangle { width: 1; height: 30; color: "#c0c6ca"; Layout.leftMargin: 4; Layout.rightMargin: 3 }

                Label { text: "Scope"; color: "#56626b"; font.pixelSize: 9; font.weight: Font.DemiBold }
                ComboBox {
                    Layout.preferredWidth: 116; Layout.preferredHeight: 30; font.pixelSize: 10
                    model: ["Configured", "Electrical", "Voltage", "Current", "All analog"]
                    currentIndex: ["visible", "electrical", "voltage", "current", "all"].indexOf(root.scopeMode)
                    onActivated: root.scopeMode = ["visible", "electrical", "voltage", "current", "all"][currentIndex]
                }

                Label { text: "Spectrum"; color: "#56626b"; font.pixelSize: 9; font.weight: Font.DemiBold; Layout.leftMargin: 4 }
                ToolButton { text: "Full"; checkable: true; checked: root.spectrumMode === "full"; font.pixelSize: 10; onClicked: root.spectrumMode = "full" }
                ToolButton { text: "Distortion"; checkable: true; checked: root.spectrumMode === "distortion"; font.pixelSize: 10; onClicked: root.spectrumMode = "distortion" }

                Label { text: "Values"; color: "#56626b"; font.pixelSize: 9; font.weight: Font.DemiBold; Layout.leftMargin: 4 }
                ToolButton { text: "% of H1"; checkable: true; checked: root.displayMode === "percent"; font.pixelSize: 10; onClicked: root.displayMode = "percent" }
                ToolButton { text: "RMS"; checkable: true; checked: root.displayMode === "rms"; font.pixelSize: 10; onClicked: root.displayMode = "rms" }

                Label { text: "To"; color: "#56626b"; font.pixelSize: 9; font.weight: Font.DemiBold; Layout.leftMargin: 4 }
                ComboBox {
                    Layout.preferredWidth: 72; Layout.preferredHeight: 30; font.pixelSize: 10
                    model: ["H10", "H15", "H25", "H50"]
                    currentIndex: [10, 15, 25, 50].indexOf(root.maximumOrder)
                    onActivated: root.maximumOrder = [10, 15, 25, 50][currentIndex]
                }

                Item { Layout.fillWidth: true }
                Label {
                    text: root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY"
                    color: "#4d5c66"; font.pixelSize: 9; font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#f8f9fa"
            border.color: "#d3d8dc"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 12
                Rectangle { width: 8; height: 8; radius: 4; color: "#244f9e" }
                Label {
                    text: document ? "Cursor 1  " + ((root.cursorTime - document.triggerOffsetSeconds) * 1000.0).toFixed(3) + " ms" : "Cursor 1 —"
                    color: "#244f9e"; font.pixelSize: 10; font.weight: Font.DemiBold
                }
                Rectangle { width: 1; height: 18; color: "#d0d5d9" }
                Label { text: "Full-cycle DFT · RMS harmonic magnitude · THD uses H2…Hn"; color: "#56626b"; font.pixelSize: 9 }
                Item { Layout.fillWidth: true }
                Label { text: "Hover a bar for magnitude, %H1, frequency and angle"; color: "#717b82"; font.pixelSize: 9 }
            }
        }

        ListView {
            id: spectrumList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.displayedChannels
            spacing: 5
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: height
            reuseItems: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 12 }

            delegate: HarmonicDiagram {
                required property int modelData
                width: spectrumList.width - 14
                x: 3
                height: 176
                document: root.document
                analysis: root.analysis
                snapshot: root.snapshot
                channelIndex: modelData
                cursorTime: root.cursorTime
                maximumOrder: root.maximumOrder
                displayMode: root.displayMode
                spectrumMode: root.spectrumMode
                valueRepresentation: root.valueRepresentation
            }
        }

        Label {
            visible: root.displayedChannels.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "No signals are assigned to Harmonics. Open Signals → Signal Configuration…"
            color: "#657078"
            font.pixelSize: 12
        }
    }
}
