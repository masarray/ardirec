<p align="center">
  <img src="icon/android-chrome-192x192.png" width="128" height="128" alt="ArDiRec logo">
</p>

# ArDiRec

**ArDiRec** (Ari Disturbance Recorder) is an open-source, vendor-neutral COMTRADE workstation for protection and disturbance engineers.

> Open any disturbance record. Understand it immediately.

The project is intentionally being built in layers. The first product gate is a **SIGRA/TransView-class manual disturbance-analysis workstation** with a more modern, compact and smooth industrial UX. Multi-record combination, fault analysis and protection intelligence come only after the manual viewer is trustworthy.

## Product direction

ArDiRec targets four qualities above feature count:

1. **Compatibility** — real-world COMTRADE 1991/1999/2001/2013, including ugly vendor edge cases.
2. **Speed** — progressive loading, lazy decoding, multi-resolution waveform LOD and GPU rendering.
3. **Clarity** — compact precision-industrial UX where the waveform is the primary surface.
4. **Engineering transparency** — every calculated quantity must be reproducible and explainable.

See [PRD](docs/PRD.md), [parity matrix](docs/PARITY_MATRIX.md), [architecture](docs/ARCHITECTURE.md), and [roadmap](docs/ROADMAP.md).

## Current status — engineering prerelease candidate (not stable)

The current desktop alpha contains:

- C++20 COMTRADE CFG parser.
- DAT decoding for ASCII, BINARY, BINARY32 and FLOAT32.
- Packed digital/status decoding.
- Automatic sibling-file discovery (`.cfg/.dat/.hdr/.inf/.dmf`).
- `ardirec-cli inspect` for deterministic parser smoke tests.
- Qt Quick disturbance stack with synchronized voltage, current, other analog and digital-event tracks.
- Custom `QQuickItem` / Qt Scene Graph waveform rendering, min/max display decimation, shared timebase, fixed trigger reference, pan/zoom and dual measurement cursors.
- Cursor hover feedback plus snapping to nearby rising/falling COMTRADE digital transitions for protection timing measurements.
- Standard electrical phase identity across voltage/current views: L1 red, L2 yellow, L3 blue, E/N green.
- Time Signals with Instantaneous / one-cycle RMS switching that preserves the same investigation context.
- Synchronized Phasor, raw R-X Locus, Harmonics and Value Table views.
- Global **Secondary / Primary** value representation driven by COMTRADE transformer-ratio and recorded-side metadata; the same representation is consumed by waveform, RMS, phasor, harmonics, table and raw impedance calculations.
- Compact cursor-analysis header with C1, C2, C2-C1 and C2+C1 plus simultaneous instantaneous/RMS values.
- Windows CI plus a portable ZIP development distribution.
- Product, compatibility, performance and validation specifications.

Large COMTRADE records use indexed/lazy DAT access and bounded waveform LOD. CI exercises 10M-sample and 120-channel cases, but manual vendor compatibility and SIGRA comparison remain release gates.

## Install on Windows or Linux (candidate packages)

The build workflows generate two installer ZIPs; they are CI artifacts until
a reviewed **draft prerelease** is explicitly published:

- **Windows 10/11 x64:** `ArDiRec-v<VERSION>-Windows-x64-Setup.zip` contains a
  conventional Inno Setup wizard (per-user, no admin required), Start menu
  entry, optional desktop shortcut, in-place upgrades and standard uninstall.
- **Linux x86_64:** `ArDiRec-v<VERSION>-Linux-x86_64-Setup.zip` contains a
  double-click-installable Debian `.deb` package for Ubuntu/Debian users and
  a portable AppImage alternative for compatible Linux desktop distributions.

Unzip the matching package, read its README, and launch the installer.
The Windows setup is **not code-signed**; ZIP packaging does not avoid
Windows SmartScreen warnings. Verify SHA256SUMS from the official release.
See [installer and test contract](docs/DISTRIBUTION.md).

## Build

### Core + CLI only

```bash
cmake -S . -B build -G Ninja -DARDIREC_BUILD_DESKTOP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
./build/cli/ardirec-cli inspect tests/data/minimal_1999.cfg
```

### Desktop

Requirements:

- CMake 3.24+
- C++20 compiler
- Qt 6.8+ (Qt 6.11.2 is the recommended development version as of August 2026)
- Qt Quick, QML and Quick Controls 2

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<kit>
cmake --build build
```

Qt 6.11.2 is currently available in the official Qt release tree; the project keeps a Qt 6.8 minimum so contributors can also use the maintained 6.8 line.

## Non-goals for Viewer 1.0

Viewer 1.0 does **not** include AI-generated fault conclusions. It first earns trust through manual investigation parity: time signals, cursors, RMS/phasors, vector, symmetrical components, power/frequency, impedance/R-X, harmonics, formulas, RIO/XRIO overlays, tables and exports.

## Licensing

ArDiRec is licensed under the **GNU General Public License v3.0 or later**. See [LICENSE](LICENSE).

Third-party code must be license-compatible and documented. Competitor products are used only as black-box capability/UX benchmarks; proprietary code and visual assets must not be copied.

## Trademark note

SIGRA, SIPROTEC, OMICRON, TransView, SEL and other product names are trademarks of their respective owners. ArDiRec is not affiliated with or endorsed by those vendors.

## Contributing

Start with [CONTRIBUTING.md](CONTRIBUTING.md). Compatibility fixtures, reproducible parser edge cases, benchmark records that can legally be redistributed, and calculation-validation evidence are especially valuable.
