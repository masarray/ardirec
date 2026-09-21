#!/usr/bin/env python3
"""R6 workspace/Phasor release-quality contract state checker.

R6.0 records manual Windows acceptance blockers as explicit known failures while
keeping normal CI green. Each implementation milestone must promote only its own
contract entries to pass in the same PR that adds regression evidence.
R6.5 uses --require-release-ready as the strict gate.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Callable

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "scripts" / "r6_workspace_contract.json"


def read_text(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def compact(value: str) -> str:
    return " ".join(value.split())


def probe_lucide_child_chrome() -> bool:
    child = compact(read_text("apps/desktop/qml/MdiChildWindow.qml"))
    cmake = read_text("apps/desktop/CMakeLists.txt")
    runtime_path = ROOT / "tests" / "test_r6_workspace_runtime.cpp"
    runtime = runtime_path.read_text(encoding="utf-8") if runtime_path.is_file() else ""
    glyph_free = all(token not in child for token in (
        'text: "—"', 'text: "□"', 'text: "❐"', 'text: "×"'
    ))
    canonical_icons = all(token in child for token in (
        'icons/minus.svg',
        'icons/maximize-2.svg',
        'icons/copy.svg',
        'icons/x.svg',
    ))
    packaged = all(token in cmake for token in (
        'qml/icons/minus.svg',
        'qml/icons/maximize-2.svg',
        'qml/icons/copy.svg',
        'qml/icons/x.svg',
    ))
    coherent_controls = all(token in child for token in (
        "component WindowChromeButton",
        "Layout.preferredWidth: 28",
        "Layout.preferredHeight: 26",
        "control.hovered",
        "control.down",
        "root.activeWindow",
    ))
    regression = (
        "ardirec_r6_workspace_runtime_tests" in read_text("tests/CMakeLists.txt")
        and "Lucide child chrome keeps stable hit targets" in runtime
        and "maximized child uses restore icon" in runtime
        and "minimized child preserves existing chrome visibility semantics" in runtime
    )
    return glyph_free and canonical_icons and packaged and coherent_controls and regression


def probe_tile_shared_edge_resize() -> bool:
    workspace = compact(read_text("apps/desktop/qml/MdiWorkspace.qml"))
    cmake = read_text("tests/CMakeLists.txt")
    runtime_path = ROOT / "tests" / "test_r6_workspace_runtime.cpp"
    runtime = runtime_path.read_text(encoding="utf-8") if runtime_path.is_file() else ""
    managed_topology = all(token in workspace for token in (
        "property string arrangementMode",
        "function resizeTileBoundary",
        "tileBoundary",
    ))
    regression = (
        "ardirec_r6_workspace_runtime_tests" in cmake
        and "shared vertical tile boundary resizes both neighbors" in runtime
        and "shared horizontal tile boundary resizes both neighbors" in runtime
        and "tile resize preserves the occupied workspace extent" in runtime
    )
    return managed_topology and regression


def probe_virtual_workspace_scroll() -> bool:
    workspace = compact(read_text("apps/desktop/qml/MdiWorkspace.qml"))
    cmake = read_text("tests/CMakeLists.txt")
    runtime_path = ROOT / "tests" / "test_r6_workspace_runtime.cpp"
    runtime = runtime_path.read_text(encoding="utf-8") if runtime_path.is_file() else ""
    scroll_surface = all(token in workspace for token in (
        "Flickable {",
        "contentWidth:",
        "contentHeight:",
        "ScrollBar.horizontal:",
        "ScrollBar.vertical:",
    ))
    regression = (
        "ardirec_r6_workspace_runtime_tests" in cmake
        and "moving a child beyond the viewport expands logical workspace width" in runtime
        and "moving a child beyond the viewport expands logical workspace height" in runtime
        and "workspace exposes scroll range instead of clamping the child" in runtime
    )
    return scroll_surface and regression


def probe_free_drag_direct_manipulation() -> bool:
    child = compact(read_text("apps/desktop/qml/MdiChildWindow.qml"))
    cmake = read_text("tests/CMakeLists.txt")
    runtime_path = ROOT / "tests" / "test_r6_workspace_runtime.cpp"
    runtime = runtime_path.read_text(encoding="utf-8") if runtime_path.is_file() else ""
    no_viewport_clamp = (
        "root.workspaceWidth - root.width" not in child
        and "root.workspaceHeight - root.height" not in child
    )
    regression = (
        "ardirec_r6_workspace_runtime_tests" in cmake
        and "free drag follows every pointer update without viewport clamp" in runtime
        and "edge drag advances workspace scroll while preserving pointer-relative position" in runtime
    )
    return no_viewport_clamp and regression


def probe_phasor_latency_budget() -> bool:
    cmake = read_text("tests/CMakeLists.txt")
    perf_path = ROOT / "tests" / "test_r6_phasor_performance.cpp"
    perf = perf_path.read_text(encoding="utf-8") if perf_path.is_file() else ""
    phasor_view = compact(read_text("apps/desktop/qml/PhasorView.qml"))
    sequence = compact(read_text("apps/desktop/qml/SequenceSummary.qml"))
    controller = compact(read_text("apps/desktop/cursor_snapshot_controller.cpp"))
    bounded_ui = all(token in phasor_view for token in (
        "property bool bodyActivated",
        "readonly property bool nearViewport",
        "asynchronous: true",
        "groupCard.nearViewport ? root.displaySnapshotA",
        "groupCard.nearViewport ? root.displaySnapshotB",
    ))
    single_request_authority = (
        "requestCursorA" not in sequence
        and "identical one-cycle DFT that is already in flight" in controller
    )
    return (
        "ardirec_r6_phasor_performance_tests" in cmake
        and "first-open latency budget" in perf
        and "cursor scrub latency budget" in perf
        and "std::chrono::steady_clock" in perf
        and "committed-frame continuity remains valid" in perf
        and bounded_ui
        and single_request_authority
    )


PROBES: dict[str, Callable[[], bool]] = {
    "lucide_child_chrome": probe_lucide_child_chrome,
    "tile_shared_edge_resize": probe_tile_shared_edge_resize,
    "virtual_workspace_scroll": probe_virtual_workspace_scroll,
    "free_drag_direct_manipulation": probe_free_drag_direct_manipulation,
    "phasor_latency_budget": probe_phasor_latency_budget,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-release-ready",
        action="store_true",
        help="fail unless every R6 blocker is declared and observed as pass",
    )
    args = parser.parse_args()

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    failures: list[str] = []

    if manifest.get("schema") != 1:
        failures.append("unsupported R6 contract schema")

    baseline = manifest.get("baseline", {})
    if baseline.get("main_commit") != "af54b9122928c2e6cc03243b95ba93aed2e93087":
        failures.append("R6.0 main baseline commit changed unexpectedly")
    if baseline.get("observed_r5_candidate_pr") != 90:
        failures.append("R6.0 must retain the manually observed R5 candidate PR")
    if baseline.get("observed_r5_candidate_head") != "88202432e376eef0be739df26fe250e7f4935f27":
        failures.append("R6.0 manually observed candidate head changed unexpectedly")
    if baseline.get("rc_qualified") is not False:
        failures.append("manual R6 blockers mean the observed R5 candidate must remain not RC-qualified")

    contracts = manifest.get("contracts", {})
    if set(contracts) != set(PROBES):
        missing = sorted(set(PROBES) - set(contracts))
        extra = sorted(set(contracts) - set(PROBES))
        failures.append(f"manifest/probe mismatch: missing={missing}, extra={extra}")

    print("R6 workspace / Phasor release-blocker contracts")
    remaining: list[str] = []
    for name, probe in PROBES.items():
        spec = contracts.get(name, {})
        expected = spec.get("expected")
        if expected not in {"known_fail", "pass"}:
            failures.append(f"{name}: invalid expected state {expected!r}")
            continue
        try:
            passed = bool(probe())
        except Exception as exc:
            failures.append(f"{name}: probe error: {exc}")
            print(f"  FAIL {name}: probe error: {exc}")
            continue

        observed = "pass" if passed else "known_fail"
        stage = spec.get("stage", "?")
        issue = spec.get("issue", "?")
        marker = "PASS" if observed == expected else "FAIL"
        print(
            f"  {marker} {name}: expected={expected}, observed={observed}, "
            f"owner={stage}/#{issue}"
        )
        if observed != expected:
            failures.append(
                f"{name}: observed {observed} but manifest expects {expected}; "
                "implementation and contract state must change together"
            )
        if expected != "pass" or observed != "pass":
            remaining.append(name)

    if args.require_release_ready and remaining:
        failures.append(
            "strict R6 gate rejected remaining blockers: " + ", ".join(remaining)
        )

    if failures:
        print("\nR6 CONTRACT CHECK: FAIL")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    if remaining:
        print("\nR6 CONTRACT CHECK: STATE MATCHES MANIFEST; NOT RELEASE-READY")
        print("Remaining blockers: " + ", ".join(remaining))
    else:
        print("\nR6 CONTRACT CHECK: RELEASE CONTRACTS READY")
    return 0


if __name__ == "__main__":
    sys.exit(main())
