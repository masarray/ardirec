#!/usr/bin/env python3
"""R5.5 release-qualification guard.

This gate intentionally does not replace the manual Windows/SIGRA visual check.
It proves that every executable R5 contract is green and that CI/Windows packaging
exercise the release-blocking interaction/lifecycle paths repeatedly before an RC
can be promoted.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAILURES: list[str] = []
CANDIDATE_VERSION = "0.2.0-alpha.23"
STRESS_TESTS = (
    "ardirec_document_lifecycle_tests",
    "ardirec_r4_qml_runtime_tests",
    "ardirec_r5_linked_cursor_tests",
    "ardirec_r5_phasor_zero_flicker_tests",
    "ardirec_r5_table_zero_flicker_tests",
    "ardirec_r5_locus_parity_tests",
)


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, needle: str, reason: str) -> None:
    if needle not in text(path):
        FAILURES.append(f"{path}: missing {needle!r} — {reason}")


def forbid(path: str, needle: str, reason: str) -> None:
    if needle in text(path):
        FAILURES.append(f"{path}: found forbidden {needle!r} — {reason}")


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
    require(".github/workflows/ci.yml", test,
            "every R5.5 stress test must run in Linux desktop qualification")
    require(".github/workflows/windows-build.yml", test,
            "every R5.5 stress test must run in Windows packaged qualification")

require("apps/desktop/main.cpp", CANDIDATE_VERSION,
        "manual qualification needs an unambiguous post-R5.4 candidate identity")
require(".github/workflows/windows-build.yml", CANDIDATE_VERSION,
        "Windows artifact identity must match the application candidate version")
forbid(".github/workflows/release.yml", "0.2.0-alpha.14",
       "release workflow must not publish a stale historical build after R5.5")
require("docs/R5_RELEASE_QUALIFICATION.md", "manual Windows/SIGRA",
        "the non-automatable final visual gate must remain explicit")
require("docs/R5_RELEASE_QUALIFICATION.md", "0.2.0-rc.1",
        "RC promotion policy must remain explicit")

if FAILURES:
    print("ArdIREC R5.5 release qualification guard: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC R5.5 release qualification guard: PASS")
print(strict.stdout.strip())
