#!/usr/bin/env python3
"""Guard R3 Qt Quick internal-MDI architecture and workstation semantics."""
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


def require_count(path: str, needle: str, minimum: int, reason: str) -> None:
    count = text(path).count(needle)
    if count < minimum:
        FAILURES.append(f"{path}: found {count} x {needle!r}, expected >= {minimum} — {reason}")


def forbid(path: str, needle: str, reason: str) -> None:
    if needle in text(path):
        FAILURES.append(f"{path}: found forbidden {needle!r} — {reason}")


# Main shell must be a real internal-MDI host, not the former one-view/fixed shell.
require("apps/desktop/qml/Main.qml", "MdiWorkspace {",
        "the application workspace must be owned by the internal MDI manager")
forbid("apps/desktop/qml/Main.qml", "DeferredEngineeringViews {",
       "the legacy single-active deferred host must not define the workstation architecture")
forbid("apps/desktop/qml/Main.qml", "id: timeSignals",
       "Time Signals must live inside an MDI child rather than as a fixed workspace pane")
require("apps/desktop/qml/Main.qml", "readonly property string viewMode: mdiWorkspace.activeViewType",
        "top-level workstation controls must follow the active MDI child")
require("apps/desktop/qml/Main.qml", 'mdiWorkspace.openView(viewName, false)',
        "normal view commands must activate-or-create an MDI child")
require("apps/desktop/qml/Main.qml", 'mdiWorkspace.openView(viewName, true)',
        "duplicate analysis windows must be explicitly creatable")

# Packaging must include the three separable layers of the Qt Quick MDI architecture.
for qml in ("qml/MdiWorkspace.qml", "qml/MdiChildWindow.qml", "qml/AnalysisViewHost.qml"):
    require("apps/desktop/CMakeLists.txt", qml, "new MDI QML type must be compiled into the application module")
for path in ("apps/desktop/qml/MdiWorkspace.qml",
             "apps/desktop/qml/MdiChildWindow.qml",
             "apps/desktop/qml/AnalysisViewHost.qml"):
    forbid(path, "QMdiArea", "R3 must remain pure Qt Quick inside the existing scene graph")
    forbid(path, "QQuickWidget", "QQuickWidget would add an offscreen pass and disable the threaded render loop")

# Workspace model/lifecycle semantics.
require("apps/desktop/qml/MdiWorkspace.qml", "ListModel { id: windowsModel }",
        "MDI children require one explicit model owned by the workspace")
require("apps/desktop/qml/MdiWorkspace.qml", "property var activationHistory",
        "next/previous activation requires stable activation history")
require("apps/desktop/qml/MdiWorkspace.qml", "function openView(viewType, forceNew)",
        "workspace must distinguish activate-or-create from duplicate creation")
require("apps/desktop/qml/MdiWorkspace.qml", "if (!forceNew)",
        "ordinary view activation must reuse an existing child")
require("apps/desktop/qml/MdiWorkspace.qml", "windowsModel.append({",
        "duplicate child creation must append a distinct model row")
for method in ("closeWindow", "minimizeWindow", "restoreWindow", "maximizeWindow",
               "toggleMaximize", "activateNext", "activatePrevious", "cascade",
               "tileHorizontal", "tileVertical"):
    require("apps/desktop/qml/MdiWorkspace.qml", f"function {method}(",
            f"MDI workspace requires {method} semantics")
require("apps/desktop/qml/MdiWorkspace.qml", 'openView("time", true)',
        "a freshly loaded record must start with one Time Signals child")
require("apps/desktop/qml/MdiWorkspace.qml", 'windowState === "minimized"',
        "minimized children must be a first-class state")
require("apps/desktop/qml/MdiWorkspace.qml", 'live: childWindow.windowState !== "minimized"',
        "minimized analysis hosts must receive quiescent/frozen inputs")
require("apps/desktop/qml/MdiWorkspace.qml", "recomputeRequestOwners",
        "duplicate views need shared snapshot request ownership rather than duplicate producer work")
forbid("apps/desktop/qml/MdiWorkspace.qml", 'setProperty(index, "viewType"',
       "Cascade/Tile/resize must never mutate a child analysis type")
forbid("apps/desktop/qml/MdiWorkspace.qml", 'setProperty(i, "viewType"',
       "arrangement must preserve each child analysis type")

# Child chrome supports professional window manipulation without native QWidget embedding.
for role in ("windowId", "windowTitle", "viewType", "windowX", "windowY",
             "windowWidth", "windowHeight", "windowState", "zOrder"):
    require("apps/desktop/qml/MdiChildWindow.qml", "required property", "MDI model roles must bind explicitly")
    require("apps/desktop/qml/MdiChildWindow.qml", role, f"child window must carry the {role} role")
for signal in ("activateRequested", "closeRequested", "minimizeRequested",
               "restoreRequested", "toggleMaximizeRequested", "geometryRequested"):
    require("apps/desktop/qml/MdiChildWindow.qml", f"signal {signal}",
            f"child chrome must expose {signal}")
for mask in (1, 2, 4, 8, 5, 6, 9, 10):
    require("apps/desktop/qml/MdiChildWindow.qml", f"ResizeHandle {{ edgeMask: {mask};",
            "all four edges and all four corners must support manual resize")
require("apps/desktop/qml/MdiChildWindow.qml", "onDoubleClicked:",
        "title-bar double-click must support maximize/restore")

# Each child hosts exactly one view-type-selected AnalysisViewHost. Minimized views
# keep lightweight UI state but lose record-sized render/analysis inputs.
require("apps/desktop/qml/AnalysisViewHost.qml", "Loader {",
        "each child requires one retained loader boundary")
require_count("apps/desktop/qml/AnalysisViewHost.qml", "Loader {", 1,
              "one child host should not instantiate multiple competing view loaders")
for component in ("TimeSignalsView {", "PhasorView {", "LocusView {", "HarmonicsView {", "ValueTableView {"):
    require("apps/desktop/qml/AnalysisViewHost.qml", component,
            "AnalysisViewHost must support every workstation analysis type")
require("apps/desktop/qml/AnalysisViewHost.qml", "analysis: root.live ? root.analysis : null",
        "minimized views must not retain active analysis work")
require("apps/desktop/qml/AnalysisViewHost.qml", "snapshot: root.live ? root.harmonicSnapshot : null",
        "minimized Harmonics must not issue snapshot work")
require("apps/desktop/qml/AnalysisViewHost.qml", "snapshot: root.live ? root.tableSnapshot : null",
        "minimized Engineering Table must not issue snapshot work")
require("apps/desktop/qml/AnalysisViewHost.qml", "voltageChannels: root.live ? root.voltageChannels : []",
        "minimized Time Signals must release lane render delegates")
require("apps/desktop/qml/AnalysisViewHost.qml", "requestOwner: root.requestOwner && root.live",
        "duplicate Phasor children must share one snapshot producer")
require("apps/desktop/qml/PhasorView.qml", "property bool requestOwner: true",
        "Phasor view must expose shared request ownership")
require("apps/desktop/qml/PhasorView.qml", "if (!root.requestOwner || !root.document || !root.visible) return",
        "non-owner duplicate Phasor children must consume shared snapshots without launching DFT work")

# User-facing workstation actions and Window menu semantics.
for shortcut in ('shortcut: "Shift+1"', 'shortcut: "Shift+2"', 'shortcut: "Shift+3"',
                 'shortcut: "Shift+4"', 'shortcut: "Shift+5"', 'shortcut: "Ctrl+F4"',
                 'shortcut: "Ctrl+Tab"', 'shortcut: "Ctrl+Shift+Tab"'):
    require("apps/desktop/qml/AppActions.qml", shortcut,
            "R3 requires discoverable keyboard access to MDI child operations")
for signal in ("newViewRequested", "closeAnalysisWindowRequested", "nextWindowRequested",
               "previousWindowRequested", "cascadeRequested", "tileHorizontalRequested",
               "tileVerticalRequested"):
    require("apps/desktop/qml/AppActions.qml", f"signal {signal}",
            f"workstation actions must expose {signal}")
require("apps/desktop/qml/WorkstationMenuBar.qml", 'title: "&Window"',
        "professional MDI semantics require a Window menu")
for action in ("closeAnalysisWindow", "nextAnalysisWindow", "previousAnalysisWindow",
               "cascadeWindows", "tileHorizontalWindows", "tileVerticalWindows"):
    require("apps/desktop/qml/WorkstationMenuBar.qml", f"root.actions?.{action}",
            f"Window menu must expose {action}")

# R3's permanent invariant is the MDI architecture above, not the historical
# alpha.21 label. Cross-milestone executable/artifact version alignment is owned
# by the generalized R2 contract, while the current milestone owns its candidate.
require("apps/desktop/main.cpp", "setApplicationVersion",
        "desktop application must keep an explicit build identity")
require(".github/workflows/windows-build.yml", "ardirec-v",
        "Windows packaging must keep an explicit versioned artifact identity")

if FAILURES:
    print("ArdIREC R3 internal MDI contract: FAIL", file=sys.stderr)
    for failure in FAILURES:
        print(f" - {failure}", file=sys.stderr)
    raise SystemExit(1)

print("ArdIREC R3 internal MDI contract: PASS")
