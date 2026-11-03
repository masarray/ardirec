# Changelog

Notable changes to the public ArDiRec desktop application. For downloadable packages and checksums, use [GitHub Releases](https://github.com/masarray/ardirec/releases).

## [0.2.0] — 2026-11-03

First public Stable release.

### Available

- Synchronized analog and digital COMTRADE time signals with C1/C2 cursor measurements.
- Instantaneous and one-cycle RMS viewing, phase-aware phasors, harmonics, impedance locus and value table.
- Primary/secondary value representation where the recording supplies usable transformer metadata.
- Compact multi-window workspace with retained, anti-aliased Qt scene-graph drawing and digital-event visibility.
- Indexed/lazy record access, bounded waveform level-of-detail, asynchronous analysis, and large-record regression benchmarks.
- Windows x64 installer and Linux x86_64 Debian/AppImage distribution, packaged as ZIP downloads with SHA-256 checksums.

### Known boundaries

- This release supports manual investigation; automatic event diagnosis and exhaustive vendor-specific RIO/XRIO processing are not claimed.
- Windows installer is unsigned. Linux compatibility depends on desktop/runtime environment.
- Compatibility varies across records; report reproducible import or calculation issues with legally shareable examples.

## Earlier development

Earlier alpha milestones established the parser, electrical-analysis engine, multi-view workstation and testing foundation. See the repository history for engineering-stage details.
