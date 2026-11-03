#!/usr/bin/env python3
"""Public repository quality gate.

Checks the current tracked tree without rewriting history or published releases.
The legacy comparison identifier is deliberately encoded so the audit does
not introduce a matching literal into its own source.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RETIRED_IDENTIFIER = bytes((115, 105, 103, 114, 97))
REQUIRED = [
    "README.md",
    "CHANGELOG.md",
    "RELEASE_NOTES.md",
    "SECURITY.md",
    "CONTRIBUTING.md",
    "docs/GETTING_STARTED.md",
    "docs/FAQ.md",
    "docs/COMTRADE_COMPATIBILITY.md",
    "index.html",
]
failures: list[str] = []

tracked = subprocess.run(
    ["git", "ls-files", "-z"], cwd=ROOT, check=True,
    stdout=subprocess.PIPE,
).stdout.split(b"\0")

for raw in tracked:
    if not raw:
        continue
    if RETIRED_IDENTIFIER in raw.lower():
        failures.append(f"retired identifier in tracked path: {raw.decode(errors='replace')}")
        continue
    relative = raw.decode("utf-8", errors="surrogateescape")
    path = ROOT / relative
    if not path.is_file():
        failures.append(f"tracked file missing: {relative}")
        continue
    if RETIRED_IDENTIFIER in path.read_bytes().lower():
        failures.append(f"retired identifier in tracked file: {relative}")

for relative in REQUIRED:
    if not (ROOT / relative).is_file():
        failures.append(f"missing public project document: {relative}")

readme = (ROOT / "README.md").read_text(encoding="utf-8")
landing = (ROOT / "index.html").read_text(encoding="utf-8")
for label, value in (
    ("README stable version", "v0.2.0"),
    ("README Windows package", "ArDiRec-v0.2.0-Windows-x64-Setup.zip"),
    ("README Linux package", "ArDiRec-v0.2.0-Linux-x86_64-Setup.zip"),
    ("README onboarding", "docs/GETTING_STARTED.md"),
):
    if value not in readme:
        failures.append(f"missing {label}")
for value in ("v0.2.0", "ArDiRec-v0.2.0-Windows-x64-Setup.zip", "ArDiRec-v0.2.0-Linux-x86_64-Setup.zip"):
    if value not in landing:
        failures.append(f"landing page missing {value}")

if failures:
    print("Public repository contract: FAIL", file=sys.stderr)
    for failure in failures:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)
print(f"Public repository contract: PASS ({len(tracked) - 1} tracked files)")
