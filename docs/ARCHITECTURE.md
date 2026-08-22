# Architecture

## Architectural rule

UI, file parsing, signal storage, calculations and rendering are separate systems. This prevents the Viewer 1.0 foundation from being discarded when multi-record and fault analysis arrive.

```text
COMTRADE bundle
      │
      ▼
Raw Record Model ────────────── diagnostics
      │
      ▼
Normalized Signal Model
      │
      ├──────────────► Derived Signal Engine
      │
      ▼
Signal Store ─► LOD Cache ─► Render Snapshot ─► QQuickItem/QSG ─► GPU
      │
      └──────────────► Analysis Engine
                              │
                              ▼
                       Investigation Model
                              │
                ┌─────────────┼─────────────┐
                ▼             ▼             ▼
              Time          Vector         R-X / FFT / Table
```

## Technology

- **C++20**: parser, signal storage, calculations, workspace model and rendering backend.
- **Qt Quick/QML**: presentation and interaction composition.
- **Qt Scene Graph / QSG**: waveform rendering; no QML Canvas/Qt Charts as the primary waveform path.
- **CMake**: build system.

## Raw vs normalized models

Raw COMTRADE metadata remains exact enough to explain what was recorded. Semantic grouping (e.g. `IL1`, `IA`, `I_A` → phase-A current) lives separately and is reversible.

## Large-file design for G1

The reference `DatReader` in G0 validates decoding only. G1 replaces viewer access with:

1. memory mapping for binary DAT where practical;
2. streaming/indexed cache for ASCII;
3. channel-oriented access;
4. min/max multi-resolution LOD pyramid;
5. background jobs for I/O, decode, LOD, FFT/DFT and derived quantities;
6. immutable render snapshots passed to the render path.

Never send millions of off-screen samples to the GPU when the display has only a few thousand horizontal pixels.

## Investigation model

A future `.ardirec` workspace stores references and analysis state, not modified COMTRADE samples:

- selected/grouped signals;
- derived signals;
- cursor positions/bookmarks;
- plot layout/axes;
- imported RIO/XRIO;
- notes and user classifications.
