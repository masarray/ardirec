#!/usr/bin/env python3
"""Validate the public ArDiRec website with only the Python standard library."""
from __future__ import annotations

import argparse
import json
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit
from xml.etree import ElementTree as ET

BASE = "https://masarray.github.io/ardirec/"
WIN = "ArDiRec-v0.2.0-Windows-x64-Setup.zip"
LINUX = "ArDiRec-v0.2.0-Linux-x86_64-Setup.zip"


class Document(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids: list[str] = []
        self.references: list[str] = []
        self.meta: dict[str, str] = {}
        self.images_without_alt: list[str] = []
        self.lang = ""

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        a = dict(attrs)
        if "id" in a and a["id"]:
            self.ids.append(a["id"])
        if tag == "html":
            self.lang = a.get("lang") or ""
        for attr in ("href", "src"):
            if a.get(attr):
                self.references.append(a[attr])
        if tag == "meta":
            self.meta[a.get("name") or a.get("property") or ""] = a.get("content") or ""
        if tag == "img" and "alt" not in a:
            self.images_without_alt.append(a.get("src", ""))


def check(site: Path) -> None:
    required = (
        "index.html", "assets/site.css", "assets/site.js",
        "icon/favicon.ico", "icon/favicon-32x32.png",
        "icon/android-chrome-192x192.png", "icon/android-chrome-512x512.png",
        "icon/apple-touch-icon.png", "icon/site.webmanifest",
        "robots.txt", "sitemap.xml",
    )
    for name in required:
        assert (site / name).is_file(), f"Required website file missing: {name}"
    html = (site / "index.html").read_text(encoding="utf-8")
    css = (site / "assets/site.css").read_text(encoding="utf-8")
    js = (site / "assets/site.js").read_text(encoding="utf-8")
    assert len(html.encode()) < 50000, "HTML budget exceeded"
    assert len(css.encode()) < 45000, "CSS budget exceeded"
    assert len(js.encode()) < 12000, "JS budget exceeded"
    assert "@import" not in css and "@font-face" not in css, "Unexpected external font dependency"

    page = Document()
    page.feed(html)
    assert page.lang == "en", "HTML language is missing"
    assert len(page.ids) == len(set(page.ids)), "Duplicate HTML element id"
    assert not page.images_without_alt, f"Images missing alt: {page.images_without_alt}"
    required_ids = ("main", "top", "features", "workflow", "download", "download-windows", "download-linux", "download-checksums")
    assert set(required_ids).issubset(page.ids), "Required navigation/download IDs missing"
    assert page.meta.get("description", "").startswith("ArDiRec"), "Missing SEO description"
    assert page.meta.get("og:url") == BASE, "Incorrect canonical social URL"
    assert f'<link rel="canonical" href="{BASE}">' in html, "Incorrect canonical link"
    assert 'href="#main"' in html and "prefers-reduced-motion" in css, "Accessibility requirements missing"

    for ref in page.references:
        parts = urlsplit(ref)
        if parts.scheme or ref.startswith("//"):
            assert parts.scheme == "https", f"Non-HTTPS external link: {ref}"
            continue
        if ref.startswith("#"):
            assert parts.fragment in page.ids, f"Broken fragment: {ref}"
            continue
        assert not parts.path.startswith("/"), f"Absolute root path breaks GitHub project Pages: {ref}"
        assert ".." not in Path(parts.path).parts, f"Parent path traversal: {ref}"
        assert (site / parts.path).is_file(), f"Broken local website asset: {ref}"

    for filename in (WIN, LINUX, "SHA256SUMS.txt"):
        assert filename in html, f"Official stable package missing from static fallback: {filename}"
    assert "https://github.com/masarray/ardirec/releases/download/v0.2.0/" in html
    assert "https://api.github.com/repos/masarray/ardirec/releases/latest" in js
    assert "AbortController" in js and ".catch(" in js, "Latest-release client must keep safe fallback"

    # Public wording is intended for people using the application, not maintainers.
    assert "See the whole event." in html and "Download ArDiRec" in html
    assert "not an application screenshot" not in html.lower()
    assert "milestone" not in html.lower() and "code agent" not in html.lower()
    assert 'class="preview-caption">Waveform illustration' in html, "Keep the sample graphic clearly identified"
    assert "⌘" not in html, "Linux platform marker must not look like the macOS Command key"

    manifest = json.loads((site / "icon/site.webmanifest").read_text(encoding="utf-8"))
    assert manifest["short_name"] == "ArDiRec"
    assert manifest["display"] == "browser", "Do not impersonate a standalone installed web app"
    assert manifest["start_url"] == "../" and manifest["scope"] == "../", "Project Pages scope mismatch"
    for icon in manifest["icons"]:
        assert (site / "icon" / icon["src"]).is_file(), "Missing webmanifest icon"
    sitemap = ET.parse(site / "sitemap.xml")
    ns = "{http://www.sitemaps.org/schemas/sitemap/0.9}"
    urls = [x.text for x in sitemap.findall(f".//{ns}loc")]
    assert urls == [BASE], "Sitemap must point only to canonical live URL"
    robots = (site / "robots.txt").read_text(encoding="utf-8")
    assert BASE + "sitemap.xml" in robots, "robots.txt sitemap pointer missing"
    print(f"ArDiRec Pages contract PASS: {len(page.references)} references, {len(page.ids)} IDs, {len(required)} assets")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--site-root", default=".")
    args = parser.parse_args()
    check(Path(args.site_root).resolve())
