// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"
    border.color: "#cbd2d7"
    radius: 2

    property var document
    property var analysis
    property real cursorTime: 0.0
    property string cursorLabel: "C1"
    property color cursorAccent: "#244f9e"
    property string valueRepresentation: document ? document.valueRepresentation : "secondary"

    readonly property var voltageSnapshot: {
        const representationDependency = root.valueRepresentation
        return root.analysis ? root.analysis.sequenceComponentsAt("Voltage", root.cursorTime) : ({valid:false})
    }
    readonly property var currentSnapshot: {
        const representationDependency = root.valueRepresentation
        return root.analysis ? root.analysis.sequenceComponentsAt("Current", root.cursorTime) : ({valid:false})
    }
    readonly property bool hasVoltage: voltageSnapshot && voltageSnapshot.valid
    readonly property bool hasCurrent: currentSnapshot && currentSnapshot.valid
    readonly property bool hasData: hasVoltage || hasCurrent

    function displayScale(snapshot) {
        if (!root.document || !snapshot || !snapshot.valid || root.valueRepresentation !== "primary") return 1.0
        const index = snapshot.sourceL1
        if (index < 0) return 1.0
        const unit = root.document.channelUnit(index).trim().toUpperCase()
        const peak = Math.abs(root.document.channelPeak(index))
        if (unit === "V" || unit === "A") {
            if (peak >= 1000000.0) return 0.000001
            if (peak >= 1000.0) return 0.001
        }
        return 1.0
    }

    function displayUnit(snapshot) {
        if (!snapshot || !snapshot.valid) return ""
        const unit = snapshot.unit || ""
        const scale = root.displayScale(snapshot)
        if (scale === 0.000001) return unit.trim().toUpperCase() === "V" ? "MV" : "MA"
        if (scale === 0.001) return unit.trim().toUpperCase() === "V" ? "kV" : "kA"
        return unit
    }

    function formatMagnitude(snapshot, component) {
        if (!snapshot || !snapshot.valid || !component || !component.valid || !Number.isFinite(component.magnitude)) return "—"
        const value = component.magnitude * root.displayScale(snapshot)
        const absValue = Math.abs(value)
        const decimals = absValue >= 100 ? 1 : absValue >= 10 ? 2 : 3
        const unit = root.displayUnit(snapshot)
        return value.toFixed(decimals) + (unit.length ? " " + unit : "")
    }

    function componentText(snapshot, key) {
        if (!snapshot || !snapshot.valid) return "—"
        const component = snapshot[key]
        if (!component || !component.valid) return "—"
        const angle = Number.isFinite(component.angle) ? component.angle.toFixed(1) + "°" : "—"
        return component.name + "  " + root.formatMagnitude(snapshot, component) + "  ∠" + angle
    }

    function ratioText(snapshot, numerator) {
        if (!snapshot || !snapshot.valid) return "—"
        const value = numerator === "negative" ? snapshot.negativePercent : snapshot.zeroPercent
        if (!Number.isFinite(value)) return "—"
        const prefix = snapshot.role === "Voltage" ? "V" : "I"
        return prefix + (numerator === "negative" ? "2/" : "0/") + prefix + "1  "
               + value.toFixed(value < 10.0 ? 2 : 1) + "%"
    }

    component MetricCell: Rectangle {
        required property string valueText
        color: "#f8fafb"
        border.color: "#dde3e7"
        radius: 2
        Label {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            verticalAlignment: Text.AlignVCenter
            text: parent.valueText
            color: "#364149"
            font.pixelSize: 9
            font.family: "Consolas"
            elide: Text.ElideRight
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        color: root.cursorAccent
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            spacing: 7
            Label {
                text: root.cursorLabel
                color: root.cursorAccent
                font.pixelSize: 10
                font.weight: Font.Bold
            }
            Rectangle {
                Layout.preferredWidth: 58
                Layout.preferredHeight: 16
                radius: 2
                color: "#edf1f4"
                border.color: "#cbd3d9"
                Label {
                    anchors.centerIn: parent
                    text: "DERIVED"
                    color: "#64717a"
                    font.pixelSize: 7
                    font.weight: Font.Bold
                    font.letterSpacing: 0.5
                }
            }
            Item { Layout.fillWidth: true }
            Label {
                text: root.valueRepresentation === "primary" ? "PRIMARY" : "SECONDARY"
                color: "#7a838a"
                font.pixelSize: 8
            }
        }

        RowLayout {
            visible: root.hasVoltage
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 27 : 0
            spacing: 4
            Label { Layout.preferredWidth: 50; text: "V SEQ"; color: "#56636c"; font.pixelSize: 8; font.weight: Font.DemiBold }
            MetricCell { Layout.fillWidth: true; Layout.preferredWidth: 1; valueText: root.componentText(root.voltageSnapshot, "positive") }
            MetricCell { Layout.fillWidth: true; Layout.preferredWidth: 1; valueText: root.componentText(root.voltageSnapshot, "negative") }
            MetricCell { Layout.fillWidth: true; Layout.preferredWidth: 1; valueText: root.componentText(root.voltageSnapshot, "zero") }
            MetricCell { Layout.preferredWidth: 94; valueText: root.ratioText(root.voltageSnapshot, "negative") }
        }

        RowLayout {
            visible: root.hasCurrent
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 27 : 0
            spacing: 4
            Label { Layout.preferredWidth: 50; text: "I SEQ"; color: "#56636c"; font.pixelSize: 8; font.weight: Font.DemiBold }
            MetricCell { Layout.fillWidth: true; Layout.preferredWidth: 1; valueText: root.componentText(root.currentSnapshot, "positive") }
            MetricCell { Layout.fillWidth: true; Layout.preferredWidth: 1; valueText: root.componentText(root.currentSnapshot, "negative") }
            MetricCell { Layout.fillWidth: true; Layout.preferredWidth: 1; valueText: root.componentText(root.currentSnapshot, "zero") }
            MetricCell { Layout.preferredWidth: 94; valueText: root.ratioText(root.currentSnapshot, "negative") }
        }
    }
}
