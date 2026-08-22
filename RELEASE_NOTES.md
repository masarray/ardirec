# ardirec v0.2.0-alpha.1 — G1 Viewer Alpha

This is the first testable Windows desktop build of **ardirec**.

## What works

- Open a COMTRADE `.cfg` and automatically locate its companion `.dat`.
- Decode ASCII, BINARY, BINARY32 and FLOAT32 sample data using the native C++ core.
- Display the first/selected analog channel with a custom Qt Quick Scene Graph waveform renderer.
- Select analog channels from the signal list.
- Zoom with the mouse wheel and pan the visible record by dragging.
- Use two visible cursors for basic time measurement.
- Show record metadata and sample count.
- Package the Qt runtime into a single portable Windows `.exe` download.

## Alpha limitations

- Desktop loading is deliberately capped at 500,000 sample frames until the memory-mapped SignalStore and persistent LOD cache land.
- Digital channels are parsed but digital-track rendering is not in this alpha.
- Cursor snapping, RMS/phasor/vector/R-X/harmonics and workspace persistence are later G1/G2 gates.
- The Windows binary is currently unsigned and may trigger SmartScreen.

## Packaging note

The one-file portable EXE is a self-extracting Qt bundle. It extracts its private runtime to a temporary location and launches `ardirec.exe`; it does not install ardirec into Windows.
