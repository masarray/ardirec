# P0 Milestone — Large Record Architecture & Field Robustness

This milestone establishes the production data-loading foundation for ArdIREC before further numerical-analysis and UX expansion.

## Milestone contract

P0 is accepted only when all of the following remain true on the integration head:

- COMTRADE BINARY/BINARY32/FLOAT32 DAT uses read-only memory mapping when available, with bounded streaming fallback.
- ASCII DAT is indexed by compact row offsets and parsed lazily; the desktop does not materialize a `SampleFrame` array or eager per-channel whole-record arrays.
- Record loading/indexing runs off the UI thread and can be cancelled/replaced safely.
- The historical 500,000-sample viewer cap is removed.
- Time-indexed analysis retains a safe monotonic prefix when field data becomes damaged.
- Recoverable CFG corruption produces deterministic safe fallbacks plus diagnostics; structural corruption fails cleanly.
- Invalid analog payload is represented as unavailable (`NaN`) rather than fabricated engineering data.
- Invalid digital payload is tri-state unknown and cannot manufacture a digital transition.
- Truncated binary DAT tails retain complete frames and diagnose ignored bytes.
- Oversized/untrusted metadata is bounded (CFG safety limit and bounded HDR preview).
- Waveform zoom-out uses a bounded Min/Max LOD cache that preserves extrema; full raw-record scans are not required for every zoomed-out paint.
- Engineering calculations continue to read original samples; visual LOD floats are never substituted into protection calculations.
- The visual LOD cache is bounded to 2,000,000 channel/block cells and is published as an immutable snapshot before the DAT source reaches the UI/render thread.
- Waveform bucket scratch storage is reused across paints to avoid hot-path allocation churn.
- CI exercises a 10,000,000-sample record and a 120-analog-channel record near the 250 MiB design class.
- Core tests pass on Linux, Windows and macOS; desktop tests pass on Linux; CodeQL passes; the packaged Windows application stays alive in the startup smoke test.

## Architecture

`IndexedDatFile` is the immutable backing store. The operating system owns paging for memory-mapped binary files, while ASCII records retain only row offsets. `DocumentController` exposes shared snapshots rather than copying complete channels. Analysis code requests bounded windows. Qt Quick waveform items consume the same immutable source and the precomputed visual LOD.

The Min/Max LOD is intentionally an extrema-preserving representation, not averaging or cosmetic smoothing. Partial blocks at viewport boundaries are read from raw samples so arbitrary pan/zoom boundaries remain faithful. When zoomed in beyond the useful LOD resolution, the renderer falls back to raw samples.

## Field-data behavior

ArdIREC distinguishes recoverable field damage from structural failure. A damaged optional metadata field must not terminate a record that can still be interpreted safely. Missing or invalid measurement values remain unavailable instead of being silently converted into plausible values. Digital unknown states are excluded from edge generation to avoid false protection-event timing.

Operator-facing diagnostics report salvaged/truncated/corrupt conditions without treating normal mmap operation as a warning.

## Performance acceptance

The CI benchmark is intentionally deterministic and repeatable rather than tied to a single hosted-runner timing threshold. It verifies:

1. 10,000,000 samples / 6 analog / 8 digital (~210 MiB binary DAT class).
2. 1,000,000 samples / 120 analog / 8 digital (~238 MiB binary DAT class).
3. mmap access when supported by the runner.
4. complete compact indexing, bounded LOD creation, random lazy access and full LOD traversal.
5. LOD storage never exceeds the 2,000,000-cell contract.

Wall-clock values are printed for regression tracking (`index_s`, `index_mframes_s`, `random_4k_s`, `lod_scan_s`, `compact_index_mib`, `lod_mib`) but are not made brittle pass/fail thresholds on shared CI hardware.

## Deliberately outside this P0 milestone

The following are follow-up optimizations, not reasons to delay this foundational milestone once all gates above are green:

- SIMD/AVX/NEON specialization after profiler evidence identifies a compute hotspot.
- Broader randomized/fuzz COMTRADE corpus testing beyond the deterministic corruption regression suite.
- Multi-level LOD pyramids if profiling shows the bounded single-level cache is insufficient for extreme viewport/channel combinations.
- Further parallelization of heavy analytical transforms after numerical parity tests are established.
- SIGRA numerical distance/locus parity work; visual/performance code must not cosmetically alter engineering trajectories.

## Release discipline

Do not merge this milestone while any acceptance gate is red. The Windows artifact used for field validation must be produced by the same accepted integration head (or its merge commit) and must pass the packaged executable startup smoke test.
