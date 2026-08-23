# P4 manual test plan

Use a 3V+3I COMTRADE fixture.

1. Open Table: verify one Table Cursor, no C2 table columns.
2. Move cursor across pre-fault/fault/post-fault: all rows update without losing scope/sort/scroll.
3. Snap cursor to a digital edge and verify exact time is retained.
4. Compare Secondary vs Primary: absolute values scale, THD/H2/H3/H5/angle do not.
5. Scope Electrical shows all 3V+3I channels; Voltage and Current isolate each family.
6. Sort THD and RMS: descending order is correct and cursor does not move.
7. Verify signed extremum and DC against the same trailing-cycle samples.
8. Confirm Time/Phasor/Locus/Harmonics remain usable after switching back.
