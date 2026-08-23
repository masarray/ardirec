# ardirec synchronized multi-view strategy

ardirec uses one shared **Investigation Context** for every analysis surface. A user may switch views without losing the event window, Cursor 1, Cursor 2, measuring signal, visible channel set or trigger reference.

## Product rule

The COMTRADE trigger is a fixed record reference. It is never presented as a draggable measurement cursor.

- Trigger: muted dashed reference, relative time `0 ms`.
- Cursor 1 / Cursor 2: explicit movable engineering measurement cursors.
- Cursors snap to nearby rising/falling digital transitions.
- Moving a cursor updates every cursor-dependent analysis view.

## Phase visual standard

The same phase identity is used for voltage, current and derived quantities:

| Phase | Color |
| --- | --- |
| L1 / A | Red `#d32f2f` |
| L2 / B | Yellow `#d6a700` |
| L3 / C | Blue `#1976d2` |
| E / N / residual | Green `#2e8b57` |

Unknown/unclassified analog channels use a neutral gray rather than inventing a phase assignment.

## Viewer 1.0 analysis surfaces

1. **Time Signals** — recorded instantaneous/sinusoidal samples, one-cycle sliding RMS toggle, analog and digital tracks on one common timebase.
2. **Phasor** — fundamental full-cycle DFT, voltage and current vector diagrams, cursor synchronized.
3. **Locus** — R-X trajectory. The first implementation is explicitly raw phase `V/I`; compensated protection loops and RIO/XRIO zones are a later protection-analysis gate.
4. **Harmonics** — full-cycle DFT harmonic RMS magnitudes and percent of the fundamental.
5. **Value Table** — instantaneous, RMS and phasor-angle values at C1/C2.

Planned surfaces after this slice include symmetrical components, frequency/ROCOF, P/Q/S/PF, calculated signals, protection loops, RIO/XRIO overlays and multi-record synchronization.

## Shared state

```text
COMTRADE Document
      │
      ▼
Investigation Context
  ├─ common time window
  ├─ Cursor 1
  ├─ Cursor 2
  ├─ measuring signal
  ├─ visible channels
  └─ trigger reference
      │
      ├─ Time Signals
      ├─ Phasor
      ├─ Locus
      ├─ Harmonics
      └─ Table
```

Switching a view changes only the visualization. It does not create a second copy of the event state.

## Calculation policy

Electrical calculations live in C++, separate from QML presentation. QML is responsible for interaction and visualization only.

Current deterministic analysis primitives:

- one-cycle true RMS;
- fundamental full-cycle DFT phasor;
- harmonic full-cycle DFT;
- raw complex impedance `Z = V/I`.

The next calculation gate must add synthetic validation fixtures before extending into symmetrical components, distance-protection loop compensation and fault analysis.
