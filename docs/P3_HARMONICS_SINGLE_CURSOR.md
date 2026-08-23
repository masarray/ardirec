# P3 Harmonics single-cursor workflow

Harmonics intentionally uses one analysis cursor only.

- The Harmonic Cursor reuses the shared Cursor 1 timestamp from the investigation session.
- It keeps digital-edge snapping and the current shared time window.
- The spectrum window is one rated-frequency cycle immediately to the left of the Harmonic Cursor.
- Cursor 2 is hidden and inactive while Harmonics is selected.
- Leaving Harmonics restores the normal dual C1/C2 workflow in Time, Phasor, Locus and Table.

This removes ambiguity about which timestamp owns the displayed spectrum and frees vertical space for more simultaneous voltage/current rows.

## P3 visual/workflow policy

Default Harmonics scope is `Electrical`, which shows all recognized Voltage channels followed by all recognized Current channels in one virtualized vertical list. Scope controls can narrow the list to Voltage, Current, channels visible in Time, or All Analog.

Default spectrum mode is `Full`: H0/DC + H1 + H2..Hn. `Distortion` remains available for H2..Hn-only inspection.

H0/DC is the arithmetic mean of the same trailing one-cycle sample set. H1..H50 remain RMS DFT magnitudes. THD excludes DC and H1.
