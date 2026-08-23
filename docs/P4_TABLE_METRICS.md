# Table metric semantics

All table metrics are evaluated at the single Table Cursor.

- Instant: nearest recorded sample at the cursor timestamp.
- RMS: true RMS over the trailing nominal-frequency cycle ending at the cursor.
- H1 RMS / H1 angle: full-cycle DFT fundamental over the same trailing cycle.
- Cycle extremum: signed recorded sample having the greatest absolute magnitude inside the same trailing cycle.
- DC: signed arithmetic mean of the same finite cycle samples.
- THD: `sqrt(sum(H2..H25^2)) / H1`, using the same one-cycle DFT window. H0/DC and H1 are excluded.
- H2/H1, H3/H1, H5/H1: harmonic RMS magnitude divided by H1 RMS, percent.

The Table view deliberately stops its compact THD snapshot at H25 for predictable interactive cost. Harmonics remains the detailed H10/H15/H25/H50 spectrum workstation.

Absolute quantities use the active Primary/Secondary representation. Percentage and phase quantities are invariant.
