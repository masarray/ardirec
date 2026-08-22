// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f2f2f2"
    border.color: "#b8b8b8"

    property var channels: []
    property var visibleChannels: []
    property int analogCount: 0
    property int maximumTracks: 8
    signal channelToggled(int index, bool enabled)
    signal closeRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#e4e4e4"
            border.color: "#b8b8b8"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                Label {
                    text: "Signals"
                    color: "#202020"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    text: "×"
                    font.pixelSize: 18
                    onClicked: root.closeRequested()
                    ToolTip.visible: hovered
                    ToolTip.text: "Close"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: "#f7f7f7"
            border.color: "#d0d0d0"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                TextField {
                    id: filter
                    Layout.fillWidth: true
                    placeholderText: "Filter channel name or unit"
                    selectByMouse: true
                    font.pixelSize: 11
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 7
            Layout.bottomMargin: 5
            text: "ANALOG TRACKS  ·  " + root.visibleChannels.length + "/" + root.maximumTracks
            color: "#626262"
            font.pixelSize: 9
            font.letterSpacing: 0.7
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.channels
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: row
                required property int index
                required property string modelData
                width: ListView.view.width
                height: rowVisible ? 32 : 0
                visible: rowVisible
                color: mouse.containsMouse ? "#e8f0fb" : "transparent"
                property bool isAnalog: index < root.analogCount
                property bool rowVisible: filter.text.length === 0
                                          || modelData.toLowerCase().includes(filter.text.toLowerCase())
                property bool isChecked: root.visibleChannels.indexOf(index) >= 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    CheckBox {
                        id: check
                        checked: row.isChecked
                        enabled: row.isAnalog
                        onClicked: root.channelToggled(row.index, checked)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData
                        color: row.isAnalog ? "#202020" : "#8a8a8a"
                        elide: Text.ElideRight
                        font.pixelSize: 10
                    }
                    Label {
                        visible: !row.isAnalog
                        text: "digital"
                        color: "#8c8c8c"
                        font.pixelSize: 8
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    onClicked: {
                        if (row.isAnalog)
                            root.channelToggled(row.index, !row.isChecked)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#ededed"
            border.color: "#c7c7c7"
            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2
                Label {
                    text: "Select up to " + root.maximumTracks + " analog channels."
                    color: "#525252"
                    font.pixelSize: 9
                }
                Label {
                    text: "Digital tracks will use the same synchronized timebase."
                    color: "#787878"
                    font.pixelSize: 8
                }
            }
        }
    }
}
