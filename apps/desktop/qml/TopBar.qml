// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: compactMode ? 38 : 62
    color: "#f2f2f2"
    border.color: "#b7b7b7"

    property string recordTitle: "No record open"
    property string recordMetadata: ""
    property string currentViewLabel: "TIME SIGNALS"
    property bool hasRecord: false
    readonly property bool compactMode: currentViewLabel === "ENGINEERING TABLE"
    signal openRequested()
    signal signalsRequested()
    signal fitRequested()
    signal triggerRequested()
    signal zoomInRequested()
    signal zoomOutRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactMode ? 0 : 24
            visible: !root.compactMode
            color: "#fafafa"
            border.color: "#d0d0d0"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                spacing: 14
                Repeater {
                    model: ["File", "Edit", "View", "Signals", "Analysis", "Help"]
                    Label {
                        required property string modelData
                        text: modelData
                        color: "#202020"
                        font.pixelSize: 10
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: "ardirec"
                    color: "#3a3a3a"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
                Item { Layout.preferredWidth: 8 }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 7
            Layout.rightMargin: 8
            spacing: 3

            ToolButton {
                text: "Open"
                font.pixelSize: root.compactMode ? 9 : 10
                onClicked: root.openRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Open COMTRADE CFG"
            }
            ToolButton {
                text: "Signals"
                font.pixelSize: root.compactMode ? 9 : 10
                enabled: root.hasRecord
                onClicked: root.signalsRequested()
            }
            Rectangle { width: 1; height: root.compactMode ? 18 : 24; color: "#c8c8c8" }
            ToolButton {
                visible: !root.compactMode
                text: "Fit"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.fitRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Fit complete record"
            }
            ToolButton {
                visible: !root.compactMode
                text: "Trigger"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.triggerRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Center common time view around COMTRADE trigger"
            }
            ToolButton {
                visible: !root.compactMode
                text: "Zoom +"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.zoomInRequested()
            }
            ToolButton {
                visible: !root.compactMode
                text: "Zoom −"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.zoomOutRequested()
            }
            Rectangle { visible: !root.compactMode; width: 1; height: 24; color: "#c8c8c8" }

            ColumnLayout {
                Layout.leftMargin: 5
                spacing: 0
                Label {
                    text: root.recordTitle
                    color: "#1d1d1d"
                    font.pixelSize: root.compactMode ? 9 : 10
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.compactMode ? 360 : 300
                }
                Label {
                    visible: !root.compactMode
                    text: root.recordMetadata
                    color: "#696969"
                    font.pixelSize: 8
                    elide: Text.ElideRight
                    Layout.maximumWidth: 450
                }
            }
            Item { Layout.fillWidth: true }
            Label {
                text: root.currentViewLabel
                color: "#555555"
                font.pixelSize: 9
                font.weight: root.compactMode ? Font.DemiBold : Font.Normal
                font.letterSpacing: 0.6
            }
            Label {
                visible: root.compactMode
                text: "ardirec"
                color: "#747b81"
                font.pixelSize: 8
            }
        }
    }
}
