# ArdIREC Performance Contract

ArdIREC is an interactive protection/disturbance workstation. Performance is a correctness property: an analysis result that freezes input, flickers, publishes stale data, or scales rendering cost with record length is not production-ready.

This contract supplements `AGENTS.md` and is enforced by CI where a deterministic static or benchmark gate is possible.

## Non-negotiable architecture invariants

### 1. Render code consumes prepared data only

`QQuickItem::updatePaintNode()` and QML paint handlers must not perform RMS, DFT/FFT, full-record traversal, DAT decoding loops, or other engineering analysis.

Allowed work in a render callback is bounded scene-graph preparation: map an immutable prepared snapshot to viewport coordinates, update a bounded number of vertices/material properties, and return.

### 2. Interaction and analysis are separate pipelines

Pointer motion may update a cursor/thumb preview at raw input rate. Heavy engineering state is committed at most around display cadence while dragging, and the exact final value is committed on release.

Heavy jobs are latest-wins. A newer cursor, pan, zoom, representation, document, or protection-setting request cancels/supersedes older work. Stale results must never publish.

### 3. Bulk analysis is never synchronous on the GUI thread

Full-record RMS, phasor sweeps, locus generation, large transforms, and export work run in cancellable background workers. QML must not invoke a synchronous full-record numerical loop from a binding or event handler.

### 4. Shared numerical work is computed once

Consumers at one cursor position reuse one immutable engineering snapshot. Fundamental V/I phasors sharing a window use one timestamp/window traversal and one trigonometric basis per sample; Sequence, Phasor and distance cursor math reuse those values rather than repeating DFTs.

### 5. Plot complexity follows pixels, not samples

A 10M-sample record and a 10k-sample record displayed in the same 1600-pixel viewport must have the same order of rendering complexity.

Waveforms use extrema-preserving LOD. Bulk locus geometry is capped by a viewport-derived point budget. No production renderer receives one QML/JavaScript object per source sample.

### 6. Bulk plots use native contiguous buffers

High-volume waveform/locus geometry remains in contiguous C++ storage across analysis and render boundaries. Do not marshal thousands of `QVariantMap`, JavaScript objects, or QObject instances per trajectory.

Small scalar UI snapshots may use `QVariantMap`; bulk plot data may not.

### 7. High-frequency engineering graphics use retained rendering

Dynamic waveform, phasor and locus trajectories use Qt Quick Scene Graph / native geometry. A QML `Canvas` may remain for low-frequency static decoration such as grids, labels or protection-zone backgrounds, but cursor motion must not cause a large Canvas trajectory repaint or texture re-upload.

### 8. Numeric hot paths contain no metadata discovery

Channel role/phase/unit classification belongs at document-load or snapshot-source construction. Repeated DFT/RMS/locus loops operate on integer indices/scales and contiguous numeric storage, not regex, QString normalization or channel scans.

## Release targets

These are product targets, not promises about GitHub-hosted runner wall-clock timing:

- cursor preview: visible within one display frame;
- normal interactive target: sustained 60 FPS on supported desktop hardware;
- GUI-thread heavy-analysis time: zero by architecture; bounded handoff/geometry work only;
- plot vertex count: O(viewport pixels), not O(record samples);
- large-record design cases: 10M samples, 100+ channels, ~250 MB;
- background work: cancellable/latest-wins with no stale publication;
- Windows package: staged application startup smoke test must pass.

## CI enforcement

`scripts/check_performance_contract.py` rejects known architectural regressions, including:

- RMS/sample traversal reintroduced inside `updatePaintNode()`;
- Phasor high-frequency Canvas rendering reintroduced;
- synchronous full-record locus calls from QML;
- production locus point materialization into per-point QVariant objects;
- direct raw pointer-rate cursor commits bypassing coalescing;
- removal of cancellable background snapshot engines or native retained trajectory renderers.

The existing `large-record-contract` benchmark remains mandatory. Measured analysis/frame-time benchmarks should be added where runner variance can be controlled or compared against stable regression baselines.

## Review rule

A performance-sensitive PR is incomplete until it answers four questions:

1. What work occurs on the GUI/render thread before and after the change?
2. What is the asymptotic cost with respect to samples, channels and viewport pixels?
3. How are cancellation and stale-result publication prevented?
4. Which CI/test/benchmark proves the architecture and numerical behavior remain valid?
