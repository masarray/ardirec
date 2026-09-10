# P1D.2A — ARSAS investigation-context bridge parity

This slice exposes the ArdIrec workstation semantics that ARSAS needs before porting the mature COMTRADE UX to WPF. It is intentionally stacked on the latest open ArdIrec engine branch and keeps the existing C ABI version at 1 by adding new exports only.

## Contract

- `ardirec_bridge_capabilities()` lets hosts probe additive functionality without assuming every ABI-v1 DLL has the same optional exports.
- Analog role/phase classification is owned by ArdIrec core (`Voltage`, `Current`, `Other`; `L1/L2/L3/Neutral/Other`). ARSAS must not maintain an independent classifier.
- Primary/Secondary conversion is owned by ArdIrec `representation_scale()`; hosts request the scale or a cursor measurement in the target representation.
- Cursor measurement is source-frame based and returns the recorded timestamp, instantaneous nearest-frame value, trailing nominal-cycle true RMS, and the exact sample window used.
- Digital active state is defined against COMTRADE `normal_state`, never by assuming `1 == active`.
- Native digital-edge snapping returns the nearest transition within a caller-provided time tolerance together with raw transition direction and whether the transition became active.
- Existing waveform, phasor and harmonic exports remain unchanged and binary compatible.

## P1D.2B consumer expectations

ARSAS will keep C1/C2 as global investigation state. Cursor motion can repaint immediately, then asynchronously request ArdIrec cursor measurements. Digital snap will use the native edge API; event labels remain presentation semantics, while `became_active` is authoritative engine state.

## Acceptance

- Core CI Windows/Linux/macOS green.
- Desktop Qt regression remains green.
- Native bridge smoke covers capability probing, role/phase semantics, Primary/Secondary scaling, instantaneous + true RMS cursor measurement, active-high and active-low digital state, and nearest-edge snap.
- Existing phasor/harmonic bridge smoke remains green.
