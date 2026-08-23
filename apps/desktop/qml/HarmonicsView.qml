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
    property string scopeMode: "electrical"
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
        const visibleDependency = root.visibleChannels
        if (!document) return []
        if (scopeMode === "voltage") return orderedRole("Voltage")
        if (scopeMode === "current") return orderedRole("Current")
        if (scopeMode === "visible") return visibleDependency ? visibleDependency.slice() : []
        if (scopeMode === "all") {
            let all = []
            for (let i = 0; i < document.analogCount; ++i) all.push(i)
            return all
        }
        return orderedRole("Voltage").concat(orderedRole("Current"))
    }

    readonly property var displayedChannels: buildChannels()
    readonly property int voltageCount: orderedRole("Voltage").length
    readonly property int currentCount: orderedRole("Current").length

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#e8ebed"
            border.color: "#c4c9cd"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                Label {
                    text: "HARMONICS"
                    color: "#343c43"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }
                Label {
                    text: displayedChannels.length + " signals · " + voltageCount + " V · " + currentCount + " I"
                    color: "#6d757b"
                    font.pixelSize: 7
                }

                Rectangle { width: 1; height: 20; color: "#c1c6ca"; Layout.leftMargin: 4; Layout.rightMargin: 2 }
                Label { text: "SCOPE"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Repeater {
                    model: [
                        {id:"electrical", text:"Electrical"},
                        {id:"voltage", text:"Voltage"},
                        {id:"current", text:"Current"},
                        {id:"visible", text:"Visible"},
                        {id:"all", text:"All"}
                    ]
                    ToolButton {
                        required property var modelData
                        text: modelData.text
                        checkable: true
                        checked: root.scopeMode === modelData.id
                        font.pixelSize: 7
                        onClicked: root.scopeMode = modelData.id
                    }
                }

                Rectangle { width: 1; height: 20; color: "#c1c6ca"; Layout.leftMargin: 2; Layout.rightMargin: 2 }
                Label { text: "SPECTRUM"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                ToolButton { text: "Full"; checkable: true; checked: root.spectrumMode === "full"; font.pixelSize: 7; onClicked: root.spectrumMode = "full" }
                ToolButton { text: "Distortion"; checkable: true; checked: root.spectrumMode === "distortion"; font.pixelSize: 7; onClicked: root.spectrumMode = "distortion" }

                Rectangle { width: 1; height: 20; color: "#c1c6ca"; Layout.leftMargin: 2; Layout.rightMargin: 2 }
                Label { text: "VALUES"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                ToolButton { text: "% H1"; checkable: true; checked: root.displayMode === "percent"; font.pixelSize: 7; onClicked: root.displayMode = "percent" }
                ToolButton { text: "RMS"; checkable: true; checked: root.displayMode === "rms"; font.pixelSize: 7; onClicked: root.displayMode = "rms" }

                Rectangle { width: 1; height: 20; color: "#c1c6ca"; Layout.leftMargin: 2; Layout.rightMargin: 2 }
                Label { text: "ORDER"; color: "#687078"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Repeater {
                    model: [10, 15, 25, 50]
                    ToolButton {
                        required property int modelData
                        text: "H" + modelData
                        checkable: true
                        checked: root.maximumOrder === modelData
                        font.pixelSize: 7
                        onClicked: root.maximumOrder = modelData
                    }
                }

                Item { Layout.fillWidth: true }
                Label {
                    text: (root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY")
                          + " · 1 cursor · 1-cycle trailing DFT"
                    color: "#53616b"
                    font.pixelSize: 7
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: "#f7f8f9"
            border.color: "#d5d9dc"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 10
                Label {
                    text: document
                          ? "Cursor " + ((root.cursorTime - document.triggerOffsetSeconds) * 1000.0).toFixed(3) + " ms"
                          : "Cursor —"
                    color: "#244f9e"
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
                Label {
                    text: "Full = H0/DC + H1 + H2… · bar labels show %H1 and engineering magnitude when space permits"
                    color: "#687078"
                    font.pixelSize: 7
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: "Scroll keeps spectra virtualized; moving the harmonic cursor does not change scope or scroll position"
                    color: "#7a8288"
                    font.pixelSize: 7
                }
            }
        }

        ListView {
            id: spectrumList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.displayedChannels
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: height * 0.7
            reuseItems: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 10 }

            delegate: HarmonicDiagram {
                required property int modelData
                width: spectrumList.width - 12
                x: 2
                height: 128
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
    }
}
