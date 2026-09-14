#!/usr/bin/env python3
"""Guard the R4 recovery Definition of Done.

R4 is intentionally stronger than a compile gate: executable qualification must
cover the user-visible regressions, while architecture guards keep the tests from
being satisfied by duplicating numerical engines or bypassing production QML.
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


def require_count(path: str, needle: str, expected: int, reason: str) -> None:
    count = text(path).count(needle)
    if count != expected:
        FAILURES.append(f"{path}: found {count} x {needle!r}, expected {expected} — {reason}")


def forbid(path: str, needle: str, reason: str) -> None:
    if needle in text(path):
        FAILURES.append(f"{path}: found forbidden {needle!r} — {reason}")


# 1. C1/C2 numeric + screen-transform isolation is executable, not screenshot-only.
require("tests/test_r4_cursor_isolation.cpp", "cursor.cursorA() == frozenA",
        "sweeping C2 must prove the complete committed C1 snapshot is immutable")
for token in ("frozenScreen.fraction", "frozenScreen.angleDegrees", "frozenScreen.x", "frozenScreen.y"):
    require("tests/test_r4_cursor_isolation.cpp", token,
            "C1 normalized screen transform must be frozen while C2 moves")
require("tests/test_r4_cursor_isolation.cpp", "distance_sigra_parity.cfg",
        "C1/C2 isolation must use a deterministic COMTRADE engineering fixture")

# 2. Locus parity/default fit remains explicit and production-path native.
require("tests/test_r4_locus_fit.cpp", "distance_p1.cfg",
        "R4 must keep a known golden protection record")
require("tests/test_r4_locus_fit.cpp", "snapshot->loops[0].at(22)",
        "known L1-E cursor position must be qualified")
require("tests/test_r4_locus_fit.cpp", "100.0, 0.2",
        "golden resistance tolerance must stay explicit")
require("tests/test_r4_locus_fit.cpp", "lowCurrentTail.valid == 0",
        "near-zero measuring current must remain an explicit gap")
require("tests/test_r4_locus_fit.cpp", "earthRelevantMaxAbsR",
        "default protection fit must be independently qualified")
require("tests/test_r4_locus_fit.cpp", "earthRelevantMaxAbsX",
        "default protection fit must reject invalid-sample domination in both axes")

# 3/4/6/7/8. Headless production-QML runtime coverage.
runtime = "tests/test_r4_qml_runtime.cpp"
require(runtime, "MdiWorkspace.qml", "MDI runtime test must instantiate production workspace QML")
require(runtime, "AnalysisHostInterceptor", "heavy content may be stubbed only below the production MDI layer")
for token in (
    'openView", QStringLiteral("time"), true',
    'openView", QStringLiteral("phasor"), true',
    '"updateGeometry"',
    '"activateWindow"',
    '"minimizeWindow"',
    '"cascade"',
    '"tileHorizontal"',
    '"tileVertical"',
    '"closeWindow"',
):
    require(runtime, token, "MDI runtime semantics must be executed through production methods")
require(runtime, "all children share one document engine instance",
        "four-window qualification must prove shared document identity")
require(runtime, "all children share one analysis engine instance",
        "four-window qualification must prove shared analysis identity")
require(runtime, "all children share one locus engine instance",
        "four-window qualification must prove shared locus engine identity")
require(runtime, "all children share one harmonic snapshot engine instance",
        "four-window qualification must prove shared scalar engine identity")
require(runtime, "all children share one table snapshot engine instance",
        "four-window qualification must prove shared table engine identity")
require(runtime, "Tile operations never mutate analysis view type",
        "arrangement must preserve child analysis types at runtime")
require(runtime, "closed child delegate is destroyed",
        "closed MDI children must release their view-host lifetime")
require(runtime, "minimized child has no heavy analysis activity",
        "minimized children must be runtime-quiescent")

# Cursor dimension must exercise narrow and edge spans, with ms retained.
require(runtime, "EventStrip.qml", "cursor dimension qualification must instantiate production EventStrip")
require(runtime, "!dimension->property(\"labelInside\").toBool()",
        "narrow cursor spans must move the label outside the dimension span")
require(runtime, "narrow-span millisecond label remains visible",
        "milliseconds are primary and cannot disappear at narrow spans")
require(runtime, "dimension label is clamped in bounds near the right edge",
        "outside labels must remain inside the plot")

# About/Properties use the production TopBar and resize through several window sizes.
require(runtime, "TopBar.qml", "dialog qualification must instantiate production TopBar")
require(runtime, '"showAbout"', "About must be opened at runtime")
require(runtime, '"showProperties"', "Properties must be opened at runtime")
require(runtime, "application popup is horizontally centered on overlay",
        "dialogs must be horizontally centered at runtime")
require(runtime, "application popup is vertically centered on overlay",
        "dialogs must be vertically centered at runtime")
if runtime and text(runtime).count("QSize(") < 6:
    FAILURES.append(f"{runtime}: expected several runtime resize cases for About/Properties")

# Duplicate Phasor views share one global producer and hidden/minimized views are quiescent.
require(runtime, "PhasorView.qml", "request-owner qualification must instantiate production PhasorView")
require(runtime, "visible duplicate Phasor consumer does not launch heavy snapshot jobs without ownership",
        "duplicate consumers must not duplicate one-cycle DFT work")
require(runtime, "hidden/minimized Phasor view issues no heavy cursor snapshot jobs",
        "hidden/minimized Phasor views must remain quiescent")
require("apps/desktop/qml/PhasorView.qml", "property bool requestOwner: true",
        "production Phasor view keeps explicit request ownership")
require("apps/desktop/qml/PhasorView.qml", "if (!root.requestOwner || !root.document || !root.visible) return",
        "production request path must enforce owner+visibility quiescence")

# 5. Close lifetime must cover idle, load-active, and analysis-worker-active cases.
life = "tests/test_document_lifecycle.cpp"
require(life, "document.closeDocument()", "real DocumentController close must be exercised")
require(life, "cursor.requestCursorA", "close qualification must start cursor analysis")
require(life, "locus.request", "close qualification must start locus analysis")
require(life, "locusRevisionAfterClose", "close invalidation must be separated from stale publication")
require(life, "pump_events(350)", "cancelled callbacks must get an opportunity to arrive")
require(life, "stale pre-close locus callback cannot publish",
        "old document-generation worker results must be rejected")
for cache_assertion in ("dataStoreSnapshot() == nullptr", "analogLodPyramidSnapshot() == nullptr",
                        "rmsTileCacheSnapshot() == nullptr"):
    require(life, cache_assertion, "record-scoped resources must be released by close")
require(life, "harmonic.spectrumAt", "bounded harmonic cache must be tested across close")
require(life, "table.snapshotAt", "bounded table cache must be tested across close")

# CTest must execute every R4 qualification target, including headless Qt Quick.
cmake = "tests/CMakeLists.txt"
for target in ("ardirec_r4_cursor_isolation_tests", "ardirec_r4_locus_fit_tests", "ardirec_r4_qml_runtime_tests"):
    require(cmake, target, f"CTest must build and execute {target}")
require(cmake, 'QT_QPA_PLATFORM=offscreen', "QML runtime qualification must be deterministic/headless in CI")

# Performance/architecture: exactly one global record/numerical engine instance exists
# in production startup. MDI children receive references; they must not construct copies.
main = "apps/desktop/main.cpp"
for declaration in (
    "DocumentController document;",
    "AnalysisController analysis(&document);",
    "CursorSnapshotController cursorSnapshots(&document);",
    "LocusSnapshotController locusSnapshots(&document);",
    "HarmonicSnapshotController harmonicSnapshots(&document);",
    "TableSnapshotController tableSnapshots(&document);",
):
    require_count(main, declaration, 1, "production desktop startup must own exactly one shared engine instance")
for path in ("apps/desktop/qml/MdiWorkspace.qml", "apps/desktop/qml/MdiChildWindow.qml"):
    forbid(path, "QMdiArea", "R4 must preserve the pure Qt Quick scene-graph workspace")
    forbid(path, "QQuickWidget", "R4 must not disable the threaded Qt Quick render path")
require("apps/desktop/qml/MdiWorkspace.qml", 'live: childWindow.windowState !== "minimized"',
        "minimized MDI hosts must detach heavy inputs")

# Existing performance/large-record/package gates remain mandatory, and R4 itself
# becomes a required recovery-contract step.
require(".github/workflows/ci.yml", "python3 scripts/check_performance_contract.py",
        "performance architecture contract remains mandatory")
require(".github/workflows/ci.yml", "Exercise 10M-sample / ~210 MiB record",
        "10M-sample large-record qualification remains mandatory")
require(".github/workflows/ci.yml", "Exercise 120-channel / ~238 MiB record",
        "120-channel qualification remains mandatory")
require(".github/workflows/ci.yml", "python3 scripts/check_r4_definition_of_done.py",
        "R4 Definition of Done contract must run in CI")
require(".github/workflows/windows-build.yml", "Smoke test packaged application startup",
        "Windows packaged startup remains a release gate")

# Final recovery candidate identity must be unique and aligned with Windows artifacts.
require("apps/desktop/main.cpp", "0.2.0-alpha.22",
        "R4 qualification build must be distinguishable from R3")
require(".github/workflows/windows-build.yml", "ardirec-v0.2.0-alpha.22-windows-x64",
        "Windows staging identity must match the R4 executable")
require(".github/workflows/windows-build.yml", "ardirec-v0.2.0-alpha.22-windows-x64-portable.zip",
        "Windows portable ZIP identity must match R4")

# Durable DoD documentation maps every issue-62 requirement to executable evidence.
require("docs/RECOVERY_DEFINITION_OF_DONE.md", "R4 — Recovery Definition of Done",
        "final recovery acceptance must remain discoverable")
for heading in ("C1/C2 isolation", "Locus golden parity", "Cursor dimension", "Dialog placement",
                "Close lifecycle", "MDI semantics", "Quiescence", "Performance / shared engines"):
    require("docs/RECOVERY_DEFINITION_OF_DONE.md", heading,
            f"DoD documentation must preserve the {heading} acceptance item")

if FAILURES:
    print("ArdIREC R4 Definition of Done contract: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC R4 Definition of Done contract: PASS")
