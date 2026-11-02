#!/usr/bin/env python3
"""R10 Qt Material-icon and anti-aliased waveform release safety check."""
from __future__ import annotations

from pathlib import Path
import re
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
QML = ROOT / "apps/desktop/qml"
CMAKE = (ROOT / "apps/desktop/CMakeLists.txt").read_text(encoding="utf-8")
TESTS = (ROOT / "tests/CMakeLists.txt").read_text(encoding="utf-8")

def require(condition: bool, detail: str) -> None:
    if not condition:
        raise SystemExit(f"R10 visual contract: FAIL — {detail}")

icons = sorted((QML / "material").glob("*.svg"))
require(len(icons) >= 15, "Material artwork library is incomplete")
for icon in icons:
    root = ET.parse(icon).getroot()
    require(root.tag.endswith("svg"), f"not an SVG: {icon.name}")
    require(root.get("viewBox") == "0 0 24 24", f"incorrect intrinsic size: {icon.name}")
    require(f"qml/material/{icon.name}" in CMAKE, f"asset missing Qt resource: {icon.name}")
require(not list((QML / "icons").glob("*.svg")), "legacy Lucide artwork still present")
references: set[str] = set()
for path in QML.rglob("*.qml"):
    source = path.read_text(encoding="utf-8")
    require("icons/" not in source, f"legacy Lucide resource reference: {path}")
    references.update(re.findall(r"material/([\w-]+\.svg)", source))
require(references, "no Material icons referenced from UI")
require(all((QML / "material" / icon).is_file() for icon in references),
        "Qt UI refers to a non-existing Material SVG")
for name in ("TopBar.qml", "ViewModeBar.qml", "MdiChildWindow.qml"):
    source = (QML / name).read_text(encoding="utf-8")
    require("icon.source:" in source and "icon.color:" in source,
            f"{name} lost Qt native SVG colorization")
license_text = (ROOT / "licenses/Apache-2.0.txt").read_text(encoding="utf-8")
require("Apache License" in license_text and "Version 2.0" in license_text,
        "Google icon redistribution license missing")
header = (ROOT / "apps/desktop/antialiased_trace_geometry.hpp").read_text(encoding="utf-8")
require("QSGVertexColorMaterial" in header and "DrawTriangles" in header,
        "anti-aliased stroke must use vertex-color triangles")
require("halfCore = 0.925F" in header and "0.70F" in header,
        "R10 stroke width/AA feather unexpectedly changed")
for name in ("waveform_item.cpp", "rms_waveform_item.cpp"):
    source = (ROOT / "apps/desktop" / name).read_text(encoding="utf-8")
    require("update_antialiased_trace(" in source, f"{name} bypasses AA shared renderer")
    require("DrawLineStrip" not in source and "DrawLines" not in source,
            f"{name} still uses aliased primitive lines")
require("ardirec_r10_trace_geometry_tests" in TESTS,
        "R10 scenegraph geometry regression not registered in CTest")
print(f"R10 visual contract: PASS — {len(icons)} Google SVG assets, "
      f"{len(references)} referenced in QML, Qt tint and AA renderer guarded")
