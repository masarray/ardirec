# P0 Locus UX + cursor parity

Reference case: Siemens SIGRA V4.62 sample `ligne_1.CFG` with sibling DAT/HDR/RIO.

## UX acceptance

1. Keep the current dual conformal R-X panels, zone geometry and SIGRA-style loop colors.
2. Make the panel legend an operator control surface:
   - every zone and loop has a compact checkbox/show-hide state,
   - clicking a loop label also makes that loop the inspected loop,
   - keep all loops visible by default.
3. Reduce trajectory sample-marker screen spacing from ~10 px to ~6.5 px and keep hollow square markers.
4. Strengthen the selected loop only slightly; non-selected loops remain visible.
5. Cursor markers must be much easier to see while scrubbing:
   - draw C1 and C2 `+` markers on every visible loop, not only the inspected loop, matching SIGRA semantics,
   - use larger crosses (roughly 7 px half-size on selected loop, 5.5 px otherwise) and thicker stroke,
   - C1 remains amber/orange and C2 cyan/blue,
   - selected-loop cursor markers may receive a small white halo/under-stroke so they stay legible over dense locus lines.
6. Reclaim vertical space: replace the stacked Earth-kL/status/CONFORMAL rows with one compact protection status strip while preserving editability of kL and the explicit uncompensated-earth warning.
7. Increase legend/axis readability by 1–2 px without introducing large fonts/cards; grid remains subordinate.
8. Add small `EARTH LOOPS` and `PHASE-PHASE LOOPS` labels to make panel identity explicit.

## Right-side trajectory audit (`ligne_1`)

SIGRA shows the pre-fault operating point near R≈58 ohm and then three earth-loop branches (L1E left toward fault, L3E up/right, L2E down/right). ArdIREC already contains the same three core branches, but its low-current tail around breaker opening differs slightly from SIGRA (notably an extra/shifted L2E segment). Do **not** cosmetically bend or offset the curve.

Audit the calculation/plot continuity around the current-collapse interval. Preserve raw calculated points and only change rendering validity/continuity if an engineering condition is justified. Any numerical change must have a regression test and must not alter healthy phase-phase locus results. If exact SIGRA low-current behavior cannot be proven from the available single-line record, leave the calculation unchanged and document that this remains P2 numerical-parity work (mutual/parallel-line compensation and SIGRA validity semantics).

## Build gates

- Qt desktop build/test green.
- Core CI Windows/Linux/macOS green.
- Windows portable artifact green.
- No regression to Primary/Secondary recalculation, legacy SIGRA RIO ordering, dashed zones, or earth compensation warning.
