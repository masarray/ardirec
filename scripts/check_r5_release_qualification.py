#!/usr/bin/env python3
"""R5.5 release-qualification guard.

This gate intentionally does not replace the manual Windows/SIGRA visual check.
It proves that every executable R5 contract is green, version identity has one
source of truth, and CI/Windows packaging repeatedly exercise the release-
blocking interaction/lifecycle paths before an RC can be promoted.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAILURES: list[str] = []
STRESS_TESTS = (
    "ardirec_document_lifecycle_tests",
    "ardirec_r4_qml_runtime_tests",
    "ardirec_r5_linked_cursor_tests",
    "ardirec_r5_phasor_zero_flicker_tests",
    "ardirec_r5_table_zero_flicker_tests",
    "ardirec_r5_locus_parity_tests",
)
QUALIFICATION_VERSIONS = {"0.2.0-alpha.23", "0.2.0-rc.1"}


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, needle: str, reason: str) -> None:
    if needle not in text(path):
        FAILURES.append(f"{path}: missing {needle!r} — {reason}")


def forbid(path: str, needle: str, reason: str) -> None:
    if needle in text(path):
        FAILURES.append(f"{path}: found forbidden {needle!r} — {reason}")


version = text("VERSION").strip()
if version not in QUALIFICATION_VERSIONS:
    FAILURES.append(
        f"VERSION: R5.5 qualification expects one of {sorted(QUALIFICATION_VERSIONS)}, got {version!r}"
    )

manifest = json.loads(text("scripts/r5_release_contract.json"))
for name, spec in manifest.get("contracts", {}).items():
    if spec.get("expected") != "pass":
        FAILURES.append(f"scripts/r5_release_contract.json: {name} is not promoted to pass")

strict = subprocess.run(
    [sys.executable, str(ROOT / "scripts" / "check_r5_release_contract.py"), "--require-release-ready"],
    cwd=ROOT,
    text=True,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    check=False,
)
if strict.returncode != 0:
    FAILURES.append("strict R5 release contract failed:\n" + strict.stdout.strip())

require(".github/workflows/ci.yml", "check_r5_release_contract.py --require-release-ready",
        "R5.5 CI must reject any remaining release blocker")
require(".github/workflows/ci.yml", "--repeat until-fail:3",
        "Linux qualification must repeat interaction/lifecycle tests to expose flakes")
require(".github/workflows/windows-build.yml", "--repeat until-fail:3",
        "Windows qualification must repeat the same release-blocking tests")
for test in STRESS_TESTS:
    # The workflow intentionally uses the compact regex
    # ardirec_(foo|bar|...) to keep one repeated ctest invocation. Require each
    # target suffix in that regex and the full target in CTest registration.
    suffix = test.removeprefix("ardirec_")
    require(".github/workflows/ci.yml", suffix,
            "every R5.5 stress target must be selected by Linux qualification")
    require(".github/workflows/windows-build.yml", suffix,
            "every R5.5 stress target must be selected by Windows qualification")
    require("tests/CMakeLists.txt", test,
            "every R5.5 stress target must remain registered in CTest")

# One source of truth: VERSION -> CMake compile definition -> runtime and packaging.
require("apps/desktop/CMakeLists.txt", 'file(READ "${CMAKE_SOURCE_DIR}/VERSION" ARDIREC_VERSION)',
        "desktop build must consume canonical VERSION")
require("apps/desktop/main.cpp", "setApplicationVersion(QStringLiteral(ARDIREC_VERSION))",
        "runtime identity must come from canonical VERSION")
require(".github/workflows/windows-build.yml", "Get-Content VERSION -Raw",
        "Windows artifact identity must consume canonical VERSION")
require(".github/workflows/release.yml", "Get-Content VERSION -Raw",
        "release identity must consume canonical VERSION")
forbid(".github/workflows/release.yml", "0.2.0-alpha.14",
       "release workflow must not publish a stale historical build after R5.5")

require("docs/R5_RELEASE_QUALIFICATION.md", "manual Windows/SIGRA",
        "the non-automatable final visual gate must remain explicit")
require("docs/R5_RELEASE_QUALIFICATION.md", "0.2.0-rc.1",
        "RC promotion policy must remain explicit")
require("docs/R5_RELEASE_QUALIFICATION.md", "ArDiRec",
        "qualification documentation must use the canonical product brand")

if FAILURES:
    print("ArDiRec R5.5 release qualification guard: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print(f"ArDiRec R5.5 release qualification guard: PASS ({version})")
print(strict.stdout.strip())
