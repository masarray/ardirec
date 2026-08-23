// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 62
    color: "#f2f2f2"
    border.color: "#b7b7b7"

    property string recordTitle: "No record open"
    property string recordMetadata: ""
    property string currentViewLabel: "TIME SIGNALS"
    property bool hasRecord: false
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
            Layout.preferredHeight: 24
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
                font.pixelSize: 10
                onClicked: root.openRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Open COMTRADE CFG"
            }
            ToolButton {
                text: "Signals"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.signalsRequested()
            }
            Rectangle { width: 1; height: 24; color: "#c8c8c8" }
            ToolButton {
                text: "Fit"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.fitRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Fit complete record"
            }
            ToolButton {
                text: "Trigger"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.triggerRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Center common time view around COMTRADE trigger"
            }
            ToolButton {
                text: "Zoom +"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.zoomInRequested()
            }
            ToolButton {
                text: "Zoom −"
                font.pixelSize: 10
                enabled: root.hasRecord
                onClicked: root.zoomOutRequested()
            }

            Rectangle { width: 1; height: 24; color: "#c8c8c8" }

            ColumnLayout {
                Layout.leftMargin: 5
                spacing: 0
                Label {
                    text: root.recordTitle
                    color: "#1d1d1d"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.maximumWidth: 300
                }
                Label {
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
                font.letterSpacing: 0.6
            }
        }
    }
}
