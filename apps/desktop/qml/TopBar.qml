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
    property var diagnostics: documentController.diagnostics
    readonly property int diagnosticCount: diagnostics ? diagnostics.length : 0
    readonly property bool compactMode: currentViewLabel === "ENGINEERING TABLE"
    signal openRequested()
    signal signalsRequested()
    signal fitRequested()
    signal triggerRequested()
    signal zoomInRequested()
    signal zoomOutRequested()

    Popup {
        id: diagnosticsPopup
        width: Math.min(560, Math.max(320, root.width - 24))
        height: Math.min(330, 74 + root.diagnosticCount * 38)
        x: Math.max(8, root.width - width - 8)
        y: root.height + 2
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#fbfbfb"
            border.color: "#9ea4aa"
            radius: 2
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: "#f1f2f3"
                border.color: "#c5c8cb"

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Label {
                        text: "RECORD DIAGNOSTICS"
                        color: "#2c3135"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.5
                    }
                    Label {
                        text: root.diagnosticCount + " recoverable issue" + (root.diagnosticCount === 1 ? "" : "s")
                              + " · usable data retained where safe"
                        color: "#6a6f73"
                        font.pixelSize: 8
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: diagnosticList
                    model: root.diagnostics
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: diagnosticList.width
                        implicitHeight: Math.max(34, diagnosticText.implicitHeight + 14)
                        color: index % 2 === 0 ? "#ffffff" : "#f7f7f7"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            Label {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 8
                                text: (index + 1) + "."
                                color: "#9a6a00"
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                            Label {
                                id: diagnosticText
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: modelData
                                color: "#454a4e"
                                font.pixelSize: 9
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
        }
    }

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
            ToolButton {
                visible: root.hasRecord && root.diagnosticCount > 0
                text: "Diag " + root.diagnosticCount
                font.pixelSize: root.compactMode ? 9 : 10
                onClicked: diagnosticsPopup.open()
                ToolTip.visible: hovered
                ToolTip.text: "Show recoverable COMTRADE record diagnostics"
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
