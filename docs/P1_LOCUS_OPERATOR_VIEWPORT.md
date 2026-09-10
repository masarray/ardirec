# P1 — Locus operator viewport and hot-path consolidation

This phase follows the P0 cursor-performance work. It intentionally does not change distance-protection equations, RIO geometry, earth compensation, or the COMTRADE phasor window.

## Goals

- Keep C1/C2 interaction on the fast path. A cursor update must request all six distance loops through the batched `distanceLoopsAt()` API so the six base V/I phasors are calculated once per cursor timestamp.
- Make the R-X viewport operator-controlled while preserving a conformal impedance plane: one ohm in R must remain the same pixel distance as one ohm in X.
- Provide compact `-`, `Fit`, and `+` controls and mouse-wheel zoom for Locus mode. `Fit` restores the validated robust engineering frame.
- Keep the viewport stable when a user hides or shows a zone/trajectory. Visibility is a presentation choice and must not silently change axis scale.
- Preserve the static/dynamic render split from P0: cursor movement must not repaint the large Canvas or rebuild full-record loci.
- Preserve all current SIGRA-parity semantics: Earth and phase-phase panels, legacy zone ordering, dashed zones, hollow sample markers, interactive legend, Primary/Secondary conversion, and the current minimum-current validity guard.

## Non-goals

- Mutual/parallel-line compensation (`RM/RL`, `XM/XL`, `KS`, `ZS`) is P2 numerical parity work.
- No curve smoothing, coordinate offset, or cosmetic manipulation may be used to imitate SIGRA.
- No change to full-cycle DFT or distance-loop formulas is included here.

## Acceptance checks

1. `distanceLoopsAt()` agrees with the corresponding `distanceLoopAt()` result for all available loops at the same timestamp and grounding factor.
2. Dragging C1/C2 does not request a static locus repaint.
3. Hiding/showing a legend item leaves the R/X axis range unchanged.
4. Zoom remains conformal and is clamped to a useful operator range; `Fit` returns exactly to 100% robust auto-fit.
5. Mouse wheel zoom works only in Protection Locus mode and does not alter COMTRADE cursor time or calculation data.
6. CI, desktop QML build, CodeQL, and Windows portable packaging remain green.
