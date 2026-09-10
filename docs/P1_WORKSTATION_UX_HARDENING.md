# P1 Workstation UX Hardening

This milestone responds to operator feedback from the P0 large-record Windows build. It is a presentation and interaction hardening pass; protection calculations, COMTRADE parsing and distance numerical semantics are not cosmetically altered.

## Operator defects addressed

1. **Phasor readability** — each phasor panel uses a square polar plane, circular magnitude rings, dotted 30-degree spokes, strong 0/90/180/270-degree quadrant axes, degree labels, relative ring scale and exact magnitude/angle legend values.
2. **Cursor-induced phasor spin** — the phasor UI consumes `AnalysisController::phasorAt()`, whose DFT basis uses the fixed record start reference. The displayed sine-wave engineering angle applies the established +90-degree conversion. A steady sinusoid therefore keeps a stable phase direction as the trailing one-cycle cursor window advances.
3. **Dead menu labels** — the decorative menu text was replaced by functional File, View, Signals and Help menus.
4. **Excess top chrome** — menu and quick commands now share one 40 px workstation row. The view-mode row remains separate because it describes the active analysis surface.
5. **Primary/Secondary toggle stall** — heavy hidden Phasor/Locus/Harmonics/Table analysis inputs are suspended when their view is inactive. A representation switch no longer rebuilds hidden R-X locus or spectrum work while the operator is using another view.
6. **Meaningless Active/All toggle** — digital filtering controls appear only when the record contains both active and inactive configured digital channels. A record with 2/2 active channels no longer shows two controls that produce the same result.
7. **Waveform edge gap** — instantaneous and RMS scene-graph renderers retain one real neighbor sample outside each visible boundary and clip its X coordinate to the chart edge. This visually interpolates the trace to the viewport boundary without changing the underlying samples or engineering calculations.
8. **Hard-to-grab C1/C2 thumbs** — ruler thumbs are larger, centered vertically and use a 20 px acquisition radius. The existing local-preview plus 16 ms analysis coalescing path is retained for smooth dragging.

## Regression gates

- `ardirec_analysis_tests` verifies fixed-reference phasor angle stability across cursor movement on a stationary 50 Hz waveform.
- Existing distance/locus numerical tests must stay green.
- Core and desktop CI must stay green across supported runners.
- CodeQL must stay green.
- The packaged Windows portable build must pass the executable startup smoke test before merge.

## Performance rule

A UI representation change must update the active view only. Hidden heavy analysis surfaces must not consume synchronous UI time. If future views add expensive derived data, they must be gated by visibility/activation or moved to cancellable background work before release.
