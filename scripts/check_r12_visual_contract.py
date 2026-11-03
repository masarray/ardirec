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
metrics = (qml / "WorkstationVisualMetrics.js").read_text(encoding="utf-8")
require("var analogTrackHeight = 72" in metrics and "var digitalTrackHeight = 24" in metrics,
        "canonical compact density must fit analog and digital channels")
for file in ("Main.qml", "MdiWorkspace.qml", "AnalysisViewHost.qml", "TimeSignalsView.qml"):
    view = (qml / file).read_text(encoding="utf-8")
    require('import "WorkstationVisualMetrics.js" as VisualMetrics' in view
            and "property real analogTrackHeight: VisualMetrics.analogTrackHeight" in view
            and "property real digitalTrackHeight: VisualMetrics.digitalTrackHeight" in view,
            f"{file} overrides or forgets shared density")
require("analogTrackHeight: window.analogTrackHeight" in (qml / "Main.qml").read_text(encoding="utf-8")
        and "analogTrackHeight: root.analogTrackHeight" in (qml / "MdiWorkspace.qml").read_text(encoding="utf-8")
        and "analogTrackHeight: root.analogTrackHeight" in (qml / "AnalysisViewHost.qml").read_text(encoding="utf-8"),
        "Main -> MDI -> AnalysisHost -> TimeSignals binding chain broken")
require("y: 33" in track and "y: 51" in track and track.count("height: 17") >= 2,
        "both C1/C2 readouts must fit completely in the 72px analog lane")
require('objectName: "phaseLaneSeparator"' in track
        and 'height: 3\n        color: "#7d929f"' in track,
        "phase lane separator is full-width 3px and distinguishable from zero")
require('objectName: "engineeringZeroLine"' in guides
        and 'height: 1\n        color: "#658094"' in guides,
        "zero guide stays 1px with exact Instant/RMS origins")
require('height: 3\n        color: "#8398a7"' in sect,
        "section boundary should be visible across Voltage and Current")
require("halfCore = 0.55F" in wave and "feather = 0.35F" in wave,
        "waveform must stay close to original thin 1px appearance")
require("halfCore = earth ? 0.50F : 0.60F" in phasor and "feather = 0.40F" in phasor,
        "phasor shaft must remain fine and antialiased")
require("import QtQuick.Shapes" in event and "ShapePath" in event
        and 'objectName: "dimensionLeftTip"' in event
        and 'objectName: "dimensionRightTip"' in event,
        "ms dimension must use outward vector chevrons, not rotated rectangles")
require("rotation: 35" not in event and "rotation: -35" not in event,
        "old inward/time dimension arrowheads are forbidden")
print("R12 visual contract: PASS — canonical 72px analog / 24px digital, fine AA lines, zero/separator hierarchy, outward ms dimension")
