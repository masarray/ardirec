# ardirec

**ardirec** is an open-source, vendor-neutral COMTRADE workstation for protection and disturbance engineers.

> Open any disturbance record. Understand it immediately.

The project is intentionally being built in layers. The first product gate is a **SIGRA/TransView-class manual disturbance-analysis workstation** with a more modern, compact and smooth industrial UX. Multi-record combination, fault analysis and protection intelligence come only after the manual viewer is trustworthy.

## Product direction

ardirec targets four qualities above feature count:

1. **Compatibility** — real-world COMTRADE 1991/1999/2001/2013, including ugly vendor edge cases.
2. **Speed** — progressive loading, lazy decoding, multi-resolution waveform LOD and GPU rendering.
3. **Clarity** — compact precision-industrial UX where the waveform is the primary surface.
4. **Engineering transparency** — every calculated quantity must be reproducible and explainable.

See [PRD](docs/PRD.md), [parity matrix](docs/PARITY_MATRIX.md), [architecture](docs/ARCHITECTURE.md), and [roadmap](docs/ROADMAP.md).

## Current status — G1 Viewer Alpha

The current desktop alpha contains:

- C++20 COMTRADE CFG parser.
- DAT decoding for ASCII, BINARY, BINARY32 and FLOAT32.
- Packed digital/status decoding.
- Automatic sibling-file discovery (`.cfg/.dat/.hdr/.inf/.dmf`).
- `ardirec-cli inspect` for deterministic parser smoke tests.
- Qt Quick disturbance stack with synchronized voltage, current, other analog and digital-event tracks.
- Custom `QQuickItem` / Qt Scene Graph rendering, min/max display decimation, shared timebase, trigger marker, pan/zoom and dual measurement cursors.
- Cursor hover feedback plus snapping to nearby rising/falling COMTRADE digital transitions for protection timing measurements.
- Windows CI plus a portable ZIP development distribution.
- Product, compatibility, performance and validation specifications.

The alpha deliberately caps desktop preview loading at **500,000 frames**. A later G1 slice replaces this temporary limit with a memory-mapped/lazy SignalStore and persistent multi-resolution LOD pyramid.

## Try the Windows alpha

During alpha/beta development, Windows builds are distributed as a transparent portable ZIP folder, for example:

`ardirec-v0.2.0-alpha.5-windows-x64-portable.zip`

Extract the archive anywhere and run `ardirec.exe` from the extracted folder. No installer is required and ardirec does not install itself into Windows. The folder intentionally contains Qt runtime DLLs/plugins produced by `windeployqt`; keeping these files visible makes development packaging fast and predictable.

A proper Windows installer is planned closer to stable/final releases. The executable is currently unsigned, so Windows SmartScreen may warn on first launch.

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

ardirec is licensed under the **GNU General Public License v3.0 or later**. See [LICENSE](LICENSE).

Third-party code must be license-compatible and documented. Competitor products are used only as black-box capability/UX benchmarks; proprietary code and visual assets must not be copied.

## Trademark note

SIGRA, SIPROTEC, OMICRON, TransView, SEL and other product names are trademarks of their respective owners. ardirec is not affiliated with or endorsed by those vendors.

## Contributing

Start with [CONTRIBUTING.md](CONTRIBUTING.md). Compatibility fixtures, reproducible parser edge cases, benchmark records that can legally be redistributed, and calculation-validation evidence are especially valuable.
