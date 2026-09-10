// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 40
    color: "#f2f2f2"
    border.color: "#b7b7b7"

    property string recordTitle: "No record open"
    property string recordMetadata: ""
    property string currentViewLabel: "TIME SIGNALS"
    property bool hasRecord: false
    property var diagnostics: documentController.diagnostics
    readonly property int diagnosticCount: diagnostics ? diagnostics.length : 0

    signal openRequested()
    signal signalsRequested()
    signal fitRequested()
    signal triggerRequested()
    signal zoomInRequested()
    signal zoomOutRequested()

    Popup {
        id: aboutPopup
        width: 390
        height: 164
        x: Math.max(8, root.width - width - 10)
        y: root.height + 2
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#ffffff"; border.color: "#aeb4ba"; radius: 2 }
        contentItem: ColumnLayout {
            spacing: 4
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                color: "#eef1f3"
                border.color: "#d2d6da"
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "ArdIREC — COMTRADE Workstation"
                    color: "#293139"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }
            Label {
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 6
                text: "Protection disturbance-record analysis workstation"
                color: "#515b63"
                font.pixelSize: 9
            }
            Label {
                Layout.leftMargin: 12
                text: "Version " + Qt.application.version
                color: "#69727a"
                font.pixelSize: 8
            }
            Label {
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.fillWidth: true
                text: "COMTRADE waveform, phasor, harmonics, engineering table and distance R-X analysis."
                color: "#69727a"
                font.pixelSize: 8
                wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
        }
    }

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

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 3
        anchors.rightMargin: 8
        spacing: 2

        MenuBar {
            id: menuBar
            Layout.fillHeight: true

            Menu {
                title: "&File"
                MenuItem { text: "Open COMTRADE…"; onTriggered: root.openRequested() }
                MenuSeparator { }
                MenuItem { text: "Exit"; onTriggered: Qt.quit() }
            }
            Menu {
                title: "&View"
                MenuItem { text: "Fit complete record"; enabled: root.hasRecord; onTriggered: root.fitRequested() }
                MenuItem { text: "Focus trigger"; enabled: root.hasRecord; onTriggered: root.triggerRequested() }
                MenuSeparator { }
                MenuItem { text: "Zoom in"; enabled: root.hasRecord; onTriggered: root.zoomInRequested() }
                MenuItem { text: "Zoom out"; enabled: root.hasRecord; onTriggered: root.zoomOutRequested() }
            }
            Menu {
                title: "&Signals"
                MenuItem { text: "Select visible signals…"; enabled: root.hasRecord; onTriggered: root.signalsRequested() }
            }
            Menu {
                title: "&Help"
                MenuItem { text: "About ArdIREC"; onTriggered: aboutPopup.open() }
            }
        }

        Rectangle { width: 1; height: 22; color: "#c5c9cc"; Layout.leftMargin: 2; Layout.rightMargin: 4 }

        ToolButton {
            text: "Fit"
            font.pixelSize: 9
            enabled: root.hasRecord
            Layout.preferredWidth: 36
            onClicked: root.fitRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Fit complete record (Ctrl+0)"
        }
        ToolButton {
            text: "Trigger"
            font.pixelSize: 9
            enabled: root.hasRecord
            Layout.preferredWidth: 54
            onClicked: root.triggerRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Focus common time view around COMTRADE trigger"
        }
        ToolButton {
            text: "−"
            font.pixelSize: 11
            enabled: root.hasRecord
            Layout.preferredWidth: 30
            onClicked: root.zoomOutRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Zoom out"
        }
        ToolButton {
            text: "+"
            font.pixelSize: 11
            enabled: root.hasRecord
            Layout.preferredWidth: 30
            onClicked: root.zoomInRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Zoom in"
        }
        ToolButton {
            visible: root.hasRecord && root.diagnosticCount > 0
            text: "Diag " + root.diagnosticCount
            font.pixelSize: 8
            onClicked: diagnosticsPopup.open()
            ToolTip.visible: hovered
            ToolTip.text: "Show recoverable COMTRADE diagnostics"
        }

        Rectangle { width: 1; height: 22; color: "#c5c9cc"; Layout.leftMargin: 4; Layout.rightMargin: 5 }

        ColumnLayout {
            spacing: 0
            Layout.maximumWidth: 500
            Label {
                text: root.recordTitle
                color: "#1d1d1d"
                font.pixelSize: 9
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.maximumWidth: 480
            }
            Label {
                text: root.recordMetadata
                color: "#696969"
                font.pixelSize: 7
                elide: Text.ElideRight
                Layout.maximumWidth: 480
            }
        }

        Item { Layout.fillWidth: true }
        Label {
            text: root.currentViewLabel
            color: "#4f5961"
            font.pixelSize: 8
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
        }
        Rectangle { width: 1; height: 16; color: "#d0d3d5"; Layout.leftMargin: 5; Layout.rightMargin: 5 }
        Label {
            text: "ardirec"
            color: "#747b81"
            font.pixelSize: 8
            font.weight: Font.DemiBold
        }
    }
}
