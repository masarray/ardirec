#!/usr/bin/env python3
"""Guard R2 workstation-shell and COMTRADE document-lifetime invariants."""
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


def require_count(path: str, needle: str, minimum: int, reason: str) -> None:
    count = text(path).count(needle)
    if count < minimum:
        FAILURES.append(f"{path}: found {count} x {needle!r}, expected >= {minimum} — {reason}")


# File/keyboard lifecycle must call the controller, not merely hide QML content.
require("apps/desktop/qml/AppActions.qml", "property Action closeRecord", "Close COMTRADE must be a first-class workstation action")
require("apps/desktop/qml/AppActions.qml", 'shortcut: "Ctrl+W"', "Close COMTRADE keeps the expected workstation shortcut")
require("apps/desktop/qml/AppActions.qml", "root.document?.closeDocument()", "Ctrl+W must execute the real document lifetime operation")
require("apps/desktop/qml/WorkstationMenuBar.qml", "root.actions?.closeRecord", "File menu must expose the same close action")

# Close invalidates any outstanding loader generation before releasing every
# record-bound data/cache owner and returning the controller to a clean state.
require("apps/desktop/document_controller.hpp", "Q_INVOKABLE void closeDocument()", "QML must have one explicit close-document API")
require("apps/desktop/document_controller.hpp", "m_activeLoadCancel->store(true", "close must cancel an in-flight background load")
require("apps/desktop/document_controller.hpp", "++m_loadGeneration", "stale loader completion must be generation-rejected")
require("apps/desktop/document_controller.hpp", "m_rmsTileCache.reset()", "record-scoped RMS cache must be released")
require("apps/desktop/document_controller.hpp", "m_analogLodPyramid.reset()", "record-scoped LOD pyramid must be released")
require("apps/desktop/document_controller.hpp", "m_datStore.reset()", "DAT mapping/index ownership must be released")
require("apps/desktop/document_controller.hpp", "m_timeSeconds = std::make_shared<const std::vector<double>>()", "old timestamps must not survive close")
require("apps/desktop/document_controller.hpp", 'm_title = QStringLiteral("No record open")', "workstation must return to an explicit no-record state")
require("apps/desktop/document_controller.hpp", "m_distanceZonePath.clear()", "record sidecar state must be cleared")
require("apps/desktop/document_controller.hpp", "emit documentChanged()", "all document-bound consumers need one synchronous invalidation event")

# Existing consumers must remain connected to that invalidation event.
require("apps/desktop/cursor_snapshot_controller.cpp", "&DocumentController::documentChanged", "cursor sources/snapshots must invalidate on close")
require("apps/desktop/locus_snapshot_controller.cpp", "&DocumentController::documentChanged", "Locus worker/source must cancel and invalidate on close")
require("apps/desktop/harmonic_snapshot_controller.cpp", "&DocumentController::documentChanged", "harmonic cache must clear on close")
require("apps/desktop/table_snapshot_controller.cpp", "&DocumentController::documentChanged", "table cache must clear on close")
require("apps/desktop/main.cpp", "distanceZones.clearZones()", "distance zones must clear when the sidecar path disappears")

# Properties/About are application-overlay dialogs: no TopBar-local x/y placement.
require_count("apps/desktop/qml/TopBar.qml", "parent: Overlay.overlay", 2,
              "Properties and About must be parented to the application overlay")
require_count("apps/desktop/qml/TopBar.qml", "Math.round((parent.width - width) / 2)", 2,
              "both workstation dialogs must remain centered when the window/DPI changes")
require_count("apps/desktop/qml/TopBar.qml", "Math.round((parent.height - height) / 2)", 2,
              "both workstation dialogs must remain vertically centered")
require("apps/desktop/qml/TopBar.qml", "ArDiRec — Ari Disturbance Recorder", "About must identify the canonical product brand professionally")
require("apps/desktop/qml/TopBar.qml", "Developed by Ari Sulistiono", "About must carry developer attribution")
require("apps/desktop/qml/TopBar.qml", "GNU GPL v3.0 or later", "About must expose the open-source license")
require("apps/desktop/qml/TopBar.qml", "https://github.com/masarray/ardirec", "About must link the canonical project repository")
require("apps/desktop/qml/TopBar.qml", "https://www.linkedin.com/in/ari-sulistiono", "About must link the developer profile")
require("apps/desktop/qml/TopBar.qml", "Independent open-source project", "About must avoid employer/vendor endorsement ambiguity")

# Runtime regression covers resource release and stale background-loader publication.
require("tests/test_document_lifecycle.cpp", "document.closeDocument()", "close lifecycle must have a production-controller regression")
require("tests/test_document_lifecycle.cpp", "dataStoreSnapshot() == nullptr", "test must prove DAT ownership is released")
require("tests/test_document_lifecycle.cpp", "rmsTileCacheSnapshot() == nullptr", "test must prove RMS cache ownership is released")
require("tests/test_document_lifecycle.cpp", "pump_events(250)", "test must give the cancelled loader callback a chance to publish stale data")
require("tests/CMakeLists.txt", "ardirec_document_lifecycle_tests", "close lifecycle regression must run under CTest")

# R2 discovered that the executable version and Windows artifact identity could
# drift. Preserve that invariant across later milestones instead of pinning the
# repository forever to the historical R2 alpha.20 candidate.
main_cpp = text("apps/desktop/main.cpp")
windows_workflow = text(".github/workflows/windows-build.yml")
version_match = re.search(r'setApplicationVersion\(QStringLiteral\("([^\"]+)"\)\)', main_cpp)
if not version_match:
    FAILURES.append("apps/desktop/main.cpp: application version declaration not found")
else:
    version = version_match.group(1)
    staging = f"ardirec-v{version}-windows-x64"
    archive = f"ardirec-v{version}-windows-x64-portable.zip"
    if staging not in windows_workflow:
        FAILURES.append(f".github/workflows/windows-build.yml: staging folder does not match application version {version!r}")
    if archive not in windows_workflow:
        FAILURES.append(f".github/workflows/windows-build.yml: portable ZIP does not match application version {version!r}")

if FAILURES:
    print("ArdIREC R2 workstation contract: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC R2 workstation contract: PASS")
