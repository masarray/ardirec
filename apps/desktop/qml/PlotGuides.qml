// SPDX-License-Identifier: GPL-3.0-or-later
// Shared static engineering reference grid: C++ renderers own the traces.
// The zero baseline MUST use exactly the same normalized mapping as the
// instantaneous (0.50) and one-cycle RMS (0.90) QSG waveform renderers.
import QtQuick

Item {
    id: root
    property string displayMode: "instantaneous"
    readonly property real zeroFraction: displayMode === "rms" ? 0.90 : 0.50
    readonly property real zeroPixel: Math.round(height * zeroFraction)
    readonly property real upperFraction: displayMode === "rms" ? 0.10 : 0.07
    readonly property real lowerFraction: displayMode === "rms" ? 0.90 : 0.93

    // Vertical time grid intentionally stays subdued. Cursor and trigger lines
    // are separate, higher-contrast overlays owned by TrackView.
    Repeater {
        model: 11
        Rectangle {
            required property int index
            x: Math.round(root.width * index / 10)
            y: 0
            width: 1
            height: root.height
            color: index === 0 || index === 10 ? "#c3ccd3" : "#e8ecef"
        }
    }

    Repeater {
        model: root.displayMode === "rms"
               ? [0.10, 0.30, 0.50, 0.70]
               : [0.07, 0.285, 0.715, 0.93]
        Rectangle {
            required property real modelData
            x: 0
            y: Math.round(root.height * modelData)
            width: root.width
            height: 1
            color: "#e0e6eb"
        }
    }

    Rectangle {
        id: zeroReference
        objectName: "engineeringZeroLine"
        x: 0
        // Sharp 1px engineering baseline; 3px lane dividers cannot be
        // confused with this reference. Keep RMS=90% / Instant=50%.
        y: root.zeroPixel - 0.5
        width: root.width
        height: 1
        color: "#658094"
    }
}
