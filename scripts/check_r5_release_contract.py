#!/usr/bin/env python3
"""R5 release-quality contract state checker.

R5.0 intentionally records the alpha.22 release blockers as known failures while
keeping normal CI green. Each implementation stage must change its own manifest
entry to ``pass`` in the same review that fixes the behavior. ``--require-release-ready``
is the strict R5.5 gate and rejects every remaining known failure.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Callable

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "scripts" / "r5_release_contract.json"


def read_text(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def compact(text: str) -> str:
    return " ".join(text.split())


def git_blob_sha(relative: str) -> str:
    result = subprocess.run(
        ["git", "hash-object", relative],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"git hash-object failed for {relative}: {result.stderr.strip()}"
        )
    return result.stdout.strip()


def probe_linked_local_cursors() -> bool:
    main = compact(read_text("apps/desktop/qml/Main.qml"))
    host = compact(read_text("apps/desktop/qml/AnalysisViewHost.qml"))
    workspace = compact(read_text("apps/desktop/qml/MdiWorkspace.qml"))
    cmake = read_text("tests/CMakeLists.txt")
    runtime = read_text("tests/test_r5_linked_cursors.cpp")

    # R5.1 deliberately places the controls in AnalysisViewHost rather than in
    # each heavy analysis component. AnalysisViewHost is instantiated once per
    # MDI child, so the presentation is child-local while the state remains
    # linked through MdiWorkspace -> Main. This also keeps cursor plumbing out of
    # Phasor/Table/Locus calculation components.
    no_global_substitute = (
        "CursorNavigator {" not in main
        and "HarmonicCursorNavigator {" not in main
    )
    dual_local = (
        'readonly property bool dualCursorView: viewType === "time" || viewType === "phasor" || viewType === "locus"'
        in host
        and 'objectName: "localDualCursorNavigator"' in host
        and "CursorNavigator {" in host
        and "onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)" in host
        and "onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)" in host
    )
    single_local = (
        'readonly property bool singleCursorView: viewType === "harmonics" || viewType === "table"'
        in host
        and 'objectName: "localSingleCursorNavigator"' in host
        and "HarmonicCursorNavigator {" in host
        and "onCursorRequested: timeSeconds => root.cursorARequested(timeSeconds)" in host
    )
    shared_link = all(
        token in workspace
        for token in (
            "cursorATime: root.cursorATime",
            "cursorBTime: root.cursorBTime",
            "onCursorARequested: timeSeconds => root.cursorARequested(timeSeconds)",
            "onCursorBRequested: timeSeconds => root.cursorBRequested(timeSeconds)",
        )
    ) and all(
        token in main
        for token in (
            "cursorATime: window.cursorATime",
            "cursorBTime: window.cursorBTime",
            "onCursorARequested: timeSeconds => window.cursorATime = timeSeconds",
            "onCursorBRequested: timeSeconds => window.cursorBTime = timeSeconds",
        )
    )
    runtime_regression = (
        "ardirec_r5_linked_cursor_tests" in cmake
        and "localCursorCount" in runtime
        and "shared C1 update fans out" in runtime
        and "shared C2 update fans out" in runtime
        and "Harmonics/Table remain C1-only" in runtime
    )
    return (
        no_global_substitute
        and dual_local
        and single_local
        and shared_link
        and runtime_regression
    )


def probe_phasor_committed_frame() -> bool:
    text = compact(read_text("apps/desktop/qml/PhasorView.qml"))
    cmake = read_text("tests/CMakeLists.txt")
    runtime = read_text("tests/test_r5_phasor_zero_flicker.cpp")
    clears_a = (
        "displaySnapshotA: snapshotMatches(snapshotA, cursorATime) ? snapshotA : ({valid:false})"
        in text
    )
    clears_b = (
        "displaySnapshotB: snapshotMatches(snapshotB, cursorBTime) ? snapshotB : ({valid:false})"
        in text
    )
    committed_display = (
        "displaySnapshotA: snapshotA && snapshotA.valid ? snapshotA : ({valid:false})" in text
        and "displaySnapshotB: snapshotB && snapshotB.valid ? snapshotB : ({valid:false})" in text
    )
    bounded_latest = all(
        token in text
        for token in (
            "property bool cursorARequestQueued: false",
            "property bool cursorBRequestQueued: false",
            "if (cursorSnapshotController.busyA) { root.cursorARequestQueued = true return }",
            "if (cursorSnapshotController.busyB) { root.cursorBRequestQueued = true return }",
            "root.queueCursorA(root.pendingCursorATime)",
            "root.queueCursorB(root.pendingCursorBTime)",
        )
    )
    runtime_regression = (
        "ardirec_r5_phasor_zero_flicker_tests" in cmake
        and "committed C1 frame remains valid while a newer calculation is pending" in runtime
        and "continuous scrub coalesces intermediate C1 positions" in runtime
        and "latest queued scrub target wins after bounded revalidation" in runtime
    )
    return (
        not clears_a
        and not clears_b
        and committed_display
        and bounded_latest
        and runtime_regression
    )


def probe_phasor_retained_qsg() -> bool:
    text = read_text("apps/desktop/phasor_vector_item.cpp")
    cmake = read_text("tests/CMakeLists.txt")
    runtime = read_text("tests/test_r5_phasor_zero_flicker.cpp")
    retained = (
        "delete oldNode;" not in text
        and "oldNode ? static_cast<VectorRootNode*>(oldNode) : new VectorRootNode()" in text
        and "node->markDirty(QSGNode::DirtyGeometry);" in text
        and "root->appendChildNode(new VectorNode());" in text
    )
    runtime_regression = (
        "ardirec_r5_phasor_zero_flicker_tests" in cmake
        and "secondRoot == firstRoot" in runtime
        and "secondRoot->firstChild() == firstVectorNode" in runtime
        and "growing vector count preserves already-existing vector geometry" in runtime
    )
    return retained and runtime_regression


def probe_table_async_atomic_frame() -> bool:
    text = read_text("apps/desktop/qml/ValueTableView.qml")
    forbidden = (
        "snapshot.sortedChannels(",
        "snapshot.summaryAt(",
        "snapshot.snapshotAt(",
    )
    return not any(token in text for token in forbidden)


def probe_table_stable_geometry() -> bool:
    text = compact(read_text("apps/desktop/qml/ValueTableView.qml"))
    return "Layout.preferredHeight: hasData ? 92 : 0" not in text


def probe_locus_cursor_independent_viewport() -> bool:
    text = compact(read_text("apps/desktop/qml/LocusView.qml"))
    cursor_in_target = (
        "const cursor = cursorExtent(loops)" in text
        and ("cursor.r" in text or "cursor.x" in text)
    )
    return not cursor_in_target


def probe_full_backward_cycle() -> bool:
    text = compact(read_text("core/include/ardirec/power/timestamped_dft.hpp"))
    # alpha.22 clips the requested one-cycle start to times.front(), which permits
    # a partial beginning-of-record window. R5.4 must replace this with an explicit
    # complete-cycle qualification and may deliberately refine this probe then.
    partial_window_signature = (
        "const double start = std::max(times.front(), finish - period);" in text
    )
    return not partial_window_signature


PROBES: dict[str, Callable[[], bool]] = {
    "linked_local_cursors": probe_linked_local_cursors,
    "phasor_committed_frame": probe_phasor_committed_frame,
    "phasor_retained_qsg": probe_phasor_retained_qsg,
    "table_async_atomic_frame": probe_table_async_atomic_frame,
    "table_stable_geometry": probe_table_stable_geometry,
    "locus_cursor_independent_viewport": probe_locus_cursor_independent_viewport,
    "full_backward_cycle": probe_full_backward_cycle,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-release-ready",
        action="store_true",
        help="fail unless every R5 contract is declared and observed as pass",
    )
    args = parser.parse_args()

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    failures: list[str] = []

    if manifest.get("schema") != 1:
        failures.append("unsupported R5 contract schema")

    baseline = manifest.get("baseline", {})
    if baseline.get("version") != "0.2.0-alpha.22":
        failures.append("R5.0 baseline version must remain 0.2.0-alpha.22")
    if baseline.get("commit") != "0aa1c14e78e284a4224b0699621ecd939f020238":
        failures.append("R5.0 baseline commit changed unexpectedly")
    if baseline.get("release_qualified") is not False:
        failures.append("alpha.22 must remain explicitly not release-qualified")

    print("R5 locked numerical/reference blobs")
    for relative, expected_sha in manifest.get("locked_blobs", {}).items():
        path = ROOT / relative
        if not path.is_file():
            failures.append(f"locked file missing: {relative}")
            print(f"  FAIL {relative}: missing")
            continue
        try:
            actual_sha = git_blob_sha(relative)
        except RuntimeError as exc:
            failures.append(str(exc))
            print(f"  FAIL {relative}: hash error")
            continue
        if actual_sha != expected_sha:
            failures.append(
                f"locked blob changed without deliberate R5 contract update: "
                f"{relative} expected {expected_sha}, got {actual_sha}"
            )
            print(f"  FAIL {relative}: {actual_sha} != {expected_sha}")
        else:
            print(f"  PASS {relative}: {actual_sha}")

    contracts = manifest.get("contracts", {})
    if set(contracts) != set(PROBES):
        missing = sorted(set(PROBES) - set(contracts))
        extra = sorted(set(contracts) - set(PROBES))
        failures.append(f"manifest/probe mismatch: missing={missing}, extra={extra}")

    print("R5 release-blocker contracts")
    remaining: list[str] = []
    for name, probe in PROBES.items():
        spec = contracts.get(name, {})
        expected = spec.get("expected")
        if expected not in {"known_fail", "pass"}:
            failures.append(f"{name}: invalid expected state {expected!r}")
            continue
        try:
            passed = bool(probe())
        except Exception as exc:  # make source/probe breakage explicit in CI
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
                "fix the implementation or deliberately update the contract in the same PR"
            )
        if expected != "pass" or observed != "pass":
            remaining.append(name)

    if args.require_release_ready and remaining:
        failures.append(
            "strict release gate rejected remaining blockers: " + ", ".join(remaining)
        )

    if failures:
        print("\nR5 CONTRACT CHECK: FAIL")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    if remaining:
        print("\nR5 CONTRACT CHECK: STATE MATCHES MANIFEST; NOT RELEASE-READY")
        print("Remaining blockers: " + ", ".join(remaining))
    else:
        print("\nR5 CONTRACT CHECK: RELEASE CONTRACTS READY")
    return 0


if __name__ == "__main__":
    sys.exit(main())
