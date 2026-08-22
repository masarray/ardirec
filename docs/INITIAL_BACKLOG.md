# Initial Engineering Backlog

These are the recommended first GitHub issues after publication.

## G1 — Real Viewer

1. **SignalStore: memory-mapped BINARY DAT access**  
   Design channel-oriented random access without eager full-record duplication.

2. **ASCII indexed cache**  
   Parse large ASCII DAT in a worker thread and build a compact binary/index cache.

3. **Waveform LOD pyramid**  
   Build min/max levels with deterministic bucket rules and regression tests.

4. **Connect real analog samples to QSG renderer**  
   Immutable render snapshot, visible-range selection and GPU geometry batching.

5. **Digital track renderer**  
   Render transitions from packed status channels without per-sample rectangles.

6. **Shared time-axis model**  
   One transform for all plots; precision-safe time origin and tick generation.

7. **Pan/zoom interaction contract**  
   Wheel/trackpad zoom around pointer, horizontal drag, zoom reset and keyboard shortcuts.

8. **Dual cursor model**  
   Cursor A/B, Δt, cycles, sample snap and digital-transition snap foundation.

9. **Channel semantic classifier v1**  
   Rules for IA/IB/IC, IL1/IL2/IL3, VA/VB/VC, UL1/UL2/UL3 with reversible mapping.

10. **Record Health panel v1**  
    Surface parser warnings, DAT presence/length checks and sample-rate metadata.

## G2 preparation

11. Synthetic COMTRADE generator for revision/data-format matrix.
12. Case/encoding torture fixtures.
13. Multiple sample-rate timing tests.
14. Missing-data propagation contract.
15. CFF architecture spike.
