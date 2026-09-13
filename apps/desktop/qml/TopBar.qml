// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 40
    color: "#f2f2f2"
    border.color: "#b7b7b7"

    property var document
    property var actions
    property string recordTitle: "No record open"
    property string currentViewLabel: "TIME SIGNALS"
    property bool hasRecord: false
    property var diagnostics: document ? document.diagnostics : []
    readonly property int diagnosticCount: diagnostics ? diagnostics.length : 0

    function showProperties() { if (root.hasRecord) propertiesPopup.open() }
    function showDiagnostics() { if (root.hasRecord && root.diagnosticCount > 0) diagnosticsPopup.open() }
    function showAbout() { aboutPopup.open() }

    component ToolbarIconButton: ToolButton {
        id: control
        property url iconSource
        property string tipText
        property string badgeText: ""
        Layout.preferredWidth: 34
        Layout.preferredHeight: 32
        implicitWidth: 34
        implicitHeight: 32
        padding: 0
        hoverEnabled: true

        contentItem: Item {
            implicitWidth: 32
            implicitHeight: 30
            Image {
                anchors.centerIn: parent
                width: 18
                height: 18
                source: control.iconSource
                sourceSize.width: 22
                sourceSize.height: 22
                fillMode: Image.PreserveAspectFit
                opacity: control.enabled ? 1.0 : 0.38
            }
            Rectangle {
                visible: control.badgeText.length > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 1
                anchors.topMargin: 1
                width: Math.max(15, badgeLabel.implicitWidth + 6)
                height: 15
                radius: 7.5
                color: "#a86f00"
                border.color: "#ffffff"
                Label {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: control.badgeText
                    color: "#ffffff"
                    font.pixelSize: 8
                    font.weight: Font.Bold
                }
            }
        }

        background: Rectangle {
            radius: 4
            color: !control.enabled ? "transparent"
                  : control.down ? "#d9e3ef"
                  : control.hovered ? "#e5ebf1" : "transparent"
            border.color: control.hovered && control.enabled ? "#b9c4ce" : "transparent"
        }

        ToolTip.visible: hovered
        ToolTip.delay: 350
        ToolTip.text: tipText
    }

    Popup {
        id: propertiesPopup
        width: 540
        height: 350
        x: 10
        y: root.height + 2
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#ffffff"; border.color: "#aeb4ba"; radius: 4 }
        contentItem: ColumnLayout {
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: "#eef1f3"
                border.color: "#d2d6da"
                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Label { text: "COMTRADE PROPERTIES"; color: "#293139"; font.pixelSize: 13; font.weight: Font.DemiBold }
                    Label { text: root.recordTitle; color: "#657078"; font.pixelSize: 10 }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 14
                columns: 2
                rowSpacing: 8
                columnSpacing: 16

                Label { text: "Station"; color: "#6c747b"; font.pixelSize: 10 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.title : "—"; color: "#20262b"; font.pixelSize: 10; font.weight: Font.DemiBold }
                Label { text: "Recorder"; color: "#6c747b"; font.pixelSize: 10 }
                Label { text: root.document ? root.document.recorderId : "—"; color: "#20262b"; font.pixelSize: 10 }
                Label { text: "COMTRADE"; color: "#6c747b"; font.pixelSize: 10 }
                Label { text: root.document ? root.document.revisionText + " / " + root.document.dataFormatText : "—"; color: "#20262b"; font.pixelSize: 10 }
                Label { text: "Frequency"; color: "#6c747b"; font.pixelSize: 10 }
                Label { text: root.document ? root.document.nominalFrequency.toFixed(1) + " Hz" : "—"; color: "#20262b"; font.pixelSize: 10 }
                Label { text: "CT / PT"; color: "#6c747b"; font.pixelSize: 10 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.transformerRatioSummary : "—"; color: "#20262b"; font.pixelSize: 10; elide: Text.ElideRight }
                Label { text: "Start"; color: "#6c747b"; font.pixelSize: 10 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.startTimeText : "—"; color: "#20262b"; font.pixelSize: 10; elide: Text.ElideRight }
                Label { text: "Trigger"; color: "#6c747b"; font.pixelSize: 10 }
                Label { Layout.fillWidth: true; text: root.document ? root.document.triggerTimeText : "—"; color: "#20262b"; font.pixelSize: 10; elide: Text.ElideRight }
                Label { text: "Samples"; color: "#6c747b"; font.pixelSize: 10 }
                Label { text: root.document ? Number(root.document.sampleCount).toLocaleString(Qt.locale()) : "—"; color: "#20262b"; font.pixelSize: 10 }
                Label { text: "Channels"; color: "#6c747b"; font.pixelSize: 10 }
                Label { text: root.document ? root.document.analogCount + " analog · " + root.document.digitalCount + " digital" : "—"; color: "#20262b"; font.pixelSize: 10 }
            }
            Item { Layout.fillHeight: true }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                color: "#f6f7f8"
                border.color: "#e0e3e6"
                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.document ? root.document.recordHealth : "No record"
                    color: "#657078"
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
            }
        }
    }

    Popup {
        id: aboutPopup
        width: 410
        height: 174
        x: Math.max(8, root.width - width - 10)
        y: root.height + 2
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#ffffff"; border.color: "#aeb4ba"; radius: 3 }
        contentItem: ColumnLayout {
            spacing: 5
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                color: "#eef1f3"
                border.color: "#d2d6da"
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: "ArdIREC — COMTRADE Workstation"
                    color: "#293139"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }
            Label { Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.topMargin: 6; text: "Protection disturbance-record analysis workstation"; color: "#515b63"; font.pixelSize: 10 }
            Label { Layout.leftMargin: 14; text: "Version " + Qt.application.version; color: "#69727a"; font.pixelSize: 9 }
            Label {
                Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.fillWidth: true
                text: "COMTRADE waveform, phasor, harmonics, engineering table and distance R-X analysis."
                color: "#69727a"; font.pixelSize: 9; wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
        }
    }

    Popup {
        id: diagnosticsPopup
        width: Math.min(580, Math.max(340, root.width - 24))
        height: Math.min(350, 82 + root.diagnosticCount * 42)
        x: Math.max(8, root.width - width - 8)
        y: root.height + 2
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#fbfbfb"; border.color: "#9ea4aa"; radius: 3 }
        contentItem: ColumnLayout {
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: "#f1f2f3"
                border.color: "#c5c8cb"
                Column {
                    anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; spacing: 2
                    Label { text: "RECORD DIAGNOSTICS"; color: "#2c3135"; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 0.4 }
                    Label { text: root.diagnosticCount + " recoverable issue" + (root.diagnosticCount === 1 ? "" : "s") + " · usable data retained where safe"; color: "#6a6f73"; font.pixelSize: 9 }
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
                        implicitHeight: Math.max(38, diagnosticText.implicitHeight + 16)
                        color: index % 2 === 0 ? "#ffffff" : "#f7f7f7"
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 8
                            Label { Layout.alignment: Qt.AlignTop; Layout.topMargin: 9; text: (index + 1) + "."; color: "#9a6a00"; font.pixelSize: 10; font.weight: Font.DemiBold }
                            Label { id: diagnosticText; Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; text: modelData; color: "#454a4e"; font.pixelSize: 10; wrapMode: Text.Wrap }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 8
        spacing: 2

        ToolbarIconButton {
            action: root.actions?.openRecord ?? null
            iconSource: "icons/folder-open.svg"
            tipText: "Open COMTRADE… (Ctrl+O)"
        }
        ToolbarIconButton {
            action: root.actions?.signalConfiguration ?? null
            iconSource: "icons/sliders-horizontal.svg"
            tipText: "Signal Configuration… (Ctrl+R)"
        }
        ToolbarIconButton {
            action: root.actions?.recordProperties ?? null
            iconSource: "icons/layers-3.svg"
            tipText: "Record Properties… (Alt+Enter)"
        }

        Rectangle { width: 1; height: 22; color: "#c5c9cc"; Layout.leftMargin: 4; Layout.rightMargin: 4 }

        ToolbarIconButton {
            action: root.actions?.fitRecord ?? null
            iconSource: "icons/maximize-2.svg"
            tipText: "Fit complete record (Ctrl+0)"
        }
        ToolbarIconButton {
            action: root.actions?.focusTrigger ?? null
            iconSource: "icons/crosshair.svg"
            tipText: "Focus common time view around COMTRADE trigger"
        }
        ToolbarIconButton {
            action: root.actions?.zoomOut ?? null
            iconSource: "icons/zoom-out.svg"
            tipText: "Zoom out"
        }
        ToolbarIconButton {
            action: root.actions?.zoomIn ?? null
            iconSource: "icons/zoom-in.svg"
            tipText: "Zoom in"
        }
        ToolbarIconButton {
            visible: root.hasRecord && root.diagnosticCount > 0
            action: root.actions?.recordDiagnostics ?? null
            iconSource: "icons/triangle-alert.svg"
            badgeText: String(root.diagnosticCount)
            tipText: "Show recoverable COMTRADE diagnostics"
        }

        Rectangle { width: 1; height: 22; color: "#c5c9cc"; Layout.leftMargin: 4; Layout.rightMargin: 5 }

        Label {
            text: root.recordTitle
            color: "#1d1d1d"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            Layout.maximumWidth: 420
        }

        Item { Layout.fillWidth: true }

        Label {
            text: root.currentViewLabel
            color: "#4f5961"
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
        }
    }
}
