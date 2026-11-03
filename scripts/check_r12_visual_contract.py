#!/usr/bin/env python3
"""Compact workstation visual geometry regression; works without a GPU."""
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
qml = ROOT / "apps/desktop/qml"
def require(ok: bool, what: str) -> None:
    if not ok:
        raise SystemExit(f"R12 compact visual contract FAIL: {what}")
density = (qml / "TimeSignalsView.qml").read_text(encoding="utf-8")
track = (qml / "TrackView.qml").read_text(encoding="utf-8")
guides = (qml / "PlotGuides.qml").read_text(encoding="utf-8")
sect = (qml / "SectionHeader.qml").read_text(encoding="utf-8")
event = (qml / "EventStrip.qml").read_text(encoding="utf-8")
wave = (ROOT / "apps/desktop/antialiased_trace_geometry.hpp").read_text(encoding="utf-8")
phasor = (ROOT / "apps/desktop/phasor_vector_item.cpp").read_text(encoding="utf-8")
require("property real analogTrackHeight: 106" in density, "default channel height must be compact")
require("y: 39" in track and "y: 68" in track and track.count("height: 25") >= 2,
        "both cursor readouts remain visible in compact 106px lane")
require('objectName: "phaseLaneSeparator"' in track
        and 'height: 3\n        color: "#7d929f"' in track,
        "phase lane separator is full-width 3px and distinguishable from zero")
require('objectName: "engineeringZeroLine"' in guides
        and 'height: 1\n        color: "#658094"' in guides,
        "zero guide stays 1px with exact Instant/RMS origins")
require('height: 3\n        color: "#8398a7"' in sect,
        "section boundary should be visible across Voltage and Current")
require("halfCore = 0.70F" in wave and "feather = 0.85F" in wave,
        "thin but antialiased waveform stroke")
require("halfCore = earth ? 0.60F : 0.75F" in phasor and "feather = 0.80F" in phasor,
        "thin antialiased phasor stroke")
require("import QtQuick.Shapes" in event and "ShapePath" in event
        and 'objectName: "dimensionLeftTip"' in event
        and 'objectName: "dimensionRightTip"' in event,
        "ms dimension must use outward vector chevrons, not rotated rectangles")
require("rotation: 35" not in event and "rotation: -35" not in event,
        "old inward/time dimension arrowheads are forbidden")
print("R12 visual contract: PASS — compact tracks, thin AA strokes, zero/separator hierarchy, outward ms dimension")
