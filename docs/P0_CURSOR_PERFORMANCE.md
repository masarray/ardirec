# P0 cursor performance fix

## Problem reproduced

The Locus view used the cursor timestamps inside `buildDistanceSeries()`. Moving C1 or C2 therefore invalidated `earthSeries` / `phaseSeries`, rebuilt up to six full-record impedance trajectories, and then repainted the complete large QML `Canvas`. At pointer-event rate this put full-record DFT/impedance work and a large Canvas texture update on the interactive path.

The ruler also committed every mouse-position event directly to the application cursor state, so any downstream measurement binding ran at raw mouse event rate. Its 7 px diamond handles were visually small and had a narrow acquisition target.

## P0 architecture

The optimized path separates static and dynamic work:

1. `earthSeries` and `phaseSeries` contain full-record locus points only. They have no C1/C2 dependency.
2. The large `Canvas` is now a static layer for axes, grids, zones, legends, locus polylines and sample squares. It repaints only for data/model/representation/visibility/geometry changes.
3. Static panel transforms are cached after that paint.
4. C1/C2 crosses are Qt Quick scene-graph rectangles in a separate overlay, so cursor motion does not repaint or re-upload the large Canvas texture.
5. The ruler thumb follows the pointer locally on every input event. Backend cursor/analysis updates are coalesced to a 16 ms frame cadence and the exact final position is flushed on release.
6. Cursor handles are now 12 px diamonds inside a 16 px visual target, include a stem and numeric `1` / `2`, and use a 16 px hover acquisition radius.
7. `AnalysisController::distanceLoopsAt()` is available as a batched cursor-analysis path: six base phase V/I phasors are evaluated once per timestamp and all distance loops are derived from the shared snapshot. This is available for further hot-path consolidation without altering impedance semantics.

## Engineering constraints retained

- No locus geometry is simplified or cosmetically altered to gain speed.
- R-X mapping remains conformal.
- Existing kL and distance-current-floor semantics remain unchanged.
- RIO/XRIO zone interpretation and legacy SIGRA zone ordering remain unchanged.
- Cursor final values remain exact; only intermediate drag commits are coalesced.

## Acceptance

- Dragging either cursor must no longer trigger full-record `distanceLocus()` reconstruction.
- Dragging either cursor must no longer call `requestPaint()` on the large Locus Canvas.
- Cursor thumb position must respond immediately to the pointer even if downstream analysis takes a frame.
- Final cursor value after release must equal the snapped pointer position.
- Both cursor handles must be easily visible and easy to acquire.
- CI and Windows portable packaging must pass before P0 is considered ready for user validation.
