// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f4f4f4"
    border.color: "#bdbdbd"

    property var document
    property var visibleChannels: []
    property int measurementChannel: -1
    property real cursorATime: 0.0
    property real cursorBTime: 0.0
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

    function deltaValueText() {
        if (!document || measurementChannel < 0) return "—"
        const a = document.sampleValue(measurementChannel, cursorATime)
        const b = document.sampleValue(measurementChannel, cursorBTime)
        if (!Number.isFinite(a) || !Number.isFinite(b)) return "—"
        const delta = b - a
        const magnitude = Math.abs(delta)
        let decimals = 4
        if (magnitude >= 1000) decimals = 1
        else if (magnitude >= 100) decimals = 2
        else if (magnitude >= 10) decimals = 3
        const unit = document.channelUnit(measurementChannel)
        return delta.toFixed(decimals) + (unit.length ? " " + unit : "")
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 7

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 720
            color: "#ffffff"
            border.color: "#bcbcbc"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                    color: "#e9e9e9"
                    border.color: "#c5c5c5"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 5
                        spacing: 6
                        Label {
                            text: "Cursor measurements"
                            color: "#333333"
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: "Measuring signal"
                            color: "#666666"
                            font.pixelSize: 8
                        }
                        ComboBox {
                            id: measureSignal
                            Layout.preferredWidth: 230
                            Layout.preferredHeight: 22
                            model: root.visibleChannels
                            currentIndex: Math.max(0, root.visibleChannels.indexOf(root.measurementChannel))
                            contentItem: Label {
                                leftPadding: 6
                                verticalAlignment: Text.AlignVCenter
                                text: root.document && root.measurementChannel >= 0
                                      ? root.document.channelName(root.measurementChannel)
                                      : "—"
                                color: "#202020"
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                            delegate: ItemDelegate {
                                required property int modelData
                                width: measureSignal.width
                                height: 24
                                text: root.document ? root.document.channelName(modelData) : "—"
                                font.pixelSize: 9
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
                    Layout.preferredHeight: 20
                    color: "#f3f3f3"
                    border.color: "#d0d0d0"
                    Row {
                        anchors.fill: parent
                        Label { width: 92; height: parent.height; leftPadding: 6; verticalAlignment: Text.AlignVCenter; text: "Cursor"; color: "#555"; font.pixelSize: 8 }
                        Label { width: 108; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Time [ms]"; color: "#555"; font.pixelSize: 8 }
                        Label { width: 220; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Measuring signal"; color: "#555"; font.pixelSize: 8 }
                        Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Instantaneous"; color: "#555"; font.pixelSize: 8 }
                        Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "Difference"; color: "#555"; font.pixelSize: 8 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: "#ffffff"
                    border.color: "#dddddd"
                    Row {
                        anchors.fill: parent
                        Label { width: 92; height: parent.height; leftPadding: 6; verticalAlignment: Text.AlignVCenter; text: "Cursor 1"; color: root.cursorAColor; font.pixelSize: 9; font.weight: Font.DemiBold }
                        Label { width: 108; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.timeText(root.cursorATime); color: "#222"; font.pixelSize: 9 }
                        Label { width: 220; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.document && root.measurementChannel >= 0 ? root.document.channelName(root.measurementChannel) : "—"; color: "#222"; font.pixelSize: 9; elide: Text.ElideRight }
                        Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.document && root.measurementChannel >= 0 ? root.document.sampleValueText(root.measurementChannel, root.cursorATime) : "—"; color: "#222"; font.pixelSize: 9 }
                        Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "—"; color: "#777"; font.pixelSize: 9 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: "#fafafa"
                    border.color: "#dddddd"
                    Row {
                        anchors.fill: parent
                        Label { width: 92; height: parent.height; leftPadding: 6; verticalAlignment: Text.AlignVCenter; text: "Cursor 2"; color: root.cursorBColor; font.pixelSize: 9; font.weight: Font.DemiBold }
                        Label { width: 108; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.timeText(root.cursorBTime); color: "#222"; font.pixelSize: 9 }
                        Label { width: 220; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.document && root.measurementChannel >= 0 ? root.document.channelName(root.measurementChannel) : "—"; color: "#222"; font.pixelSize: 9; elide: Text.ElideRight }
                        Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.document && root.measurementChannel >= 0 ? root.document.sampleValueText(root.measurementChannel, root.cursorBTime) : "—"; color: "#222"; font.pixelSize: 9 }
                        Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "—"; color: "#777"; font.pixelSize: 9 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: "#ffffff"
                    border.color: "#dddddd"
                    Row {
                        anchors.fill: parent
                        Label { width: 92; height: parent.height; leftPadding: 6; verticalAlignment: Text.AlignVCenter; text: "C2 − C1"; color: "#333"; font.pixelSize: 9; font.weight: Font.DemiBold }
                        Label { width: 108; height: parent.height; verticalAlignment: Text.AlignVCenter; text: ((root.cursorBTime - root.cursorATime) * 1000.0).toFixed(3); color: "#222"; font.pixelSize: 9 }
                        Label { width: 220; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.document && root.measurementChannel >= 0 ? root.document.channelName(root.measurementChannel) : "—"; color: "#222"; font.pixelSize: 9; elide: Text.ElideRight }
                        Label { width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.deltaValueText(); color: "#222"; font.pixelSize: 9 }
                        Label { width: 110; height: parent.height; verticalAlignment: Text.AlignVCenter; text: root.cursorBTime !== root.cursorATime ? (1000.0 / Math.abs((root.cursorBTime - root.cursorATime) * 1000.0)).toFixed(2) + " Hz" : "—"; color: "#555"; font.pixelSize: 9 }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 390
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#bcbcbc"

            GridLayout {
                anchors.fill: parent
                anchors.margins: 7
                columns: 2
                rowSpacing: 2
                columnSpacing: 8

                Label { text: "Station"; color: "#666"; font.pixelSize: 8 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.title : "—"; color: "#222"; font.pixelSize: 8; elide: Text.ElideRight }
                Label { text: "Recorder"; color: "#666"; font.pixelSize: 8 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.recorderId : "—"; color: "#222"; font.pixelSize: 8; elide: Text.ElideRight }
                Label { text: "COMTRADE"; color: "#666"; font.pixelSize: 8 }
                Label { text: root.document ? root.document.revisionText + " / " + root.document.dataFormatText : "—"; color: "#222"; font.pixelSize: 8 }
                Label { text: "Frequency"; color: "#666"; font.pixelSize: 8 }
                Label { text: root.document ? root.document.nominalFrequency.toFixed(1) + " Hz" : "—"; color: "#222"; font.pixelSize: 8 }
                Label { text: "Start"; color: "#666"; font.pixelSize: 8 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.startTimeText : "—"; color: "#222"; font.pixelSize: 8; elide: Text.ElideRight }
                Label { text: "Trigger"; color: "#666"; font.pixelSize: 8 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.triggerTimeText : "—"; color: "#222"; font.pixelSize: 8; elide: Text.ElideRight }
            }
        }
    }
}
