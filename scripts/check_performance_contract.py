#!/usr/bin/env python3
"""Fail CI when known ArdIREC performance-architecture regressions return.

This is deliberately an architectural gate, not a noisy cloud-runner timing test.
Measured benchmarks remain separate; these checks make the most damaging hot-path
patterns impossible to merge accidentally.
"""
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FAILURES: list[str] = []


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, needle: str, reason: str) -> None:
    if needle not in text(path):
        FAILURES.append(f"{path}: missing {needle!r} — {reason}")


def forbid(path: str, needle: str, reason: str) -> None:
    if needle in text(path):
        FAILURES.append(f"{path}: found forbidden {needle!r} — {reason}")


def function_body(source: str, signature_fragment: str) -> str:
    start = source.find(signature_fragment)
    if start < 0:
        return ""
    brace = source.find("{", start)
    if brace < 0:
        return ""
    depth = 0
    for index in range(brace, len(source)):
        ch = source[index]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    return ""


# RMS render path: scene-graph synchronization must never traverse source samples.
rms = text("apps/desktop/rms_waveform_item.cpp")
rms_paint = function_body(rms, "RmsWaveformItem::updatePaintNode")
if not rms_paint:
    FAILURES.append("apps/desktop/rms_waveform_item.cpp: updatePaintNode() not found")
else:
    for token in ("analogValue(", "rms_cycle_window", "copyRecordedAnalogRange", "sumSquares"):
        if token in rms_paint:
            FAILURES.append(
                f"apps/desktop/rms_waveform_item.cpp: {token!r} inside updatePaintNode() — numerical traversal belongs in background preparation"
            )
require("apps/desktop/rms_waveform_item.cpp", "QtConcurrent::run", "RMS viewport preparation must stay asynchronous")
require("apps/desktop/rms_waveform_item.cpp", "cancel", "RMS background preparation must remain cancellable/latest-wins")

# Cursor interaction: raw pointer-rate movement must be preview/coalesced.
time_view = text("apps/desktop/qml/TimeSignalsView.qml")
require("apps/desktop/qml/TimeSignalsView.qml", "analysisCommitTimer", "cursor analysis commits must be frame-coalesced")
require("apps/desktop/qml/TimeSignalsView.qml", "pendingATime", "cursor drag must use pending/latest state")
position_match = re.search(r"onPositionChanged\s*:\s*mouse\s*=>\s*\{(?P<body>.*?)\n\s*\}", time_view, re.S)
if position_match:
    body = position_match.group("body")
    if "root.cursorARequested(" in body or "root.cursorBRequested(" in body:
        FAILURES.append("apps/desktop/qml/TimeSignalsView.qml: raw pointer-rate cursor commit bypasses coalescing")

# Shared cursor snapshot: one async batched source for expensive fundamental work.
require("apps/desktop/cursor_snapshot_controller.cpp", "QtConcurrent::run", "cursor fundamental analysis must run off the UI thread")
require("apps/desktop/cursor_snapshot_controller.cpp", "m_generationA", "stale C1 work must be generation-guarded")
require("apps/desktop/cursor_snapshot_controller.cpp", "m_generationB", "stale C2 work must be generation-guarded")
require("apps/desktop/cursor_snapshot_controller.cpp", "std::vector<std::complex<long double>> accumulators", "fundamental phasors must be batched in one sample traversal")

# Phasor: dynamic vectors must stay in retained native scene-graph geometry.
forbid("apps/desktop/qml/PhasorDiagram.qml", "Canvas {", "high-frequency phasor Canvas repaint/texture uploads are forbidden")
require("apps/desktop/qml/PhasorDiagram.qml", "PhasorVectorItem", "phasor vectors must use retained QSG geometry")
require("apps/desktop/phasor_vector_item.cpp", "QSGGeometryNode", "native phasor renderer is required")

# Locus: no synchronous bulk analysis and no per-point QVariant materialization in worker path.
proxy = text("apps/desktop/qml/LocusAnalysisProxy.qml")
if ".distanceLoci(" in proxy or ".distanceLocus(" in proxy:
    FAILURES.append("apps/desktop/qml/LocusAnalysisProxy.qml: synchronous bulk distance locus call is forbidden")
if "locusSnapshotController.locus(" in proxy:
    FAILURES.append("apps/desktop/qml/LocusAnalysisProxy.qml: bulk per-point QVariant materialization is forbidden in production QML")
require("apps/desktop/locus_snapshot_controller.cpp", "QtConcurrent::run", "locus generation must run in a background worker")
require("apps/desktop/locus_snapshot_controller.hpp", "std::array<std::vector<LocusNativePoint>, 6>", "protection loci must remain in contiguous native buffers")
require("apps/desktop/locus_trajectory_item.cpp", "QSGGeometryNode", "locus trajectories must render with retained scene-graph geometry")
require("apps/desktop/qml/LocusView.qml", "LocusTrajectoryItem", "production locus view must consume native trajectory geometry")
forbid("apps/desktop/qml/LocusView.qml", "for(let item of series)drawSeries", "Canvas must not paint bulk protection trajectories")

locus_cpp = text("apps/desktop/locus_snapshot_controller.cpp")
bulk_worker = function_body(locus_cpp, "build_locus_snapshot")
if not bulk_worker:
    FAILURES.append("apps/desktop/locus_snapshot_controller.cpp: build_locus_snapshot() not found")
elif "QVariantMap" in bulk_worker or "QVariantList" in bulk_worker:
    FAILURES.append("apps/desktop/locus_snapshot_controller.cpp: bulk locus worker allocates QVariant containers per trajectory")
if "4096" not in locus_cpp:
    FAILURES.append("apps/desktop/locus_snapshot_controller.cpp: bounded locus point budget guard is missing")

# Contract documentation itself must remain discoverable from agent rules.
require("AGENTS.md", "PERFORMANCE_CONTRACT.md", "agents must be directed to the enforceable performance contract")

if FAILURES:
    print("ArdIREC performance contract: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC performance contract: PASS")
