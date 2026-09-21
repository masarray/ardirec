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

Implemented in R6.1. MDI child minimize, maximize/restore and close actions now use canonical Lucide-style SVG resources rather than font/text glyphs. The existing `minus.svg` and `maximize-2.svg` resources are joined by packaged `copy.svg` for the maximized/restore state and `x.svg` for close.

All three controls share one `WindowChromeButton` implementation with a stable 28 × 26 px hit target, proportional 15 px icon geometry, explicit hover/pressed borders/backgrounds, active/inactive icon emphasis, tooltips and accessible names. Close keeps a restrained destructive hover/pressed treatment without changing the workstation title-bar palette.

Window-state semantics are preserved: normal exposes minimize/maximize/close; maximized substitutes the restore icon and keeps the same maximize-toggle request; minimized continues hiding minimize/restore while retaining close and title-bar restore behavior. Runtime regression verifies the hit targets, icon state transition and minimized visibility contract.

### R6.2 — Shared-edge tile resize

Implemented in R6.2. Tile mode owns a managed split topology rather than a one-time rectangle arrangement. Dragging a shared vertical or horizontal boundary updates the adjacent pair atomically, preserves their outer extent, keeps non-neighbor panes unchanged, and rejects independent free movement while Tile owns geometry. Runtime regression covers two-neighbor boundary movement, multi-pane stability, gap-free adjacency and occupied-workspace preservation.

### R6.3 — Virtual scrollable workspace and direct drag

Implemented in R6.3: Free mode now owns logical content extents independent of the visible viewport. A child may move or resize beyond the current right/bottom edge without being pulled back into view; the workspace grows around its logical rectangle and exposes horizontal/vertical scroll range through a `Flickable` viewport with attached scrollbars.

Title dragging updates the child geometry directly on every pointer event. Near a viewport edge, a bounded 16 ms auto-scroll step advances the viewport and offsets the dragged child by the exact same logical delta, preserving the pointer-relative grab point rather than making the frame lag behind the mouse. Top/left remain reachable and are still constrained at the logical origin.

Runtime regression covers width/height extent growth, visible scroll range, sequential unclamped geometry updates, and edge auto-scroll that preserves screen-space pointer-relative position. The path remains UI-geometry-only and does not trigger record traversal or engineering analysis.

### R6.4 — Phasor hot-path optimization

Implemented in R6.4. The qualified path now measures both synchronous first-open construction and real immutable fundamental-snapshot completion with `std::chrono::steady_clock`, then exercises a 250-update cursor scrub and sequential real-fixture snapshot requests under explicit latency budgets.

First-open work is reduced in two places. Heavy `PhasorDiagram` bodies are activated asynchronously only when their group approaches the scroll viewport; empty and distant groups no longer construct radial grids, labels and retained vector items merely because the Phasor child opened. Once a group has been activated it stays loaded, while off-screen groups detach from changing cursor snapshots so scrub updates do not rebuild invisible geometry.

The fundamental snapshot controller now suppresses identical committed or already-in-flight requests at the same source revision. This prevents multiple consumers from cancelling/restarting the same one-cycle DFT during view construction. Source revision is tracked explicitly so representation/document changes still force revalidation. `SequenceSummary` is a pure consumer of the child-local committed immutable snapshot and no longer owns a fallback request path.

The performance qualification also locks:
- committed-frame / stale-while-revalidate continuity during scrub;
- one request owner for duplicate Phasor children, with controller-level identical-request dedup as a second guard;
- one immutable fundamental snapshot feeding sequence and vector panels;
- retained QSG geometry from R5.2;
- no changes to qualified DFT/distance equations solely to improve latency.

## R6.5 strict gate

R6.5 automation qualification makes `python3 scripts/check_r6_workspace_contract.py --require-release-ready` a permanent CI gate. The desktop runtime qualification additionally runs 24 repeated MDI interaction cycles that alternate vertical/horizontal managed Tile resize, Cascade/Free transition, off-viewport movement, edge auto-scroll, minimize/restore, maximize/restore, next/previous activation, and transient child open/close. The stress test must finish with the original child count, reachable delegates, no active edge drag, and working virtual scroll range.

Automated R6.5 qualification must show every R6 contract declared and observed as `pass`, exact-head CI/CodeQL/Windows packaging green, existing R5 zero-flicker/Table/Locus/lifecycle tests intact, and the repeated interaction stress green.

**Manual Windows acceptance remains intentionally human-gated** and must confirm:

1. Lucide child controls are visually proportionate, readable and behave correctly at normal Windows scaling.
2. Shared tile boundaries feel direct and resize both adjacent panes without gaps or unexpected jumps.
3. Free-mode child movement can extend the workspace and activate scrollbars without laggy viewport clamping.
4. Edge auto-scroll remains controllable during real pointer drag.
5. Phasor first-open and cursor scrub are materially responsive without reintroducing flicker.
6. Existing Time Signals, linked cursors, Table, Locus and record lifecycle behavior still feels correct in the packaged build.

Only after that manual Windows acceptance is recorded may issue #97 be closed and the project return to `0.2.0-rc.1` promotion.
