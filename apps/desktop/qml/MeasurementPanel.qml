// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f2f3f4"
    border.color: "#b8bdc2"

    property var document
    property var analysis
    property var visibleChannels: []
    property int measurementChannel: -1
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
    property string valueRepresentation: "secondary"
    property color cursorAColor: "#244f9e"
    property color cursorBColor: "#b77900"
    signal measurementChannelRequested(int channelIndex)

    function relativeMs(timeSeconds) {
        if (!document) return 0
        return (timeSeconds - document.triggerOffsetSeconds) * 1000.0
    }

    function timeText(timeSeconds) {
        return relativeMs(timeSeconds).toFixed(3)
    }

    function formatValue(value) {
        if (!document || measurementChannel < 0 || !Number.isFinite(value)) return "—"
        return document.formatChannelValue(measurementChannel, value)
    }

    function instantAt(timeSeconds) {
        return document && measurementChannel >= 0 ? document.sampleValue(measurementChannel, timeSeconds) : NaN
    }

    function rmsAt(timeSeconds) {
        return analysis && measurementChannel >= 0 ? analysis.rmsValue(measurementChannel, timeSeconds) : NaN
    }

    function deltaTimeMs() {
        return (cursorBTime - cursorATime) * 1000.0
    }

    function cyclesText() {
        if (!document) return "—"
        return (Math.abs(cursorBTime - cursorATime) * document.nominalFrequency).toFixed(4) + " cyc"
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 6

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 850
            color: "#ffffff"
            border.color: "#b9bec3"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 23
                    color: "#e7eaed"
                    border.color: "#c4c9ce"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 5
                        spacing: 6
                        Label {
                            text: "CURSOR MEASUREMENTS"
                            color: "#343b41"
                            font.pixelSize: 8
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.6
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: "Measuring signal"
                            color: "#666e75"
                            font.pixelSize: 8
                        }
                        ComboBox {
                            id: measureSignal
                            Layout.preferredWidth: 245
                            Layout.preferredHeight: 21
                            model: root.visibleChannels
                            currentIndex: Math.max(0, root.visibleChannels.indexOf(root.measurementChannel))
                            contentItem: Label {
                                leftPadding: 6
                                verticalAlignment: Text.AlignVCenter
                                text: root.document && root.measurementChannel >= 0
                                      ? root.document.channelName(root.measurementChannel)
                                      : "—"
                                color: "#20262b"
                                font.pixelSize: 8
                                elide: Text.ElideRight
                            }
                            delegate: ItemDelegate {
                                required property int modelData
                                width: measureSignal.width
                                height: 23
                                text: root.document ? root.document.channelName(modelData) : "—"
                                font.pixelSize: 8
                            }
                            onActivated: index => {
                                if (index >= 0 && index < root.visibleChannels.length)
                                    root.measurementChannelRequested(root.visibleChannels[index])
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 18
                    color: "#f0f2f4"
                    border.color: "#d2d6da"
                    Row {
                        anchors.fill: parent
                        Label { width: 82; height: parent.height; leftPadding: 6; verticalAlignment: Text.AlignVCenter; text: "Cursor"; color: "#555e65"; font.pixelSize: 7 }
                        Label { width: 100; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Time [ms]"; color: "#555e65"; font.pixelSize: 7 }
                        Label { width: 210; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Measuring signal"; color: "#555e65"; font.pixelSize: 7 }
                        Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Instantaneous"; color: "#555e65"; font.pixelSize: 7 }
                        Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "R.M.S."; color: "#555e65"; font.pixelSize: 7 }
                        Label { width: 100; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Context"; color: "#555e65"; font.pixelSize: 7 }
                    }
                }

                Repeater {
                    model: [
                        {name:"Cursor 1", kind:"c1"},
                        {name:"Cursor 2", kind:"c2"},
                        {name:"C2 − C1", kind:"delta"},
                        {name:"C2 + C1", kind:"sum"}
                    ]
                    Rectangle {
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 20
                        color: index % 2 ? "#fafbfc" : "#ffffff"
                        border.color: "#e0e3e6"

                        readonly property bool isC1: modelData.kind === "c1"
                        readonly property bool isC2: modelData.kind === "c2"
                        readonly property bool isDelta: modelData.kind === "delta"
                        readonly property real timeValue: isC1 ? root.relativeMs(root.cursorATime)
                                                              : isC2 ? root.relativeMs(root.cursorBTime)
                                                                     : isDelta ? root.relativeMs(root.cursorBTime) - root.relativeMs(root.cursorATime)
                                                                               : root.relativeMs(root.cursorBTime) + root.relativeMs(root.cursorATime)
                        readonly property real instantValue: isC1 ? root.instantAt(root.cursorATime)
                                                                 : isC2 ? root.instantAt(root.cursorBTime)
                                                                        : isDelta ? root.instantAt(root.cursorBTime) - root.instantAt(root.cursorATime)
                                                                                  : root.instantAt(root.cursorBTime) + root.instantAt(root.cursorATime)
                        readonly property real rmsValue: isC1 ? root.rmsAt(root.cursorATime)
                                                             : isC2 ? root.rmsAt(root.cursorBTime)
                                                                    : isDelta ? root.rmsAt(root.cursorBTime) - root.rmsAt(root.cursorATime)
                                                                              : root.rmsAt(root.cursorBTime) + root.rmsAt(root.cursorATime)

                        Row {
                            anchors.fill: parent
                            Label {
                                width: 82; height: parent.height; leftPadding: 6; verticalAlignment: Text.AlignVCenter
                                text: modelData.name
                                color: isC1 ? root.cursorAColor : isC2 ? root.cursorBColor : "#343b41"
                                font.pixelSize: 8
                                font.weight: Font.DemiBold
                            }
                            Label { width: 100; height: parent.height; verticalAlignment: Text.AlignVCenter; text: timeValue.toFixed(3); color: "#252b30"; font.pixelSize: 8 }
                            Label {
                                width: 210; height: parent.height; verticalAlignment: Text.AlignVCenter
                                text: root.document && root.measurementChannel >= 0 ? root.document.channelName(root.measurementChannel) : "—"
                                color: "#30363b"; font.pixelSize: 8; elide: Text.ElideRight
                            }
                            Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.formatValue(instantValue); color: "#20262b"; font.pixelSize: 8 }
                            Label { width: 145; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.formatValue(rmsValue); color: "#20262b"; font.pixelSize: 8 }
                            Label {
                                width: 100; height: parent.height; verticalAlignment: Text.AlignVCenter
                                text: isDelta ? root.cyclesText() : isC1 || isC2 ? root.valueRepresentation.toUpperCase() : ""
                                color: "#697178"; font.pixelSize: 7
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 390
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#b9bec3"

            GridLayout {
                anchors.fill: parent
                anchors.margins: 6
                columns: 2
                rowSpacing: 2
                columnSpacing: 7

                Label { text: "Station"; color: "#666e75"; font.pixelSize: 7 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.title : "—"; color: "#20262b"; font.pixelSize: 7; elide: Text.ElideRight }
                Label { text: "Recorder"; color: "#666e75"; font.pixelSize: 7 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.recorderId : "—"; color: "#20262b"; font.pixelSize: 7; elide: Text.ElideRight }
                Label { text: "COMTRADE"; color: "#666e75"; font.pixelSize: 7 }
                Label { text: root.document ? root.document.revisionText + " / " + root.document.dataFormatText : "—"; color: "#20262b"; font.pixelSize: 7 }
                Label { text: "Frequency"; color: "#666e75"; font.pixelSize: 7 }
                Label { text: root.document ? root.document.nominalFrequency.toFixed(1) + " Hz" : "—"; color: "#20262b"; font.pixelSize: 7 }
                Label { text: "Representation"; color: "#666e75"; font.pixelSize: 7 }
                Label { text: root.valueRepresentation === "primary" ? "Primary" : "Secondary"; color: "#20262b"; font.pixelSize: 7; font.weight: Font.DemiBold }
                Label { text: "CT/PT"; color: "#666e75"; font.pixelSize: 7 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.transformerRatioSummary : "—"; color: root.document && root.document.transformerRatiosAvailable ? "#20262b" : "#8a6d3b"; font.pixelSize: 7; elide: Text.ElideRight }
                Label { text: "Start"; color: "#666e75"; font.pixelSize: 7 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.startTimeText : "—"; color: "#20262b"; font.pixelSize: 7; elide: Text.ElideRight }
                Label { text: "Trigger"; color: "#666e75"; font.pixelSize: 7 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.triggerTimeText : "—"; color: "#20262b"; font.pixelSize: 7; elide: Text.ElideRight }
            }
        }
    }
}
