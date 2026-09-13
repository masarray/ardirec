# AGENTS.md — ArdIREC Engineering Rules

These rules apply to every AI/code agent working in this repository. ArdIREC is a professional COMTRADE disturbance-analysis workstation; correctness, field robustness, deterministic behavior, and interactive performance are product requirements, not later polish.

**Mandatory performance gate:** every agent must read and obey [`docs/PERFORMANCE_CONTRACT.md`](docs/PERFORMANCE_CONTRACT.md) before changing rendering, interaction, numerical analysis, record loading, caching, or large-data code. The performance contract is an architectural invariant enforced by CI, not advisory guidance. A change that reintroduces synchronous heavy analysis, record-sized render geometry, stale publication, or high-frequency Canvas rendering is incomplete even when its numerical output and screenshots look correct.

## 1. Production-ready from the first implementation

Do not ship a deliberately naive or throwaway implementation first when the production architecture is already known. Prefer the scalable path immediately. Avoid temporary shortcuts that force a later rewrite of file I/O, rendering, threading, or analysis pipelines.

Every change must preserve these priorities, in order:
1. engineering correctness and numerical parity;
2. crash resistance on real field data;
3. UI responsiveness;
4. bounded memory use;
5. maintainability and testability.

Never cosmetically alter engineering data to imitate a reference tool. Rendering may optimize representation, but source samples, phasors, impedance, protection zones, timestamps, and cursor values must retain correct semantics.

## 2. Defensive programming for field COMTRADE data

COMTRADE files from the field may be incomplete, truncated, malformed, contain empty/misaligned rows, invalid numbers, inconsistent channel counts, missing sidecars, unsupported metadata, NaN/Inf samples, or damaged tails.

For C++ code, use the language-appropriate equivalents of nullish coalescing / optional chaining:
- `std::optional`, `value_or`, explicit null checks, `QPointer`, `QVariant::value`, and safe default initialization;
- bounds checks before indexing vectors/containers;
- `std::isfinite()` before using floating-point data in analysis or rendering;
- safe fallbacks such as NaN/unknown state/diagnostic messages when a sample is unusable;
- never permit an uncaught parsing or I/O exception to escape into the Qt event loop.

For QML/JavaScript, use `?.` and `??` where supported and appropriate when reading optional objects/properties.

Parsing policy:
- salvage every valid frame before a truncated/damaged tail;
- a damaged ASCII row should be skipped or partially recovered with diagnostics when safe, rather than crashing the application;
- missing analog samples should become NaN, not invented engineering values;
- missing/invalid digital data should use an explicit safe/unknown fallback rather than out-of-range access;
- only unrecoverable structural errors should abort opening the record;
- user-visible diagnostics must explain what was recovered or ignored.

Add regression fixtures/tests for malformed, empty, truncated, misaligned, and inconsistent CFG/DAT combinations whenever parser behavior changes.

## 3. Large-file architecture: mmap/lazy access or streaming, never eager whole-file loading

Do not read an entire large `.DAT` file into a monolithic RAM blob. For BINARY/BINARY32/FLOAT32, prefer a memory-mapped, indexed, lazy-access architecture where practical. For ASCII or when mapping is unsuitable, use buffered streaming/chunk-by-chunk parsing.

Required properties:
- record size must not directly imply equivalent heap allocation;
- avoid an intermediate `vector<SampleFrame>` plus a second full per-channel copy for large records;
- index frame offsets/timestamps once and access only the ranges/channels needed by the active view or analysis;
- keep a streaming fallback for environments where mapping fails;
- mapping/stream errors must degrade cleanly and report diagnostics;
- large records must remain usable without arbitrary preview truncation as the final architecture.

Target cases from the performance contract remain mandatory: 10M samples, 100+ channels, and ~250 MB records are normal design cases, not exceptional cases.

## 4. Zero UI blocking

Any operation likely to exceed one frame (~16 ms) must not run synchronously on the GUI thread. This includes large file parsing/indexing, bulk RMS/phasor/locus generation, expensive transforms, exports, and other heavy analysis.

Use cancellable background workers (`QThread`, `QtConcurrent`, task/thread-pool infrastructure, or an equivalent tested design) and marshal only the minimal completed result back to the GUI thread.

Interactive rules:
- cursor/thumb motion must update visually in the same frame whenever possible;
- coalesce expensive analysis updates to a frame cadence instead of processing raw pointer-event rate;
- the exact final cursor value must be committed on release;
- never rebuild a full-record locus, FFT/DFT, or waveform merely because a cursor moved;
- static data and dynamic overlays must be separate render/update paths.

Hard prohibitions for production hot paths:
- no RMS/DFT/FFT/DAT traversal in `updatePaintNode()` or high-frequency QML paint callbacks;
- no synchronous full-record numerical analysis from QML bindings or pointer handlers;
- no one-`QVariantMap`/QObject/JavaScript-object-per-point transport for bulk plots;
- no raw pointer-event stream directly launching expensive analysis;
- no publication of an asynchronous result after its document/cursor/view generation is stale.

## 5. SIMD, vectorization, and parallel compute

Profile first, then optimize proven hotspots. Use compiler auto-vectorization and contiguous data layouts by default. For heavy numeric loops that remain dominant, use SIMD/vector techniques (for example SSE/AVX2 on x86-64 with a portable fallback) when they preserve numerical correctness and platform support.

Parallelize independent heavy work in background workers where beneficial, such as multi-channel preprocessing or bulk record analysis. Do not parallelize tiny cursor calculations if synchronization overhead would make them slower.

Do not replace targeted one-cycle harmonic calculations with a full FFT simply because FFT sounds faster. Choose FFT, Goertzel/targeted DFT, or direct recurrence based on measured workload, requested bins, sample count, and parity requirements.

No SIMD/parallel optimization may silently reduce precision or change protection-analysis results outside agreed tolerances.

## 6. Waveform LOD/downsampling is mandatory

Never send millions of raw points to the renderer when the viewport contains only a few thousand horizontal pixels.

For disturbance waveforms, Min/Max envelope downsampling is the default because it preserves transient extrema and trip spikes. Target output should scale with viewport width (typically O(pixel width), commonly around 1–2 extrema vertices per pixel bucket), not with the total sample count.

LTTB may be used only for data where preserving every extrema bucket is not an engineering requirement. Do not use simple averaging for protection/disturbance waveforms because it can hide short transients.

Downsampling must be viewport-aware: zoomed-in views should reveal progressively more native samples; zoomed-out views should use appropriate LOD.

## 7. GPU rendering and UI virtualization

Prefer Qt Quick Scene Graph / `QSGGeometryNode` for high-frequency or large engineering plots. Avoid large QML `Canvas` surfaces that repaint or re-upload textures during cursor motion or other frequent interactions.

Render only what is visible:
- compute geometry from the current time/sample window;
- do not instantiate or regenerate expensive content for off-screen tracks/panels when virtualization is possible;
- keep static grids/zones/trajectories separate from dynamic cursors/selection overlays;
- avoid full-scene invalidation for local changes.

Maintain hardware acceleration through Qt's supported graphics backend; do not force a software renderer as a performance workaround.

For high-volume trajectories, retain contiguous C++ numeric buffers across the analysis/render boundary. QML may control transforms, visibility and labels, but it must not become the storage/transport layer for thousands of trajectory samples.

## 8. Reuse allocations; no hot-path heap churn

Avoid repeated allocation/copying in paint, cursor, pan/zoom, and analysis hot paths.

Prefer:
- persistent scratch buffers with retained capacity;
- `reserve()` when a stable upper bound is known;
- reusing `QSGGeometryNode`/geometry/materials when sizes permit;
- spans/views/references over copying whole sample/time arrays;
- contiguous channel/sample structures suited to cache-efficient analysis;
- precomputed/indexed metadata rather than repeated scans.

Do not introduce a generic object pool unless profiling shows it improves the C++ workload. The goal is reduced allocation churn and copying, not a pattern for its own sake.

## 9. Resource lifetime and leak prevention

Use RAII for files, mappings, buffers, locks, and native resources. Qt objects must have clear ownership. Every long-running worker must have a cancellation/shutdown path.

On record close/reload/application shutdown:
- cancel and join/retire outstanding workers safely;
- release mapped views and file handles;
- clear caches tied to the old document/representation;
- release obsolete GPU geometry/resources through correct Qt ownership;
- never leave callbacks referencing a destroyed `DocumentController` or view.

Avoid manual `new/delete` unless ownership cannot be expressed with Qt ownership or standard smart pointers.

## 10. Performance budgets and regression gates

Treat performance as a testable contract:
- normal pan/zoom target: sustained 60 FPS on supported desktop hardware;
- cursor drag target: one-frame visual response, with expensive computation coalesced/backgrounded;
- hide/show signal: no blocking reparse;
- opening large records: progressive/non-blocking UX rather than a frozen window;
- heavy analysis: never block the GUI thread.

Add repeatable benchmarks for parsing throughput, memory use, waveform LOD, cursor latency, and bulk analysis as these paths evolve. A performance-sensitive PR should state the before/after behavior or measurement when practical.

`scripts/check_performance_contract.py` is a required CI gate. Do not weaken, bypass, delete, or special-case the gate merely to make a PR green. If a valid new architecture conflicts with a check, update the contract and gate together with an explicit engineering rationale and equivalent-or-stronger invariant.

## 11. Packaging/startup regression protection

A successful compile and unit test are not sufficient proof that the desktop application starts. Windows packaging CI must continue to launch the staged `ardirec.exe` as a smoke test after Qt/runtime deployment and fail if the process exits during startup unexpectedly.

Any change to QML types, plugin registration, deployment, or startup wiring must preserve this gate.

## 12. Exception-free parser/analysis hot paths and bounded asynchronous diagnostics

Exceptions must not be used as normal control flow inside repeated COMTRADE frame parsing, sample conversion, waveform preparation, harmonic/phasor/locus loops, cursor-driven calculations, or rendering hot paths.

Prefer one coherent explicit status/result model such as compact result structs/enums, `std::expected` when available in the configured toolchain, or `std::optional` only when detailed failure context is unnecessary. Normal recoverable conditions such as malformed row, invalid numeric field, truncated tail, unavailable channel, unsupported encoding/value, or failed sample conversion should produce deterministic status rather than stack unwinding.

For parsing large records, aggregate failures instead of producing one expensive diagnostic per row. A parser should be able to report summaries such as `validFrames`, `malformedRows`, `truncatedRows`, `invalidNumericFields`, and representative first/last locations while continuing to salvage valid data according to the existing parsing policy.

Qt, filesystem, mapping, OS, STL, or third-party exceptions may still occur at outer boundaries. Catch them at the nearest meaningful file/worker/application boundary, convert them to the same structured error/result model, and never permit exception unwinding into the Qt event loop or inner parse/render loops. Do not silently swallow exceptions.

Hot paths must not synchronously format large error strings, write log files, print repeated console output, emit one QML/UI event per error, serialize JSON, or perform telemetry. Enqueue only compact machine-readable diagnostic events into a bounded asynchronous queue/channel owned by a background diagnostic consumer.

The diagnostic queue must have a fixed/bounded capacity and explicit overload behavior. Duplicate storms must be coalesced/rate-limited and represented with counters. If the queue is full, increment dropped/coalesced counters and preserve the most useful high-severity/latest context according to policy; parsing, cursor motion, waveform rendering, and GUI responsiveness must never block on diagnostic persistence.

Human-readable message formatting and user-facing summaries belong outside hot paths. Diagnostic infrastructure is observational, not a correctness dependency: logging failure must never crash the workstation or prevent valid COMTRADE data from being analyzed.

Do not create multiple incompatible result/error abstractions across parser, analysis, and rendering subsystems. Reuse a lightweight machine-readable error taxonomy and translate it to UI text at a non-hot boundary.

## 13. Required implementation workflow

Before modifying a performance-critical path:
1. read `docs/PERFORMANCE_CONTRACT.md` and identify which invariant applies;
2. identify the actual hot path and its data lifetime;
3. preserve engineering/numerical behavior with tests;
4. choose the production architecture, not a disposable prototype;
5. keep GUI-thread work bounded and minimal;
6. avoid unnecessary copies and allocations;
7. add corruption/boundary tests when touching parsers;
8. run the performance-contract gate, core tests, desktop build, CodeQL, Windows packaged-startup smoke test, and relevant benchmarks before declaring the work complete.

When trade-offs arise, prefer deterministic, measurable engineering behavior over cosmetic similarity or speculative micro-optimization.
