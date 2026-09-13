// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: 25
    color: "#eceeef"
    border.color: "#bcc2c7"

    property var document
    property bool hasRecord: false
    property real viewStart: 0.0
    property real viewEnd: 0.0
    property real zoomFactor: 1.0

    readonly property bool hasError: document && document.error.length > 0
    readonly property int diagnosticCount: document ? document.diagnosticCount : 0
    readonly property color healthColor: hasError ? "#a62a2a" : diagnosticCount > 0 ? "#a86f00" : hasRecord ? "#2f7a47" : "#7a8288"

    function relativeMs(timeSeconds) {
        return document ? (timeSeconds - document.triggerOffsetSeconds) * 1000.0 : 0.0
    }

    component Separator: Rectangle {
        width: 1
        height: 14
        color: "#c6cbd0"
        Layout.alignment: Qt.AlignVCenter
    }

    component StatusLabel: Label {
        color: "#4f5961"
        font.pixelSize: 8
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8

        Rectangle {
            width: 7
            height: 7
            radius: 3.5
            color: root.healthColor
            Layout.alignment: Qt.AlignVCenter
        }

        StatusLabel {
            Layout.maximumWidth: 220
            Layout.minimumWidth: 90
            text: root.hasError
                  ? root.document.error
                  : root.hasRecord
                    ? (root.diagnosticCount > 0
                       ? root.diagnosticCount + " diagnostic" + (root.diagnosticCount === 1 ? "" : "s")
                       : "Record healthy")
                    : "No record"
            color: root.healthColor
            font.weight: Font.DemiBold
        }

        Separator { visible: root.hasRecord }

        StatusLabel {
            visible: root.hasRecord
            text: root.document.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY"
            font.weight: Font.DemiBold
        }

        Separator { visible: root.hasRecord }

        StatusLabel {
            visible: root.hasRecord
            text: "fn " + root.document.nominalFrequency.toFixed(2) + " Hz"
            ToolTip.visible: nominalFrequencyHover.containsMouse
            ToolTip.text: "Nominal system frequency declared by COMTRADE"
            MouseArea {
                id: nominalFrequencyHover
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }

        Separator { visible: root.hasRecord && root.width >= 980 }

        StatusLabel {
            visible: root.hasRecord && root.width >= 980
            Layout.maximumWidth: 235
            text: "fcalc " + root.document.calculationFrequency.toFixed(3) + " Hz · "
                  + root.document.calculationFrequencyProvenance
            color: "#536875"
            ToolTip.visible: calculationFrequencyHover.containsMouse
            ToolTip.text: "Shared full-cycle DFT frequency used by cursor and Locus calculations. "
                          + root.document.calculationFrequencyProvenance
            MouseArea {
                id: calculationFrequencyHover
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }

        Separator { visible: root.hasRecord && root.width >= 1120 }

        StatusLabel {
            visible: root.hasRecord && root.width >= 1120
            text: "COMTRADE " + root.document.revisionText + " · " + root.document.dataFormatText
        }

        Separator { visible: root.hasRecord && root.width >= 1240 }

        StatusLabel {
            visible: root.hasRecord && root.width >= 1240
            text: root.document.analogCount + "A / " + root.document.digitalCount + "D · "
                  + Number(root.document.sampleCount).toLocaleString(Qt.locale()) + " samples"
        }

        Separator { visible: root.hasRecord && root.width >= 1450 }

        StatusLabel {
            visible: root.hasRecord && root.width >= 1450
            Layout.maximumWidth: 245
            text: root.document.transformerRatioSummary
            color: root.document.transformerRatiosAvailable ? "#4f5961" : "#8a6d3b"
        }

        Separator { visible: root.hasRecord && root.width >= 1580 }

        StatusLabel {
            visible: root.hasRecord && root.width >= 1580
            text: "ready " + Number(root.document.lastLoadMilliseconds).toLocaleString(Qt.locale()) + " ms"
            color: "#65717a"
            ToolTip.visible: loadTimingHover.containsMouse
            ToolTip.text: "Background COMTRADE open/index time to first ready document state"
            MouseArea {
                id: loadTimingHover
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }

        Item { Layout.fillWidth: true }

        StatusLabel {
            visible: root.hasRecord && root.width >= 1180
            text: "View " + root.relativeMs(root.viewStart).toFixed(2) + " … "
                  + root.relativeMs(root.viewEnd).toFixed(2) + " ms · " + root.zoomFactor.toFixed(2) + "×"
        }

        Separator { visible: root.width >= 760 }

        StatusLabel {
            visible: root.width >= 760
            text: "ArdIREC " + Qt.application.version
            color: "#747c82"
        }
    }
}
