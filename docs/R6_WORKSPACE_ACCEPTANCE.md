# R6 Workspace / MDI Acceptance Contract

R6 is a workstation-quality recovery phase for **ArDiRec — Ari Disturbance Recorder**. It starts from the manual Windows acceptance of R5.5 candidate PR #90 at head `88202432e376eef0be739df26fe250e7f4935f27`, which exposed interaction and performance defects that are outside the R5 numerical/cursor/Table/Locus recovery scope.

R6.0 does **not** fix the defects. It makes them impossible to silently redefine away: each blocker is represented in `scripts/r6_workspace_contract.json` and probed by `scripts/check_r6_workspace_contract.py`. Normal CI accepts the current `known_fail` state; R6.5 will use `--require-release-ready` and reject every remaining blocker.

## Reference interpretation

The implementation target is deliberate ArDiRec behavior, not folklore about "default Windows MDI":

- Microsoft Win32 MDI defines child windows inside an MDI client and states that clipped MDI children are confined to that client.
- Qt's `QMdiArea` is a `QAbstractScrollArea`-based MDI manager, but its scrollbars are off by default.
- Qt Quick `SplitView` demonstrates a managed shared-boundary model where one handle controls the space allocation between adjacent items.
- Qt Quick scene-graph guidance expects custom items to reuse the node returned as `oldNode` when possible rather than reconstructing unchanged rendering state.

ArDiRec therefore explicitly chooses two workspace modes:

### Free workspace

Free mode is a virtual desktop. Child logical coordinates are independent of the visible viewport. Moving a child beyond the current right/bottom viewport grows the logical content extent and exposes horizontal/vertical scroll range. Child motion must remain pointer-relative; the child is not clamped merely because the viewport edge has been reached. Dragging near a viewport edge may auto-scroll while preserving the pointer-relative grab offset.

Negative logical coordinates may still be constrained so users cannot permanently lose a title bar beyond the reachable top/left origin.

### Tile workspace

Tile mode is a managed split topology, not just a one-time assignment of independent rectangles. Adjacent panes that share a boundary also share the resize operation. Dragging that boundary moves it once and atomically updates both neighbors while preserving the occupied tile extent, minimum sizes, no overlap, and no gap.

Manual movement of an individual tiled child must not silently break the topology. Leaving Tile should explicitly return to Free mode with a defined geometry policy.

## Locked release blockers

### R6.1 — Child chrome / Lucide controls

Current child chrome uses text glyphs for minimize/maximize/restore/close. Release acceptance requires canonical Lucide-style SVG assets, proportional icon geometry, stable hit targets, and clear hover/pressed/active states. Window-state behavior itself must remain unchanged.

### R6.2 — Shared-edge tile resize

Current `tileHorizontal()` / `tileVertical()` assign rectangles once. Each `MdiChildWindow` then resizes independently. Release acceptance requires a shared-boundary model with runtime regression for vertical, horizontal and multi-pane layouts.

### R6.3 — Virtual scrollable workspace and direct drag

Implemented in R6.3: Free mode now owns logical content extents independent of the visible viewport. A child may move or resize beyond the current right/bottom edge without being pulled back into view; the workspace grows around its logical rectangle and exposes horizontal/vertical scroll range through a `Flickable` viewport with attached scrollbars.

Title dragging updates the child geometry directly on every pointer event. Near a viewport edge, a bounded 16 ms auto-scroll step advances the viewport and offsets the dragged child by the exact same logical delta, preserving the pointer-relative grab point rather than making the frame lag behind the mouse. Top/left remain reachable and are still constrained at the logical origin.

Runtime regression covers width/height extent growth, visible scroll range, sequential unclamped geometry updates, and edge auto-scroll that preserves screen-space pointer-relative position. The path remains UI-geometry-only and does not trigger record traversal or engineering analysis.

### R6.4 — Phasor hot-path optimization

R5.2 fixed blank-frame flicker and retained QSG vector geometry. It did not establish an opening/scrub latency budget. R6.4 must add measured, deterministic performance regression using `std::chrono::steady_clock` on repository fixtures before claiming optimization.

The performance work must preserve:
- committed-frame / stale-while-revalidate behavior;
- one request owner for duplicate Phasor children;
- one immutable fundamental snapshot feeding sequence and vector panels;
- retained QSG geometry;
- no changes to qualified DFT/distance equations solely to improve latency.

## R6.5 strict gate

R6.5 may only pass after every R6 contract is declared and observed as `pass`, exact-head CI/CodeQL/Windows packaging is green, repeated interaction stress passes, and manual Windows acceptance confirms:

1. Lucide child controls are visually proportionate and behave correctly.
2. Shared tile boundaries resize both adjacent panes.
3. Free-mode child movement can extend the workspace and activate scrollbars without laggy viewport clamping.
4. Edge auto-scroll remains controllable.
5. Phasor first-open and cursor scrub are materially responsive without reintroducing flicker.
6. All R5 cursor/Table/Locus/lifecycle acceptance remains intact.

Only after R6.5 is complete should the project return to `0.2.0-rc.1` promotion.
