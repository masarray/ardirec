#!/usr/bin/env python3
"""Guard ArdIREC recovery correctness invariants.

The checker is intentionally structural. It protects permanent investigation,
numerical, lifecycle-facing and rendering contracts without pinning a historical
QML implementation shape. When a later milestone strengthens an invariant, the
probe moves to the stronger production boundary in the same reviewed change.
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
                return source[brace + 1:index]
    return ""


# R1.1 Phasor C1/C2 isolation: cursor B must never alter cursor A's radial
# transform. Scale comes from immutable record/channel context.
phasor = text("apps/desktop/qml/PhasorView.qml")
require("apps/desktop/qml/PhasorView.qml", "function stableScale(channels)",
        "phasor comparison requires a cursor-independent engineering scale")
require("apps/desktop/qml/PhasorView.qml", "root.document.channelPeak(index)",
        "stable phasor scale must use record/channel context")
forbid("apps/desktop/qml/PhasorView.qml", "function sharedScale(",
       "C1/C2-dependent radial scaling is forbidden")
stable_scale = function_body(phasor, "function stableScale(channels)")
if not stable_scale:
    FAILURES.append("apps/desktop/qml/PhasorView.qml: stableScale() body not found")
else:
    for token in ("displaySnapshotA", "displaySnapshotB", "snapshotRow("):
        if token in stable_scale:
            FAILURES.append(
                f"apps/desktop/qml/PhasorView.qml: {token!r} inside stableScale() — radial transform must be cursor-independent"
            )


# R1 cursor dimension: delta-time remains visible even for narrow C1/C2 spans.
for needle, reason in (
    ("readonly property bool labelInside", "narrow-span dimension needs inside/outside placement"),
    ("readonly property bool outsideRight", "narrow-span label must choose a free side"),
    ("readonly property real clampedLabelX", "outside labels must stay inside the plot"),
    ('root.deltaMs.toFixed(3) + " ms"', "millisecond delta must always be represented"),
):
    require("apps/desktop/qml/EventStrip.qml", needle, reason)
forbid("apps/desktop/qml/EventStrip.qml", "&& span >= 18", "small cursor spans must not hide the dimension")
forbid("apps/desktop/qml/EventStrip.qml", "visible: parent.width >= 86", "small cursor spans must not hide the label")


# R1/R5.4 Locus display policy. The permanent invariants are independent earth
# and phase panels, native retained trajectory rendering, forensic Fit All, robust
# no-zone fallback, equal px/ohm axes, and a cursor-independent investigation
# viewport. R5.4 deliberately moved fit mathematics into LocusFitPolicy.qml.
locus = "apps/desktop/qml/LocusView.qml"
policy = "apps/desktop/qml/LocusFitPolicy.qml"
for needle, reason in (
    ('property string fitMode: "relevant"', "Locus must default to investigation-relevant fit"),
    ('text:"Fit Relevant"', "operator must be able to select relevant fit"),
    ('text:"Fit All"', "operator must retain complete finite-trajectory inspection"),
    ("LocusFitPolicy { id: fitPolicy }", "fit policy must live at an explicit testable boundary"),
    ("locusSnapshotController.earthMaxAbsR", "earth Fit All must retain its complete native extent"),
    ("locusSnapshotController.phaseMaxAbsR", "phase Fit All must retain its complete native extent"),
    ("locusSnapshotController.earthRelevantMaxAbsR", "earth no-zone fallback must retain robust extent"),
    ("locusSnapshotController.phaseRelevantMaxAbsR", "phase no-zone fallback must retain robust extent"),
    ("LocusTrajectoryItem", "trajectory must remain on the retained native renderer"),
    ("CLASSICAL RIO · 1-CYCLE BACKWARD", "UI must identify direct classical RIO compensation"),
    ("locusSnapshotController.statusRejectedCount", "measurement-window gaps must remain observable"),
    ("Math.min(4096", "trajectory geometry request budget must remain bounded"),
):
    require(locus, needle, reason)

for needle, reason in (
    ('if (fitMode === "all")', "Fit All must remain a distinct explicit forensic path"),
    ("const hasProtectionContext = zone.r > 0.0 || zone.x > 0.0", "loaded zones must define investigation context"),
    ("const baseR = hasProtectionContext ? zone.r : Number(relevantR)", "robust trajectory extent is the no-zone fallback"),
    ("const baseX = hasProtectionContext ? zone.x : Number(relevantX)", "robust trajectory extent is the no-zone fallback"),
    ("Math.min(Number(plotWidth) / (2 * safeR)", "R/X must share one engineering px-per-ohm scale"),
    ("Number(plotHeight) / (2 * safeX)", "R/X must share one engineering px-per-ohm scale"),
):
    require(policy, needle, reason)

fit_body = function_body(text(locus), "function distanceTarget(zones, fullR, fullX, relevantR, relevantX)")
if not fit_body:
    FAILURES.append(f"{locus}: distanceTarget() body not found")
else:
    for token in ("cursorExtent", "cursorAValues", "cursorBValues", "cursorATime", "cursorBTime"):
        if token in fit_body:
            FAILURES.append(
                f"{locus}: {token!r} inside distanceTarget() — R5.4 viewport must be cursor-independent"
            )
for token in ("cursorExtent", "cursorAValues", "cursorBValues", "cursorATime", "cursorBTime"):
    forbid(policy, token, "R5.4 fit policy must never depend on cursor position")
forbid(locus, "Repeater {\n                    model: locusSnapshotController.locus",
       "production locus rendering must not materialize one QML delegate per trajectory point")


# R1.1 SIGRA parity: production locus semantics use COMTRADE metadata, measured
# residual current, trailing one-cycle validity and direct classical RE/RL-XE/XL.
for needle, reason in (
    ("m_document->channelPhase(index)", "Locus channel binding must honor COMTRADE phase metadata"),
    ("residual_to_sum_multiplier", "IE/3I0 channels require explicit reference-direction conversion"),
    ("source->residualCurrent", "earth loops must support a measured residual channel"),
    ("window_has_status_change", "status changes inside a measurement window must create an invalid gap"),
    ("m_document->digitalEdgeTimes()", "Locus worker must consume immutable event timing"),
    ("read_classical_grounding_factors", "RIO RE/RL and XE/XL must remain independent"),
    ("distance_impedance_rerl_xexl", "production earth loops must support direct SIGRA classical compensation"),
    ("kMaximumAnalysisPoints = 65536u", "numerical analysis fidelity must remain separate from render budget"),
    ("simplify_bounded", "high-resolution loci must be shape-simplified before rendering"),
):
    require("apps/desktop/locus_snapshot_controller.cpp", needle, reason)
forbid("apps/desktop/locus_snapshot_controller.cpp", "grounding_factor_from_rerl_xexl",
       "production Locus must not synthesize complex kL from classical RIO ratios")
require("core/include/ardirec/distance/distance.hpp", "residual_current",
        "distance core must accept measured 3I0 with phase-sum fallback")
require("core/include/ardirec/distance/distance.hpp", "distance_impedance_rerl_xexl",
        "distance core must expose direct classical RE/RL-XE/XL solver")


# C1/C2 marker values share the same distance semantics as the trajectory.
for needle, reason in (
    ("m_document->channelPhase(index)", "cursor snapshots must honor COMTRADE phase metadata"),
    ("m_document->digitalEdgeTimes()", "cursor distance must observe event-window validity"),
    ("residual_to_sum_multiplier", "cursor distance must understand IE/3I0 direction"),
    ('QStringLiteral("statusWindowValid")', "cursor snapshot must carry explicit measurement-window validity"),
    ("distance_impedance_rerl_xexl", "cursor earth loop must use the same classical solver as trajectory"),
):
    require("apps/desktop/cursor_snapshot_controller.cpp", needle, reason)
forbid("apps/desktop/cursor_snapshot_controller.cpp", "grounding_factor_from_rerl_xexl",
       "cursor distance must not synthesize kL from classical RIO ratios")


# Production-path numerical regression fixtures keep previous qualified semantics.
for needle, reason in (
    ("distance_p1.cfg", "async production path requires deterministic fixture"),
    ("100.0", "golden energized impedance must stay locked"),
    ("post-open", "low-current invalid/gap semantics must stay locked"),
    ("distance_sigra_parity.cfg", "SIGRA fixture must exercise metadata/residual/classical/event validity"),
    ("classicalGroundingValid", "matching RIO sidecar must select classical compensation"),
    ("statusRejectedCount", "SIGRA status-window rejection must stay observable"),
):
    require("tests/test_locus_snapshot.cpp", needle, reason)
require("tests/data/distance_sigra_parity.rio", "RE/RL", "SIGRA fixture must carry resistance compensation ratio")
require("tests/data/distance_sigra_parity.rio", "XE/XL", "SIGRA fixture must carry reactance compensation ratio")
require("tests/CMakeLists.txt", "ardirec_locus_snapshot_tests", "production-path Locus golden must run under CTest")
require("tests/R5LocusParity.cmake", "ardirec_r5_locus_parity_tests", "R5.4 SIGRA viewport/full-cycle qualification must run under CTest")


# R1.2/R5.4 frequency + multi-rate DFT: calculation frequency is estimated once
# in the loader and shared. A value is valid only with a complete backward cycle;
# integration is weighted by actual COMTRADE timestamps, not sample count.
for needle, reason in (
    ("trailing_cycle_window", "Cursor and Locus require one shared causal timestamp window"),
    ("timestamp_cell_weight", "multi-rate DFT requires actual-time quadrature weights"),
    ("if (start < times.front() - tolerance) return window;", "partial beginning-of-record cycles must be rejected"),
    ("finish - start < period - tolerance", "valid windows must span one complete backward period"),
):
    require("core/include/ardirec/power/timestamped_dft.hpp", needle, reason)
for needle, reason in (
    ("kMaximumFrequencySamples = 8192u", "prefault frequency estimation must remain bounded"),
    ("PREFault estimated · voltage V1", "positive-sequence voltage remains preferred frequency source"),
    ("PREFault estimated · current I1", "positive-sequence current remains fallback source"),
    ('calculation_frequency_provenance = "COMTRADE nominal"', "frequency estimation needs explicit nominal fallback"),
):
    require("apps/desktop/document_loader.cpp", needle, reason)
require("apps/desktop/document_controller.hpp", "calculationFrequencyProvenance",
        "frequency provenance must be exposed from shared document state")
for path in ("apps/desktop/cursor_snapshot_controller.cpp", "apps/desktop/locus_snapshot_controller.cpp"):
    require(path, "timestamp_cell_weight", "Cursor and Locus must use timestamp weights across sample-rate boundaries")
    require(path, "m_document->calculationFrequency()", "Cursor and Locus must consume shared calculation frequency")
forbid("apps/desktop/cursor_snapshot_controller.cpp", "std::sqrt(2.0L) / static_cast<long double>(counts",
       "cursor phasor normalization must use accumulated time weight")
forbid("apps/desktop/locus_snapshot_controller.cpp", "std::sqrt(2.0L) / static_cast<long double>(count",
       "Locus phasor normalization must use accumulated time weight")
require("apps/desktop/qml/WorkstationStatusBar.qml", '"fcalc " + root.document.calculationFrequency.toFixed(3)',
        "UX must expose active calculation frequency")
require("apps/desktop/qml/WorkstationStatusBar.qml", "calculationFrequencyProvenance",
        "operator must distinguish estimate from nominal fallback")
for needle, reason in (
    ('"1000,91\\n"', "fixture must declare first sampling section"),
    ('"2000,271\\n"', "fixture must cross into second sampling section"),
    ("actualFrequency = 50.4", "fixture must distinguish estimate from nominal 50 Hz"),
    ("cursor.requestCursorA(0.100)", "cursor production path must cross rate boundary"),
    ("crossingRateBoundary", "native Locus must exercise the same rate-boundary window"),
):
    require("tests/test_locus_snapshot.cpp", needle, reason)


# Global-data alignment remains documented rather than hidden in implementation lore.
for needle, reason in (
    ("full-cycle DFT", "calculation window convention must stay explicit"),
    ("prefault positive-sequence", "frequency-source policy must stay explicit"),
    ("different sample-rate sections", "COMTRADE multi-rate qualification must stay explicit"),
    ("causal timestamp-cell quadrature", "multi-rate DFT weighting policy must stay explicit"),
    ("Correct impedance plane", "distance-loop and positive-sequence planes must not be conflated"),
    ("retained Qt Quick scene-graph geometry", "retained rendering is an architectural invariant"),
):
    require("docs/LOCUS_ENGINE.md", needle, reason)


if FAILURES:
    print("ArdIREC recovery correctness contract: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC recovery correctness contract: PASS")
