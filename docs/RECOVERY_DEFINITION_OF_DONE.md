# Recovery Definition of Done

> **R5 release-readiness supersession (2026-09-14):** R4 remains the durable architectural/runtime qualification for issue #62, but manual Windows comparison exposed release-blocking behavior that R4 did not model. Therefore `0.2.0-alpha.22` is an architectural baseline, **not a release-qualified build**. Release readiness is now governed by `docs/R5_RELEASE_QUALITY_CONTRACT.md` and umbrella issue #71. R5 must complete through R5.5 before an RC is allowed.

# R4 — Recovery Definition of Done

This document is the durable acceptance map for issue #62. Recovery is complete only when the executable evidence below is green on the exact pull-request head and again on the merged `main` commit, together with CodeQL, the large-record contracts, and the staged Windows startup smoke test.

The R4 suite qualifies behavior; it does not replace the numerical and performance contracts established in R0–R3. The R1.2 DFT/Locus engine remains unchanged unless a regression test demonstrates a defect.

## C1/C2 isolation

`ardirec_r4_cursor_isolation_tests` loads a deterministic COMTRADE fixture, commits C1 once, then sweeps C2 through multiple timestamps. The complete C1 snapshot must remain byte-for-byte equivalent at the QVariant level. The test also reconstructs the production Phasor screen convention using the stable whole-record scale and proves C1 radial fraction, angle, normalized X, and normalized Y remain unchanged.

This closes the regression where moving C2 appeared to change C1.

## Locus golden parity

`ardirec_r4_locus_fit_tests` exercises the production asynchronous native Locus snapshot path against the known `distance_p1` fixture. The energized L1-E point must remain approximately `100 + j0 Ω`, the low-current tail must remain invalid/a gap, and the protection-relevant extents must remain bounded around valid trajectory data rather than being dominated by invalid low-current samples.

The existing `ardirec_locus_snapshot_tests` remains mandatory for SIGRA classical-grounding parity, status-window gaps, multi-rate timestamp-weighted DFT behavior, and native-buffer production rendering data.

## Cursor dimension

`ardirec_r4_qml_runtime_tests` instantiates the production `EventStrip.qml` under Qt Quick offscreen rendering. A narrow C1/C2 span must still render the millisecond label outside the dimension span, and the label must remain clamped inside plot bounds even near the viewport edge. Cycle text remains secondary.

## Dialog placement

`ardirec_r4_qml_runtime_tests` instantiates the production `TopBar.qml` inside an `ApplicationWindow`, opens About and COMTRADE Properties, and resizes the host through multiple desktop dimensions. Both modal popups must remain centered on the application overlay and fully inside the window.

## Close lifecycle

`ardirec_document_lifecycle_tests` covers three close paths:

- close an idle loaded record after record-scoped caches have been populated;
- close while the asynchronous document loader is active;
- close while real C1/C2 and Locus background analysis workers are active.

After close, the DAT mapping/store, LOD pyramid, RMS cache, timestamps, sidecar state, cursor snapshots, Locus snapshot, and scalar-cache access must be invalid. The close invalidation revision is allowed; any older worker callback arriving afterward must be generation-rejected and must not publish another revision.

## MDI semantics

`ardirec_r4_qml_runtime_tests` loads the production `MdiWorkspace.qml` and `MdiChildWindow.qml`, substituting only the heavy `AnalysisViewHost` content boundary with a lightweight test host. It then executes the production workspace methods for:

- initial Time Signals child creation;
- duplicate Time and duplicate Phasor creation;
- manual move and resize;
- activation and request-owner reassignment;
- minimize and restore;
- Cascade;
- Tile Horizontally;
- Tile Vertically;
- close/destroy.

Arrangement operations must preserve every child `viewType`. A closed child delegate must disappear rather than remain as hidden retained work.

## Quiescence

The MDI runtime test proves a minimized child receives `live=false` and cannot remain a heavy-work owner. The same suite also instantiates production `PhasorView.qml` with an instrumented shared cursor-snapshot probe: a visible duplicate without request ownership must issue no DFT request, while the single owner issues one shared C1/C2 request pair. Hiding/minimizing the view must stop further requests.

## Performance / shared engines

Four simultaneous MDI child windows are qualified against identical QObject identities for the document, analysis, Locus, harmonic-snapshot, and table-snapshot sources. Production startup must still construct exactly one instance of each global engine.

This runtime identity check complements the architectural performance contract:

- mmap/lazy DAT access remains shared;
- Min/Max waveform LOD remains viewport-bounded;
- one RMS tile cache is shared by the document;
- scalar caches stay bounded;
- waveform/phasor/locus high-frequency rendering stays native/QSG;
- MDI geometry operations perform no record traversal;
- minimized children are quiescent;
- rendering complexity remains tied to visible viewport pixels rather than source sample count multiplied by open windows.

The 10M-sample and 120-channel large-record jobs remain mandatory on every R4 qualification run.

## Release gates

R4 is not complete on compilation alone. The exact PR head and the merged `main` commit must both pass:

1. recovery contracts R0/R1/R2/R3/R4;
2. all CTest targets, including the headless Qt Quick R4 runtime test;
3. the performance architecture contract;
4. 10M-sample and 120-channel large-record contracts;
5. CodeQL;
6. Windows MSVC/Qt build and CTest;
7. Windows Qt + VC runtime staging;
8. packaged `ardirec.exe` startup smoke;
9. portable ZIP construction and artifact upload.

The R4 candidate is `0.2.0-alpha.22`. Issue #62 may be closed as completed only after the post-merge `main` evidence above is green. That R4 completion is historical architectural qualification only; R5 now controls release readiness.
