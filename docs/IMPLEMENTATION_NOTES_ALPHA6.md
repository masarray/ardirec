# Implementation notes — alpha.6

The multi-view slice deliberately keeps the heavy electrical calculations in C++ and the presentation in QML.

- `AnalysisController`: phase mapping, RMS, fundamental/harmonic DFT, raw complex impedance.
- `RmsWaveformItem`: GPU-rendered one-cycle sliding RMS time trace.
- `CursorNavigator`: shared C1/C2 state and digital-edge snap across every view.
- `TimeSignalsView`: scrollable analog/digital disturbance stack.
- `PhasorView`, `LocusView`, `HarmonicsView`, `ValueTableView`: synchronized analysis surfaces.

The raw locus is not yet a distance-relay loop model. Compensation factors, phase-phase/phase-earth loop selection and RIO/XRIO overlays are intentionally deferred until calculation validation is in place.
