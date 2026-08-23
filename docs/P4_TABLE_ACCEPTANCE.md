# P4 Table acceptance checklist

- A 3V + 3I record shows all six electrical analog rows without changing measuring signal.
- Table mode shows one movable shared analysis cursor only; no duplicate C2 columns.
- Cursor snap to digital edges remains available.
- Every row exposes Instant, RMS, H1, angle, signed cycle extremum, signed DC, THD, H2/H1, H3/H1 and H5/H1.
- Primary/Secondary scales Instant/RMS/H1/extremum/DC, while angle/THD/harmonic ratios remain invariant.
- Electrical/Voltage/Current/Visible/All scopes do not move the cursor.
- Record/Signal/RMS/THD sorting does not recompute unchanged snapshots.
- 50+ analog channels remain responsive through virtualized rows and bounded snapshot caching.
- Time/Phasor/Locus/Harmonics regressions remain green.
- Windows portable ZIP is produced after CI, CodeQL and desktop build/test are green.
