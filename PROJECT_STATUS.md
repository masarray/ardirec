# Project Status

## Current milestone

**G1 — Analysis Workspace Alpha (`v0.2.0-alpha.2`)**

## Implemented

- C++20 COMTRADE CFG parser.
- DAT reader for ASCII, BINARY, BINARY32 and FLOAT32.
- Packed digital decoding.
- Companion-file discovery for CFG/DAT/HDR/INF/DMF.
- Native CLI inspection tool.
- Synthetic compatibility fixtures and regression tests.
- Qt Quick desktop application.
- Real COMTRADE analog sample loading into the desktop document model.
- Synchronized stacked multi-track analog waveform workspace.
- Shared trigger-relative time ruler.
- COMTRADE trigger reference through all visible tracks.
- Absolute dual cursors plus cursor measurement table.
- Instantaneous cursor values and C2-C1 measurement for a selected measuring signal.
- Signal drawer for selecting up to eight analog tracks.
- Fit-record and trigger-focused navigation.
- Mouse-centered zoom and synchronized pan.
- Custom Qt Quick Scene Graph waveform rendering with zero-centered electrical scaling.
- Readable polyline decimation for normal AC traces and dense-view min/max envelope fallback.
- Windows CI and one-file portable EXE packaging.

## Next engineering gates

1. Dedicated digital transition tracks on the same synchronized timebase.
2. Per-track manual Y scaling / auto-range controls and channel grouping presets.
3. Cursor snapping to sample, zero crossing, extrema and digital edges.
4. Memory-mapped binary SignalStore and indexed ASCII cache.
5. Persistent multi-resolution LOD pyramid for very large records.
6. Record Health diagnostics and compatibility warnings.
7. RMS/phasor/symmetrical-component calculation engine.
8. Vector, R-X, harmonics and value-table views.
9. Workspace save/load and multi-record synchronization.

The product scope remains locked: manual disturbance-analysis parity first, then multi-record combine/synchronization, then fault analysis.
