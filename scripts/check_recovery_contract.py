#!/usr/bin/env python3
"""Guard ArdIREC investigation-correctness regressions found during recovery.

This complements the performance contract. These checks are intentionally structural:
if an implementation changes, the replacement must preserve an equivalent or stronger
correctness invariant and this guard must be updated in the same reviewed change.
"""
from __future__ import annotations

from pathlib import Path
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


# R1.1 Phasor C1/C2 isolation: cursor B must never alter the radial transform
# used to draw cursor A. The scale is based on immutable record/channel context.
phasor = text("apps/desktop/qml/PhasorView.qml")
require("apps/desktop/qml/PhasorView.qml", "function stableScale(channels)",
        "phasor comparison requires a cursor-independent engineering scale")
require("apps/desktop/qml/PhasorView.qml", "root.document.channelPeak(index)",
        "stable phasor scale must use record/channel context rather than C1/C2 snapshots")
forbid("apps/desktop/qml/PhasorView.qml", "function sharedScale(",
       "C1/C2-dependent radial scaling makes a fixed C1 vector appear to move when only C2 changes")
stable_scale = function_body(phasor, "function stableScale(channels)")
if not stable_scale:
    FAILURES.append("apps/desktop/qml/PhasorView.qml: stableScale() body not found")
else:
    for token in ("displaySnapshotA", "displaySnapshotB", "snapshotRow("):
        if token in stable_scale:
            FAILURES.append(
                f"apps/desktop/qml/PhasorView.qml: {token!r} inside stableScale() — radial transform must be cursor-independent"
            )

# R1.2 Cursor dimension: delta-time is engineering information. It must remain
# visible even when the two cursor lines are closer than the label width.
require("apps/desktop/qml/EventStrip.qml", "readonly property bool labelInside", "narrow-span dimension needs inside/outside placement")
require("apps/desktop/qml/EventStrip.qml", "readonly property bool outsideRight", "narrow-span label must choose a free side")
require("apps/desktop/qml/EventStrip.qml", "readonly property real clampedLabelX", "outside labels must stay inside the visible plot")
require("apps/desktop/qml/EventStrip.qml", 'root.deltaMs.toFixed(3) + " ms"', "millisecond delta must always be represented")
forbid("apps/desktop/qml/EventStrip.qml", "&& span >= 18", "small cursor spans must not hide the dimension")
forbid("apps/desktop/qml/EventStrip.qml", "visible: parent.width >= 86", "small cursor spans must not hide the millisecond label")

# R1.3 Locus display policy: the production numerical trajectory stays intact.
# Relevant Fit is an explicit display transform; Fit All must retain access to
# every finite point for forensic inspection.
require("apps/desktop/qml/LocusView.qml", 'property string fitMode: "relevant"',
        "professional locus view defaults to an investigation-relevant scale")
require("apps/desktop/qml/LocusView.qml", 'text:"Fit Relevant"', "operator must see/select the relevant-fit policy")
require("apps/desktop/qml/LocusView.qml", 'text:"Fit All"', "operator must retain full finite-trajectory inspection")
require("apps/desktop/qml/LocusView.qml", 'if (fitMode === "all")', "Fit All must be a distinct explicit code path")
require("apps/desktop/qml/LocusView.qml", "locusSnapshotController.maxAbsR", "Fit All must use the complete finite native locus extent")
require("apps/desktop/qml/LocusView.qml", "cursorExtent(loops)", "Relevant Fit must follow committed engineering cursor context")
require("apps/desktop/qml/LocusView.qml", "LocusTrajectoryItem", "locus trajectory must remain on the native retained renderer")

# R1.4 SIGRA parity: production locus semantics must use COMTRADE metadata,
# measured residual current when trustworthy, and explicit event-window gaps.
require("apps/desktop/locus_snapshot_controller.cpp", "m_document->channelPhase(index)",
        "Locus channel binding must honor cached COMTRADE phase metadata before name fallback")
require("apps/desktop/locus_snapshot_controller.cpp", "residual_to_sum_multiplier",
        "dedicated IE/3I0 channels require an explicit reference-direction conversion")
require("apps/desktop/locus_snapshot_controller.cpp", "source->residualCurrent",
        "earth loops must be able to use a measured residual/earth-current channel")
require("apps/desktop/locus_snapshot_controller.cpp", "window_has_status_change",
        "a fault/trip/status change inside the one-cycle window must create an invalid gap")
require("apps/desktop/locus_snapshot_controller.cpp", "m_document->digitalEdgeTimes()",
        "Locus worker must consume immutable document event timing rather than infer events visually")
require("apps/desktop/locus_snapshot_controller.cpp", "kMaximumAnalysisPoints = 65536u",
        "analysis fidelity must be separated from the bounded native render budget")
require("apps/desktop/locus_snapshot_controller.cpp", "simplify_bounded",
        "high-resolution numerical loci must be shape-simplified before native rendering, not time-stride aliased")
require("core/include/ardirec/distance/distance.hpp", "residual_current",
        "distance core must accept a measured 3I0 residual with phase-sum fallback")

# Production-path numerical regression fixtures: the async native locus engine,
# not only the compatibility AnalysisController API, must be tested against both
# the 100-ohm opening case and SIGRA-specific measured-IE/status-window semantics.
require("tests/test_locus_snapshot.cpp", "distance_p1.cfg", "async locus production path requires the original deterministic fixture")
require("tests/test_locus_snapshot.cpp", "100.0", "golden energized impedance must stay locked")
require("tests/test_locus_snapshot.cpp", "post-open", "low-current invalid/gap semantics must stay locked")
require("tests/test_locus_snapshot.cpp", "distance_sigra_parity.cfg",
        "SIGRA parity fixture must exercise metadata, measured IE, and event-window validity")
require("tests/test_locus_snapshot.cpp", "statusRejectedCount",
        "SIGRA status-window rejection must remain observable in the native snapshot")
require("tests/CMakeLists.txt", "ardirec_locus_snapshot_tests", "golden production-path locus test must run under CTest")

if FAILURES:
    print("ArdIREC recovery correctness contract: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC recovery correctness contract: PASS")
