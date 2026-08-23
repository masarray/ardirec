# alpha.6 validation checklist

This release introduces deterministic RMS/DFT analysis surfaces. Before promoting beyond alpha, compare synthetic and real records against trusted reference tools.

## Required checks

- Balanced 3-phase 50 Hz: L1/L2/L3 phasor magnitudes equal and angles approximately 120° apart.
- Known-amplitude sine: one-cycle RMS equals amplitude / sqrt(2).
- Injected 3rd/5th harmonics: harmonic bars recover known RMS ratios.
- Cursor movement: phasor/harmonic/table values update without reopening the record.
- Digital edge snap: C1/C2 retain exact transition timestamps in every view.
- Raw locus: simple same-phase V/I produces the expected R-X point; do not compare against compensated distance-loop impedance until that gate is implemented.
- Phase colors remain L1 red, L2 yellow, L3 blue, E/N green across current and voltage.
- Trigger remains fixed and visually distinct from movable cursors.
