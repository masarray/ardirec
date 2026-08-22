# Performance Contract

Performance is a product requirement, not final polish.

## Viewer targets

| Scenario | Initial contract |
|---|---:|
| Normal pan/zoom | sustained 60 FPS on supported desktop hardware |
| Cursor drag | one-frame visual response target |
| Hide/show signal | no blocking reparse |
| 10M-sample record | interactive through LOD/lazy access |
| 100+ channels | supported without eager duplicate buffers |
| 250 MB record | normal design case, not exceptional |
| Heavy analysis | never blocks UI thread |

## LOD rule

Zoomed-out waveform representation preserves extrema using min/max buckets. Simple averaging is not accepted because it can hide transient spikes.

## Benchmark policy

Benchmarks live under `benchmarks/` as features arrive. Performance regressions should be measurable in CI or repeatable locally before Viewer 1.0.
