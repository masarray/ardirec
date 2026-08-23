# P3 Harmonics single-cursor UX

Harmonics uses one analysis cursor only. The cursor is the same shared Cursor 1 timestamp used by the investigation session and keeps digital-edge snapping through the common cursor navigator.

Cursor 2 remains part of Time/Phasor/Locus/Table workflows but is intentionally hidden and inactive while Harmonics is selected. This keeps spectrum interpretation unambiguous: every displayed spectrum is calculated from the one-cycle trailing window ending at the single Harmonics cursor.
