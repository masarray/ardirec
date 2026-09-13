# Locus engine contract

ArdIREC treats the R-X locus as an engineering calculation first and a visualization second. Display code must never invent, clip, or rewrite impedance values to make a plot look familiar.

## Numerical convention

- COMTRADE timestamps and channel metadata are authoritative. Phase metadata is preferred over channel-name heuristics.
- Voltage/current vectors are fundamental-frequency RMS phasors calculated by a full-cycle DFT over a trailing one-cycle window ending at the requested sampling instant.
- The measuring window is always to the left of the reference instant. A status change (fault inception, trip/disconnection, acquisition gap, or equivalent event) inside that window invalidates the calculated point and creates an explicit trajectory gap.
- SIGRA reference direction is `IE = -(IL1 + IL2 + IL3) = -3I0`. A measured residual/earth-current channel is preferred when its semantics are known; otherwise the residual is reconstructed from phase currents.
- Phase-phase loops use line-line voltage and line-current difference on the phase-loop impedance plane.
- For classical Siemens/SIGRA earth compensation, `RE/RL` and `XE/XL` remain independent real factors. They are never collapsed into a synthetic complex `kL` using an assumed line angle. A complex `kL` path remains available only for formats that explicitly provide it or provide enough line data for an unambiguous conversion.
- A low measuring-current condition is invalid, not an arbitrarily large finite impedance.
- C1/C2 readouts and the rendered trajectory must use exactly the same channel mapping, DFT frequency, timestamp weighting, measuring window, residual-current convention, compensation model, current floor, and validity rules.

## Frequency policy

The calculation frequency is selected once during the background document-load pipeline and then shared by cursor snapshots and Locus workers.

1. Prefer a bounded prefault positive-sequence space-vector estimate from the three phase voltages.
2. If a valid voltage triplet is unavailable, try the three phase currents.
3. Inspect at most 8,192 prefault samples and reject weak, implausible, or poorly fitted angle progression.
4. Fall back explicitly to the COMTRADE nominal frequency when no qualified prefault estimate exists.

The status bar exposes both `fn` (COMTRADE nominal) and `fcalc` plus its provenance. The UI must never imply that a derived frequency is active when the nominal fallback is being used.

Frequency estimation remains outside paint, scene-graph, and pointer paths. Scrubbing C1/C2 does not rerun the prefault estimator.

## COMTRADE sampling and DFT weighting

COMTRADE can describe records with different sample-rate sections. The timestamp index is therefore the primary time coordinate and no renderer or numerical worker may assume one global sample interval.

A full-cycle DFT uses causal timestamp-cell quadrature:

- the window starts exactly one calculation-frequency period before the requested instant, clipped only by the beginning of the record;
- no sample later than the requested instant can contribute;
- each sample is weighted by its local midpoint cell clipped to the exact window boundaries;
- phasor normalization uses accumulated time weight, not sample count;
- the same kernel is used by shared C1/C2 snapshots and native Locus trajectories.

This prevents a higher-rate section from receiving disproportionate numerical weight when a measuring window crosses a COMTRADE rate boundary. Numerical calculation is never resampled merely to make a trajectory smoother.

The R1.2 production regression fixture crosses a 1000 -> 2000 sample/s boundary inside a one-cycle window while carrying harmonic/DC contamination and a 50.4 Hz fundamental against a 50 Hz nominal declaration. Both cursor and Locus paths must recover the same fundamental impedance within tolerance.

## Correct impedance plane

Distance-element loci and their operating characteristics must be shown on the same engineering plane. Earth loops (`L1-E`, `L2-E`, `L3-E`) and phase loops (`L1-L2`, `L2-L3`, `L3-L1`) are distinct calculation families and have independent transforms. Positive-sequence load/power-swing quantities must not be silently overlaid as if they were distance-element loop impedances.

Protection zones imported from RIO/XRIO are overlays only. They do not modify the measured trajectory.

## Analysis/render separation

The numerical worker and renderer have separate budgets:

1. The async latest-wins worker evaluates up to 65,536 source instants for a request.
2. Calculated trajectories stay in compact native buffers.
3. Invalid samples delimit trajectory segments.
4. Only after calculation, each segment is shape-simplified to the bounded native render budget (maximum 4,096 points).
5. The QML layer never materializes per-point maps for production rendering.
6. `LocusTrajectoryItem` renders dynamic trajectories with retained Qt Quick scene-graph geometry. Canvas is reserved for low-frequency static grid/axis/zone painting.
7. Render density is bounded by the plot size; zoom/fit changes transforms, not numerical results.

This preserves engineering shape information without asking the UI thread or GPU to process every source sample.

## View policy

Earth and phase-phase diagrams have independent transforms and equal R/X engineering scale inside each plot so trajectory angles and zone geometry are not visually distorted.

- **Fit Relevant** is the default investigation view. It includes protection zones, committed C1/C2 values, and robust finite-trajectory context. It changes only the transform, never the data.
- **Fit All** exposes the complete finite native trajectory for forensic inspection.
- Exact C1/C2 values remain available even when parts of a trajectory are outside the current viewport.
- The toolbar states the active calculation convention. Classical RIO data displays `RE/RL` and `XE/XL`; it must not show a derived `kL` that is not present in the source.
- Status-window gaps are reported compactly rather than drawn as thousands of UI objects.
- Per-sample markers are deliberately not required at high point density. If added, they must be adaptive/bounded; C1/C2 markers remain the primary exact-time affordance.

## Performance invariants

- No DAT traversal or DFT in `updatePaintNode()`, Canvas paint handlers, or pointer handlers.
- Heavy work remains cancellable and off the GUI/render thread.
- Prefault frequency estimation is bounded and performed during background document load.
- Cursor requests are latest-wins.
- Native render geometry remains bounded.
- Zooming changes transforms only; it must not alter numerical results.
- Optional per-sample markers must be native/bounded and enabled only when point density is low enough to remain useful.

## Qualification

A locus change is not complete until all of the following remain green:

- distance core tests;
- production-path async cursor/Locus golden tests, including the multi-rate boundary fixture;
- recovery correctness contract;
- performance contract;
- large-record contract;
- CodeQL;
- Windows packaged application startup smoke.

The SIGRA parity fixture intentionally combines explicit COMTRADE phase metadata, a measured `IE` channel, a classical `RE/RL` + `XE/XL` RIO sidecar, and a binary transition so those semantics cannot regress independently. The multi-rate fixture separately locks frequency provenance and timestamp-weighted DFT behavior.

## External engineering references

The implementation contract above is aligned with these public references:

- IEC 60255-24 / IEEE C37.111 COMTRADE: transient waveform/event exchange format and multiple sampling-rate support.
- Siemens SIPROTEC SIGRA manual: full-cycle DFT, one-cycle backward measuring window, calculated-value invalidation on status change, prefault frequency determination/fallback, classical `RE/RL` + `XE/XL` impedance calculation, RIO/XRIO trip-zone overlays, and cursor-value tables.
- SEL, *Identifying the Proper Impedance Plane and Fault Trajectories in Distance Protection Analysis*: the measured trajectory and protection characteristic must be compared on the correct impedance plane.
- OMICRON TransView: separate time/vector/locus/table views, impedance loci with imported distance zones, and cursor-oriented engineering analysis.
- OMICRON, *Different Representations of the Ground Impedance Matching*: `Kr = RE/RL` and `Kx = XE/XL` are an independent real-value representation; conversion to a complex `kL` requires additional line information.
