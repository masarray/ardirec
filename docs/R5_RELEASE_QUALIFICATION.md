# R5.5 Release Qualification

R5.5 is a qualification stage, not a new feature stage. It closes the release-quality recovery started by #71 after the linked-cursor, Phasor, Table, and SIGRA Locus fixes are already implemented.

## Automated gate

The candidate must satisfy all existing recovery contracts and the strict R5 release contract. CI must also repeat the release-blocking runtime paths three consecutive times with `ctest --repeat until-fail:3`:

- document close/open lifecycle, active-worker cancellation, and stale-result rejection;
- production internal-MDI duplicate windows, minimize/restore, cascade/tile, close/destroy, and request-owner quiescence;
- linked local C1/C2 presentation and cross-window fan-out;
- Phasor committed-frame/latest-wins and retained-QSG continuity;
- Table async atomic-frame/latest-wins and stable geometry/scroll behavior;
- SIGRA-oriented Locus full-backward-cycle and cursor-independent viewport qualification.

The existing large-record contract remains mandatory at 10 million samples (~210 MiB) and 120 analog channels (~238 MiB). CodeQL and the Windows packaged-startup smoke are also mandatory on the exact PR head.

## Candidate identity

The first R5.5 manual-validation artifact is `0.2.0-alpha.23`. It exists only to make the post-R5.4 Windows artifact unambiguous. It is not an RC and must not be published as one.

## Manual Windows/SIGRA gate

The final non-automatable release gate uses the original user-supplied `ligne_1` CFG/DAT/HDR/RIO and SIGRA Circle Diagrams screenshots. These inputs stay outside the repository unless the owner explicitly decides otherwise.

On the exact Windows artifact that already passed CI, CodeQL, CTest stress, runtime staging, and packaged startup smoke, verify:

1. Open the original `ligne_1` CFG and allow its RIO sidecar to load.
2. Open R-X Locus in Fit Relevant with Secondary values.
3. Confirm C1/C2 remain overlays only: moving either cursor must not rescale the default Locus viewport.
4. Confirm the protection-context view is SIGRA-like: approximately ±100 ohm horizontally and ±15 ohm vertically for the supplied reference layout, with one shared px/ohm scale so R/X geometry is not distorted.
5. Confirm earth-loop and phase-phase trajectories have the same qualitative orientation/topology as the supplied SIGRA Circle Diagrams reference; remote finite poles may clip in Fit Relevant rather than forcing a forensic full-record scale.
6. Confirm Fit All still exposes the complete finite trajectory.
7. Confirm the local cursor controls remain linked across duplicate Time/Phasor/Locus windows and C1-only in Harmonics/Table.
8. Scrub continuously through C1/C2 and confirm Phasor/Table do not flash empty or collapse/reflow.
9. Minimize/restore duplicate MDI children and close/reopen the COMTRADE record; no stale frame or stuck busy state may remain.

Only after this manual Windows/SIGRA comparison passes may the candidate identity be promoted to `0.2.0-rc.1`. After the RC version commit, exact-head CI + CodeQL + Windows packaged smoke must run again, the PR must be merged with its expected head SHA, and the resulting merge commit on `main` must be validated again before #77 and umbrella #71 are closed.
