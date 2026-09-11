# P1 — SIGRA one-cycle RMS parity

## Problem reproduced

On the Siemens SIGRA `ligne_1` reference record (50 Hz, 1 kHz), ArdIREC's RMS waveform had two visible errors:

1. At the left edge of the finite record, ArdIREC divided by the number of samples currently available. The first point therefore behaved like a one-sample RMS and started near the instantaneous peak. SIGRA instead grows into the one-cycle RMS envelope from a low value.
2. After one complete cycle was available, the renderer used both endpoints of `[t-T, t]`. With a 1 kHz / 50 Hz record this is 21 samples, not 20. A stationary sinusoid therefore showed a false ripple although its true one-cycle RMS is constant.

The reference record makes the defect measurable. For UL1T, the corrected 20-sample rolling RMS settles at approximately `57.724 V`. The former 21-sample inclusive renderer oscillates by several volts around that value, while SIGRA remains essentially flat.

## Correct semantics

RMS rendering and cursor measurement now share one deterministic rule:

- one-cycle window is `(t-T, t]`;
- normalization uses the full number of sample slots in one nominal cycle;
- before enough pre-history exists at the beginning of a finite record, unavailable leading slots are zero-filled by retaining the full-cycle denominator;
- malformed/NaN samples that are actually present in the record remain excluded according to ArdIREC's damaged-data policy; they are not silently invented as zero;
- local timestamp spacing is used to infer cycle sample count, so COMTRADE sample-rate changes are followed locally instead of assuming one global rate.

For 50 Hz at 1 kHz this gives exactly 20 sample slots per cycle. The first synthetic fixture sample is therefore divided by `sqrt(20)`, and every complete steady-state window remains 20 samples rather than 21.

## Performance

The RMS renderer remains viewport-bounded. It still emits at most about two vertices per horizontal pixel and computes only one-cycle windows for those output samples. The shared window helper uses a fixed-size timestamp neighborhood with no heap allocation and a constant-time regular-sampling fast path; binary search is only a fallback for irregular timestamps/rate transitions.

## Regression gates

`ardirec_analysis_tests` now locks:

- finite-record startup normalization;
- expected RMS after the first complete cycle;
- absence of the N+1 endpoint ripple as the cursor advances across a stationary 50 Hz waveform;
- existing phasor, distance and locus numerical contracts.

Windows packaged-startup smoke testing remains mandatory before merge.
