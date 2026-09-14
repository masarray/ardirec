# R5 — Release Quality Contract

R5 exists because `0.2.0-alpha.22` proved the R0–R4 architectural and numerical regression suite can be green while the packaged Windows application still has release-blocking interaction defects. R4 remains valuable regression evidence, but it is **not** a release-readiness certificate.

Umbrella issue: #71. R5.0 contract issue: #72.

## Baseline

- Baseline version: `0.2.0-alpha.22`
- Baseline commit: `0aa1c14e78e284a4224b0699621ecd939f020238`
- Release qualified: **no**
- R5 implementation order: R5.0 #72 -> R5.1 #73 -> R5.2 #74 -> R5.3 #75 -> R5.4 #76 -> R5.5 #77

R5 adds no product feature before these release blockers are closed.

## Frozen numerical anchors

R5.0 deliberately does **not** change production DFT or distance equations. The following repository blobs are locked by `scripts/r5_release_contract.json` and checked by `scripts/check_r5_release_contract.py`:

- `core/src/distance/distance.cpp`
- `tests/data/distance_sigra_parity.cfg`
- `tests/data/distance_sigra_parity.dat`
- `tests/data/distance_sigra_parity.rio`
- `tests/data/ligne_1_sigra.rio`

A later R5 stage may change a locked numerical anchor only when its issue contains executable golden evidence proving a numerical defect and the manifest is deliberately updated in the same review.

`timestamped_dft.hpp` is not blob-frozen because R5.4 must qualify the complete backward-cycle validity rule. Any change there still requires numerical regression coverage.

## Reference limitation recorded by R5.0

The repository currently contains the deterministic `distance_sigra_parity` CFG/DAT/RIO fixture and the `ligne_1_sigra.rio` protection reference. The original complete `ligne_1` CFG/DAT/HDR set used for manual SIGRA comparison is not versioned in the repository. R5.0 therefore does not invent or synthesize that missing raw reference.

R5.4 must either add an appropriately redistributable canonical full record with traceable expected values or keep the original record as an explicit manual Windows/SIGRA qualification input. Release qualification must state which path was used.

## Release-blocking contracts

### R5.1 — linked local cursors (#73)

Cursor **state** is shared globally, but cursor **presentation and interaction** belong to each MDI analysis child.

- Time Signals: C1 + C2
- Phasor: C1 + C2
- Locus / Circle diagram: C1 + C2
- Harmonics: C1 only
- Table: C1 only

Moving C1 or C2 in any applicable child updates the same cursor in every other applicable child. A single global cursor strip above the MDI workspace is not an acceptable substitute.

R5.1 implements the presentation at the `AnalysisViewHost` boundary. That host exists once inside every MDI child, so duplicate windows each own a local interaction surface while `MdiWorkspace` and `Main` retain the single linked C1/C2 state. This avoids copying cursor plumbing into the heavy Phasor/Locus/Table calculation views and preserves one shared analysis state. Minimized children hide their local cursor surface with the rest of the non-live host. `ardirec_r5_linked_cursor_tests` executes the MDI signal path and proves C1/C2 fan-out plus the C1-only Harmonics/Table contract.

### R5.2 — Phasor committed-frame continuity (#74)

Cursor scrubbing must never clear a previously committed valid Phasor frame merely because a newer asynchronous result is pending.

Required model:

`committed frame -> pending calculation while committed frame stays visible -> atomic commit of newer frame`

The display must not use `valid -> empty -> valid` as a synchronization mechanism. Static polar grid/axis/legend geometry must remain stable while only vector data changes. QSG vector geometry must be retained and updated rather than deleting the complete old scene node on each update.

### R5.3 — Table atomic-frame stability (#75)

Heavy per-channel table analysis must not execute synchronously from QML bindings or delegates during scrub. One complete immutable table frame is calculated away from the GUI path and committed atomically. The previous committed frame remains visible until the new frame is complete.

During pending work:

- row count does not oscillate;
- vertical geometry does not collapse/expand;
- scroll position is preserved;
- optional calculated/sequence information cannot create a mandatory height-changing block.

### R5.4 — SIGRA Locus parity (#76)

Qualified DFT/distance equations are not changed merely to make a plot look familiar. R5.4 first corrects measurement-window validity and view policy, then changes mathematics only if a golden test proves it necessary.

Required behavior:

- a calculated one-cycle value requires a complete backward measurement cycle;
- C1/C2 position does not participate in automatic viewport scale selection;
- low-current poles/outliers do not dominate the default investigation viewport;
- complete finite trajectory remains available through a forensic Fit All path;
- trajectory, zones, orientation, C1/C2 markers and default viewport are compared against the SIGRA reference.

### R5.5 — release gate (#77)

The strict gate is:

```bash
python3 scripts/check_r5_release_contract.py --require-release-ready
```

It must fail until every manifest contract has graduated from `known_fail` to `pass` and the corresponding source probe observes the corrected state.

R5.5 additionally requires exact-head CI, CodeQL, Windows build/CTest, packaged startup smoke, scrub/MDI lifecycle stress, merge with expected head SHA, post-merge `main` validation, and manual Windows/SIGRA comparison before `0.2.0-rc.1` is allowed.

## Known-failure accounting

R5.0 intentionally records alpha.22 defects as `known_fail`; it does not hide them and it does not make the normal CI job fail forever. The normal checker succeeds only when the observed source state exactly matches the manifest expectation. If a defect is fixed but its contract is not deliberately promoted to `pass`, the checker fails and forces the change to update the acceptance record.

This gives R5 two useful modes:

- normal CI: prove the repository and manifest agree about remaining blockers;
- `--require-release-ready`: prove there are **zero** remaining blockers.

A stage is not complete because documentation says so. Each R5.1–R5.4 implementation PR must promote its own contract(s) to `pass`, add runtime/numerical tests where appropriate, pass exact-head gates, merge, and pass the same gates again on `main`.
