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

# R1 cursor dimension: delta-time is engineering information. It must remain
# visible even when the two cursor lines are closer than the label width.
require("apps/desktop/qml/EventStrip.qml", "readonly property bool labelInside", "narrow-span dimension needs inside/outside placement")
require("apps/desktop/qml/EventStrip.qml", "readonly property bool outsideRight", "narrow-span label must choose a free side")
require("apps/desktop/qml/EventStrip.qml", "readonly property real clampedLabelX", "outside labels must stay inside the visible plot")
require("apps/desktop/qml/EventStrip.qml", 'root.deltaMs.toFixed(3) + " ms"', "millisecond delta must always be represented")
forbid("apps/desktop/qml/EventStrip.qml", "&& span >= 18", "small cursor spans must not hide the dimension")
forbid("apps/desktop/qml/EventStrip.qml", "visible: parent.width >= 86", "small cursor spans must not hide the millisecond label")

# R1 Locus display policy: Earth and phase-phase panels are independent engineering
# diagrams. Relevant Fit follows robust valid trajectory context plus zones/cursors;
# Fit All retains each family's complete finite extent for forensic inspection.
require("apps/desktop/qml/LocusView.qml", 'property string fitMode: "relevant"',
        "professional locus view defaults to an investigation-relevant scale")
require("apps/desktop/qml/LocusView.qml", 'text:"Fit Relevant"', "operator must see/select the relevant-fit policy")
require("apps/desktop/qml/LocusView.qml", 'text:"Fit All"', "operator must retain full finite-trajectory inspection")
require("apps/desktop/qml/LocusView.qml", 'if (fitMode === "all")', "Fit All must be a distinct explicit code path")
require("apps/desktop/qml/LocusView.qml", "locusSnapshotController.earthMaxAbsR",
        "earth-loop Fit All must use its own complete native extent")
require("apps/desktop/qml/LocusView.qml", "locusSnapshotController.phaseMaxAbsR",
        "phase-loop Fit All must use its own complete native extent")
require("apps/desktop/qml/LocusView.qml", "locusSnapshotController.earthRelevantMaxAbsR",
        "earth Relevant Fit must include robust valid trajectory extent")
require("apps/desktop/qml/LocusView.qml", "locusSnapshotController.phaseRelevantMaxAbsR",
        "phase Relevant Fit must include robust valid trajectory extent")
require("apps/desktop/qml/LocusView.qml", "cursorExtent(loops)", "Relevant Fit must retain committed engineering cursor context")
require("apps/desktop/qml/LocusView.qml", "LocusTrajectoryItem", "locus trajectory must remain on the native retained renderer")
require("apps/desktop/qml/LocusView.qml", "CLASSICAL RIO · 1-CYCLE BACKWARD",
        "the UI must identify direct classical RIO compensation instead of displaying a synthetic kL")
require("apps/desktop/qml/LocusView.qml", "locusSnapshotController.statusRejectedCount",
        "measurement-window gaps should remain observable without per-point QML objects")
require("apps/desktop/qml/LocusView.qml", "Math.min(plotW / (2 * target.r), plotH / (2 * target.x))",
        "R and X must share one engineering pixels-per-ohm scale so locus angles/zones are not distorted")
require("apps/desktop/qml/LocusView.qml", "Math.min(4096",
        "screen-driven trajectory requests must remain bounded by the native 4096-point geometry budget")
forbid("apps/desktop/qml/LocusView.qml", "Repeater {\n                    model: locusSnapshotController.locus",
       "production locus rendering must not materialize one QML delegate per trajectory point")

# R1.1 SIGRA parity: production locus semantics must use COMTRADE metadata,
# measured residual current, trailing one-cycle validity, and direct classical
# RE/RL-XE/XL compensation when that is what the RIO file supplies.
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
require("apps/desktop/locus_snapshot_controller.cpp", "read_classical_grounding_factors",
        "the RIO classical factors must be preserved as independent RE/RL and XE/XL values")
require("apps/desktop/locus_snapshot_controller.cpp", "distance_impedance_rerl_xexl",
        "earth-loop production calculations must support direct SIGRA classical compensation")
forbid("apps/desktop/locus_snapshot_controller.cpp", "grounding_factor_from_rerl_xexl",
       "production locus must not synthesize complex kL from RE/RL-XE/XL using an assumed line angle")
require("apps/desktop/locus_snapshot_controller.cpp", "kMaximumAnalysisPoints = 65536u",
        "analysis fidelity must be separated from the bounded native render budget")
require("apps/desktop/locus_snapshot_controller.cpp", "simplify_bounded",
        "high-resolution numerical loci must be shape-simplified before native rendering, not time-stride aliased")
require("core/include/ardirec/distance/distance.hpp", "residual_current",
        "distance core must accept a measured 3I0 residual with phase-sum fallback")
require("core/include/ardirec/distance/distance.hpp", "distance_impedance_rerl_xexl",
        "distance core must expose the direct classical RE/RL-XE/XL solver")

# C1/C2 markers and numeric R/X must share exactly the same distance semantics as
# the trajectory: phase metadata, measured residual, classical RIO and window validity.
require("apps/desktop/cursor_snapshot_controller.cpp", "m_document->channelPhase(index)",
        "cursor snapshots must honor cached COMTRADE phase metadata")
require("apps/desktop/cursor_snapshot_controller.cpp", "m_document->digitalEdgeTimes()",
        "cursor distance values must observe the same event-window validity rule")
require("apps/desktop/cursor_snapshot_controller.cpp", "residual_to_sum_multiplier",
        "cursor distance values must understand IE/3I0 reference direction")
require("apps/desktop/cursor_snapshot_controller.cpp", 'QStringLiteral("statusWindowValid")',
        "cursor snapshot must carry explicit measurement-window validity")
require("apps/desktop/cursor_snapshot_controller.cpp", "distance_impedance_rerl_xexl",
        "cursor earth-loop calculation must use the same classical RIO solver as the trajectory")
forbid("apps/desktop/cursor_snapshot_controller.cpp", "grounding_factor_from_rerl_xexl",
       "cursor distance must not synthesize kL from classical RIO ratios")

# Production-path numerical regression fixtures keep both prior and new contracts locked.
require("tests/test_locus_snapshot.cpp", "distance_p1.cfg", "async locus production path requires the original deterministic fixture")
require("tests/test_locus_snapshot.cpp", "100.0", "golden energized impedance must stay locked")
require("tests/test_locus_snapshot.cpp", "post-open", "low-current invalid/gap semantics must stay locked")
require("tests/test_locus_snapshot.cpp", "distance_sigra_parity.cfg",
        "SIGRA parity fixture must exercise metadata, measured IE, classical RIO and event-window validity")
require("tests/test_locus_snapshot.cpp", "classicalGroundingValid",
        "production-path test must prove the matching RIO sidecar selects classical compensation")
require("tests/test_locus_snapshot.cpp", "statusRejectedCount",
        "SIGRA status-window rejection must remain observable in the native snapshot")
require("tests/data/distance_sigra_parity.rio", "RE/RL", "SIGRA parity fixture must carry the resistance compensation ratio")
require("tests/data/distance_sigra_parity.rio", "XE/XL", "SIGRA parity fixture must carry the reactance compensation ratio")
require("tests/CMakeLists.txt", "ardirec_locus_snapshot_tests", "golden production-path locus test must run under CTest")

# R1.2 frequency + multi-rate DFT: the calculation frequency is estimated once in
# the background loader, then shared by Cursor and Locus. Full-cycle integration is
# weighted by actual COMTRADE timestamps, not by sample count.
require("core/include/ardirec/power/timestamped_dft.hpp", "trailing_cycle_window",
        "cursor and Locus require one shared causal timestamp-window definition")
require("core/include/ardirec/power/timestamped_dft.hpp", "timestamp_cell_weight",
        "multi-rate full-cycle DFT requires actual-time quadrature weights")
require("apps/desktop/document_loader.cpp", "kMaximumFrequencySamples = 8192u",
        "prefault frequency estimation must stay bounded during background load")
require("apps/desktop/document_loader.cpp", "PREFault estimated · voltage V1",
        "voltage positive-sequence must be the preferred prefault frequency source")
require("apps/desktop/document_loader.cpp", "PREFault estimated · current I1",
        "current positive-sequence must remain the fallback estimate source")
require("apps/desktop/document_loader.cpp", 'calculation_frequency_provenance = "COMTRADE nominal"',
        "frequency estimation must have an explicit COMTRADE nominal fallback")
require("apps/desktop/document_controller.hpp", "calculationFrequencyProvenance",
        "frequency provenance must be exposed once from the shared document state")
require("apps/desktop/cursor_snapshot_controller.cpp", "timestamp_cell_weight",
        "C1/C2 DFT must use timestamp weights across sample-rate boundaries")
require("apps/desktop/locus_snapshot_controller.cpp", "timestamp_cell_weight",
        "native Locus DFT must use the same timestamp weighting as C1/C2")
require("apps/desktop/cursor_snapshot_controller.cpp", "m_document->calculationFrequency()",
        "cursor snapshots must consume the shared document calculation frequency")
require("apps/desktop/locus_snapshot_controller.cpp", "m_document->calculationFrequency()",
        "Locus trajectories must consume the shared document calculation frequency")
forbid("apps/desktop/cursor_snapshot_controller.cpp", "std::sqrt(2.0L) / static_cast<long double>(counts",
       "cursor phasor normalization must use accumulated time weight, not sample count")
forbid("apps/desktop/locus_snapshot_controller.cpp", "std::sqrt(2.0L) / static_cast<long double>(count",
       "Locus phasor normalization must use accumulated time weight, not sample count")
require("apps/desktop/qml/WorkstationStatusBar.qml", '"fcalc " + root.document.calculationFrequency.toFixed(3)',
        "compact UX must expose active calculation frequency without adding a heavy panel")
require("apps/desktop/qml/WorkstationStatusBar.qml", "calculationFrequencyProvenance",
        "operator must be able to tell prefault estimate from nominal fallback")
require("tests/test_locus_snapshot.cpp", '"1000,91\\n"',
        "deterministic production test must declare the first COMTRADE sampling section")
require("tests/test_locus_snapshot.cpp", '"2000,271\\n"',
        "deterministic production test must cross into a second sampling section")
require("tests/test_locus_snapshot.cpp", "actualFrequency = 50.4",
        "fixture must distinguish estimated calculation frequency from 50 Hz nominal")
require("tests/test_locus_snapshot.cpp", "cursor.requestCursorA(0.100)",
        "cursor production path must be tested on a window crossing the rate boundary")
require("tests/test_locus_snapshot.cpp", "crossingRateBoundary",
        "native Locus production path must be tested on the same rate-boundary window")

# Global-data alignment must remain documented rather than hidden in implementation lore.
require("docs/LOCUS_ENGINE.md", "full-cycle DFT", "calculation window convention must stay explicit")
require("docs/LOCUS_ENGINE.md", "prefault positive-sequence", "frequency-source policy must stay explicit")
require("docs/LOCUS_ENGINE.md", "different sample-rate sections", "COMTRADE multi-rate qualification must stay explicit")
require("docs/LOCUS_ENGINE.md", "causal timestamp-cell quadrature", "multi-rate DFT weighting policy must stay explicit")
require("docs/LOCUS_ENGINE.md", "Correct impedance plane", "distance-loop and positive-sequence planes must not be conflated")
require("docs/LOCUS_ENGINE.md", "retained Qt Quick scene-graph geometry", "lightweight retained rendering is an architectural invariant")

if FAILURES:
    print("ArdIREC recovery correctness contract: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC recovery correctness contract: PASS")
