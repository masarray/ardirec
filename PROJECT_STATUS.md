# Project Status

## Current milestone

**G1 — Viewer Alpha (`v0.2.0-alpha.1`)**

## Implemented

- C++20 COMTRADE CFG parser.
- DAT reader for ASCII, BINARY, BINARY32 and FLOAT32.
- Packed digital decoding.
- Companion-file discovery for CFG/DAT/HDR/INF/DMF.
- Native CLI inspection tool.
- Synthetic compatibility fixtures and regression tests.
- Qt Quick desktop shell.
- Real COMTRADE analog sample loading into the desktop document model.
- Custom Qt Quick Scene Graph waveform rendering.
- Min/max display decimation for zoomed-out waveform views.
- Analog channel selection.
- Basic zoom, pan and dual cursors.
- Windows CI and one-file portable EXE packaging.

## Next engineering gates

1. Memory-mapped binary SignalStore and indexed ASCII cache.
2. Persistent multi-resolution LOD pyramid for very large records.
3. Dedicated digital transition renderer.
4. Accurate shared time axis and cursor snap modes.
5. Record Health diagnostics and compatibility warnings.
6. RMS/phasor/symmetrical-component calculation engine.
7. Vector, R-X, harmonics and value-table views.

The product scope remains locked: manual disturbance-analysis parity first, then multi-record combine/synchronization, then fault analysis.
