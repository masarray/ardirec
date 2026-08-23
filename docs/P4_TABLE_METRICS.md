# Table metric semantics

All table metrics are evaluated at the single Table Cursor.

- Instant: nearest recorded sample at the cursor timestamp.
- RMS: true RMS over the trailing nominal-frequency cycle ending at the cursor.
- H1 RMS / H1 angle: full-cycle DFT fundamental over the same trailing cycle.
- Cycle extremum: signed recorded sample having the greatest absolute magnitude inside the same trailing cycle.
- DC: signed arithmetic mean of the same finite cycle samples.
- THD: sqrt(sum(H2..H5+ available spectrum orders squared)) / H1 for the spectrum engine's configured range. The table currently requests H1..H5, so table THD is a quick local snapshot; Harmonics remains the full H10/H15/H25/H50 spectrum view.
- H2/H1, H3/H1, H5/H1: harmonic RMS magnitude divided by H1 RMS, percent.

Absolute quantities use the active Primary/Secondary representation. Percentage and phase quantities are invariant.
