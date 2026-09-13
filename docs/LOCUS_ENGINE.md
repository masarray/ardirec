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
- C1/C2 readouts and the rendered trajectory must use exactly the same channel mapping, DFT window, residual-current convention, compensation model, current floor, and validity rules.

## Frequency policy

SIGRA determines the calculation frequency from the prefault positive-sequence space vector when a valid prefault state is available, and falls back to the COMTRADE frequency otherwise. R1.1 keeps the COMTRADE nominal frequency as the calculation frequency because the EPRI parity record is a stable 50 Hz record; prefault-frequency estimation is a separate compatibility hardening item and must be qualified before replacing the nominal-frequency path.

The UI must never imply that a derived frequency is being used when only the COMTRADE nominal frequency is active.

## COMTRADE sampling

COMTRADE can describe records with different sample-rate sections. The timestamp index therefore remains the primary time coordinate and no renderer may assume one global sample interval.

R1.1 is qualified for the current single-rate production path. Before multi-rate records are declared SIGRA-equivalent, add a golden fixture whose one-cycle DFT window crosses a sample-rate boundary and qualify the weighting/interpolation policy against the actual timestamps. A renderer must never resample or reinterpret the numerical data merely to make the trajectory smoother.

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
- Cursor requests are latest-wins.
- Native render geometry remains bounded.
- Zooming changes transforms only; it must not alter numerical results.
- Optional per-sample markers must be native/bounded and enabled only when point density is low enough to remain useful.

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

## External engineering references

The implementation contract above is aligned with these public references:

- IEC 60255-24 / IEEE C37.111 COMTRADE: transient waveform/event exchange format and multiple sampling-rate support.
- Siemens SIPROTEC SIGRA manual: full-cycle DFT, one-cycle backward measuring window, calculated-value invalidation on status change, prefault frequency determination/fallback, classical `RE/RL` + `XE/XL` impedance calculation, RIO/XRIO trip-zone overlays, and cursor-value tables.
- SEL, *Identifying the Proper Impedance Plane and Fault Trajectories in Distance Protection Analysis*: the measured trajectory and protection characteristic must be compared on the correct impedance plane.
- OMICRON TransView: separate time/vector/locus/table views, impedance loci with imported distance zones, and cursor-oriented engineering analysis.
- OMICRON, *Different Representations of the Ground Impedance Matching*: `Kr = RE/RL` and `Kx = XE/XL` are an independent real-value representation; conversion to a complex `kL` requires additional line information.
