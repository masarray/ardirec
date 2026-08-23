# Engineering reference conventions

- Time-domain one-cycle quantities use the nominal-frequency period ending at the active analysis cursor.
- DFT magnitude is RMS engineering magnitude.
- DFT phase is sine-wave phase position at the active cursor/reference instant, wrapped to (-180°, 180°].
- Harmonics are never calculated above the Nyquist-resolvable integer harmonic order inferred from timestamps.
- Table last extremum is the most recent completed local turning point before the cursor.
- Compact DC presentation is absolute DC as a percentage of H1; signed absolute engineering DC is an optional detailed quantity.
