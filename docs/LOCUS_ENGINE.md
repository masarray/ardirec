# Locus engine contract

ArdIREC treats the R-X locus as an engineering calculation first and a visualization second. Display code must never invent, clip, or rewrite impedance values to make a plot look familiar.

## Numerical convention

- COMTRADE timestamps and channel metadata are authoritative. Phase metadata is preferred over channel-name heuristics.
- Voltage/current vectors are fundamental-frequency RMS phasors calculated over a trailing one-cycle window ending at the requested sampling instant.
- The window follows actual COMTRADE timestamps; no fixed sample-count assumption is allowed in UI code.
- A status transition inside the trailing measuring window invalidates calculated quantities for that instant. The native locus buffer stores an explicit gap, so a renderer cannot connect across the invalid interval.
- SIGRA reference direction is `IE = -(IL1 + IL2 + IL3) = -3I0`. A measured residual/earth-current channel is preferred when its semantics are known; otherwise the residual is reconstructed from phase currents.
- Phase-phase loops use line-line voltage divided by line-current difference.
- For classical Siemens/SIGRA earth compensation, `RE/RL` and `XE/XL` remain independent factors. They are never collapsed into a synthetic complex `kL` using an assumed line angle. A complex `kL` path remains available for formats that explicitly provide it.
- A low measuring-current condition is invalid, not an arbitrarily large finite impedance.

## COMTRADE sampling

COMTRADE can describe records with different sample-rate sections. ArdIREC therefore keeps the timestamp index as the primary time coordinate. A future multi-rate qualification fixture must cover a one-cycle window crossing a sample-rate boundary before changing the DFT weighting model; the current implementation must not infer a single global sample interval in the renderer.

## Analysis/render separation

The numerical worker and renderer have separate budgets:

1. The async latest-wins worker evaluates up to 65,536 source instants for a request.
2. Calculated trajectories stay in compact native buffers.
3. Invalid samples delimit trajectory segments.
4. Only after calculation, each segment is shape-simplified to the bounded native render budget (maximum 4,096 points).
5. The QML layer never materializes per-point maps for production rendering.
6. `LocusTrajectoryItem` renders dynamic trajectories with Qt Quick scene-graph geometry. Canvas is reserved for low-frequency static grid/axis/zone painting.

This preserves shape information without asking the UI thread or GPU to process every source sample.

## View policy

Earth and phase-phase diagrams have independent transforms.

- **Fit Relevant** is the default investigation view. It includes protection zones, committed C1/C2 values, and a robust finite-trajectory extent. It changes only the transform, never the data.
- **Fit All** exposes the complete finite native trajectory for forensic inspection.
- C1/C2 markers and numeric R/X readouts use the same snapshot, current convention, compensation model, current floor, and status-window validity as the trajectory.
- The toolbar must state the active calculation convention. Classical RIO data displays `RE/RL` and `XE/XL`; it must not show a derived `kL` that is not present in the source.
- Status-window gaps are reported compactly rather than drawn as thousands of UI objects.

## Performance invariants

- No DAT traversal or DFT in `updatePaintNode()`, Canvas paint handlers, or pointer handlers.
- Heavy work remains cancellable and off the GUI/render thread.
- Cursor requests are latest-wins.
- Native render geometry remains bounded.
- Zooming changes transforms only; it must not alter numerical results.
- Optional per-sample markers, if added later, must be native/bounded and enabled only when point density is low enough to remain useful.

## Qualification

A locus change is not complete until all of the following remain green:

- distance core tests;
- production-path async locus golden tests;
- recovery correctness contract;
- performance contract;
- large-record contract;
- CodeQL;
- Windows packaged application startup smoke.

The SIGRA parity fixture intentionally combines explicit COMTRADE phase metadata, a measured `IE` channel, a classical `RE/RL` + `XE/XL` RIO sidecar, and a binary transition so those semantics cannot regress independently.
