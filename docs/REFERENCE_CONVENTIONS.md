# Engineering reference conventions

- Time-domain one-cycle quantities use the nominal-frequency period ending at the active analysis cursor.
- DFT magnitude is RMS engineering magnitude.
- DFT phase is sine-wave phase position at the active cursor/reference instant, wrapped to (-180°, 180°].
- Harmonics are never calculated above the Nyquist-resolvable integer harmonic order inferred from timestamps.
- Symmetrical components use the Fortescue transform with phase order L1-L2-L3: `X0=(L1+L2+L3)/3`, `X1=(L1+a·L2+a²·L3)/3`, `X2=(L1+a²·L2+a·L3)/3`, where `a=e^(j120°)`; sequence phasors preserve the source complex RMS convention.
- Table last extremum is the most recent completed local turning point before the cursor.
- Compact DC presentation is absolute DC as a percentage of H1; signed absolute engineering DC is an optional detailed quantity.
