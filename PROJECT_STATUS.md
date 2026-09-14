# Project Status

## Current baseline

**`0.2.0-alpha.22` — architectural baseline, NOT release-qualified**

Current `main` baseline commit: `0aa1c14e78e284a4224b0699621ecd939f020238`.

R0 through R4 established the COMTRADE engine, scalable rendering/data architecture, internal MDI lifecycle, numerical regression fixtures, asynchronous analysis paths and packaged Windows smoke coverage. Manual Windows comparison after R4 exposed release-blocking interaction/visual regressions that the previous Definition of Done did not model.

## Current milestone

**R5 — Release Quality Recovery** (#71)

R5 adds no new product feature until the known release blockers are closed.

1. **R5.0 #72 — Lock reference & regression**: codify corrected acceptance contracts and freeze qualified numerical anchors.
2. **R5.1 #73 — Linked local cursors**: cursor presentation belongs to each MDI child while cursor state remains globally linked; Time/Phasor/Locus use C1+C2, Harmonics/Table use C1.
3. **R5.2 #74 — Phasor zero-flicker**: committed-frame/stale-while-revalidate snapshot semantics and retained QSG vector geometry.
4. **R5.3 #75 — Table zero-flicker**: asynchronous immutable table frames with stable layout during scrubbing.
5. **R5.4 #76 — SIGRA Locus parity**: stable engineering viewport, complete backward-cycle validity and canonical SIGRA comparison without casually changing qualified distance mathematics.
6. **R5.5 #77 — Release qualification**: stress, exact-head CI/CodeQL/Windows, post-merge validation and manual Windows/SIGRA gate before RC.

## Release blockers observed on alpha.22

- One global cursor strip is being used as a substitute for cursor presentation inside each MDI analysis window.
- Phasor scrubbing can transition committed data through an empty display state and the vector scene graph is rebuilt instead of retained.
- Table calculations are invoked synchronously through QML-facing snapshot calls; optional Sequence Components geometry can collapse/expand while data is pending.
- Locus default fit can depend on cursor/extreme trajectory context, producing a viewport materially different from the SIGRA investigation view.

These blockers are explicitly represented in `scripts/r5_release_contract.json`. Normal CI verifies that source state and the manifest agree. The strict R5.5 gate is:

```bash
python3 scripts/check_r5_release_contract.py --require-release-ready
```

It must fail until every release blocker has been deliberately promoted to `pass` by its implementation stage.

## Preserved architecture

R5 must retain the proven scalability and numerical architecture unless a dedicated regression demonstrates a defect:

- mmap/lazy DAT access;
- bounded Min/Max waveform LOD;
- shared RMS tile cache;
- asynchronous cancellable analysis workers;
- native retained Qt Quick scene-graph rendering for high-frequency plots;
- no record traversal on GUI/render-thread paint paths;
- bounded caches and quiescent minimized/closed views;
- shared production engines across MDI children;
- deterministic COMTRADE/distance regression fixtures.

The product priority remains disturbance-analysis correctness and operator usability first. `0.2.0-rc.1` is forbidden until R5.5 passes.
