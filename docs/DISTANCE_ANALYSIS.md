# Distance / R-X analysis

P2 introduces manual distance-protection analysis. This document defines the calculation contract used by ardirec so a plotted impedance can be explained and independently reproduced.

## Frequency and calculation window

Distance quantities use the same fundamental phasor convention as the rest of the investigation workspace:

- nominal frequency comes from the COMTRADE CFG, with the existing 50 Hz fallback only when metadata is unusable;
- each impedance point uses the one-cycle window ending at that analysis time;
- phasors are RMS quantities;
- COMTRADE source samples remain immutable;
- Primary/Secondary representation is applied at the channel phasor boundary before the protection-loop equation;
- voltage/current unit prefixes are converted to SI before division, so the R-X result is in ohms.

A point is invalid when the required channels are unavailable, the calculation window cannot produce a valid phasor, or the measuring-current magnitude is too small for a meaningful impedance. ardirec does not replace an invalid point with an arbitrary large impedance.

### Locus sampling and open-breaker guard

The protection locus is evaluated on the actual COMTRADE sample timestamps inside the visible time range. If the visible range contains more points than the drawing budget, ardirec decimates the timestamp sequence while preserving the first and last visible samples. It does not invent an evenly spaced analysis time grid.

For protection loops, the minimum measuring-current magnitude is set to 0.1% of the largest displayed phase-current peak in the loaded record, with a 1 µA absolute lower bound. This floor scales naturally with the global Primary/Secondary representation and rejects the very large `V/I` artifacts that otherwise appear after a breaker has opened and current has collapsed. Rejected samples remain in the locus sequence as invalid timestamped gaps, so the renderer breaks the trajectory instead of drawing a line across the invalid interval.

## Phase-phase loops

For phases `p` and `q`:

```text
Zpq = (Vp - Vq) / (Ip - Iq)
```

P2 exposes:

- L1-L2
- L2-L3
- L3-L1

The equation is implemented in the pure C++ distance domain and is not reimplemented in QML.

## Phase-earth loops

P2 uses the residual-current compensation convention represented by OMICRON RIO grounding factor `kL`:

```text
Ires = IL1 + IL2 + IL3 = 3 I0
Zp-E = Vp / (Ip + kL * Ires)
```

P2 exposes L1-E, L2-E and L3-E.

`kL` is complex and is stored/displayed as magnitude and angle. A zero/missing value is allowed as an explicitly labelled **uncompensated earth loop**; the UI must not silently imply that compensation exists.

### RIO grounding-factor forms

Classic RIO can provide the equivalent grounding model in several forms. P2 supports:

```text
KL magnitude, angle
```

directly, or:

```text
kL = (Z0/Z1 - 1) / 3
```

for `Z0Z1`, or the RIO `RE/RL` + `XE/XL` representation converted using the imported line angle. The resulting `kL` is dimensionless, so Primary/Secondary switching does not change it.

Reference vocabulary and conversion semantics follow the public OMICRON RIO / Test Universe distance-object documentation. The implementation is validated with independent analytical literals in `ardirec_distance_tests`.

## RIO zone model

P2 imports the distance subset needed for manual overlay:

- DEVICE ratio metadata used for impedance-base conversion;
- DISTANCE line angle, `IMPPRIM` and grounding-factor data;
- ZONE index, label, type, fault loop, active state and trip time;
- mho characteristics;
- generic finite LINE/LINEP polygons;
- full-circle ARC/ARCP characteristics.

Generic LINE borders are interpreted as half-planes using the RIO LEFT/RIGHT inside convention and clipped into a finite polygon. Unsupported/inverted/open/mixed-arc geometry is reported as a compatibility diagnostic instead of being approximated silently.

Zone filtering follows the active measuring loop: `LN` applies to earth loops, `LL` to phase-phase loops, specific loop identifiers apply only to the matching loop, and `ALL` applies everywhere.

### Legacy SIGRA RIO

The SIGRA compatibility path also recognizes legacy `BEGIN PROTECTIONDEVICE` RIO files. `TRIPCHAR` polygons become internal `LL` characteristics and `TRIPCHAR-EARTH` polygons become `LN` characteristics. `LINEANGLE`, `RE/RL` and `XE/XL` are imported for earth-loop compensation. Mutual/parallel-circuit parameters can be detected and reported, but the parallel-line compensation algorithm remains outside the current parity scope.

## Primary / Secondary zone base

A measured loop impedance and a relay characteristic are comparable only on the same impedance base.

When RIO/XRIO provides secondary nominal voltage/current plus primary voltage/current, ardirec derives:

```text
Zsecondary / Zprimary = (Vsecondary / Vprimary) * (Iprimary / Isecondary)
```

and converts imported zone geometry to the global ardirec Primary/Secondary representation. If ratio metadata is incomplete, file-native zone values are retained at 1:1 and the UI raises a warning; the application must not claim that conversion was verified.

## SIGRA-style dual locus view

Protection mode renders two R-X diagrams simultaneously:

- **Earth loops**: L1-E, L2-E and L3-E with the earth-zone family;
- **Phase-phase loops**: L1-L2, L2-L3 and L3-L1 with the phase-zone family.

All available loops are visible at the same time. The loop selector is retained as an **inspection selector** for C1/C2 values and marker emphasis; it no longer hides the other protection trajectories.

Each panel computes its own symmetric R and X ranges. R and X are scaled independently, matching the wide R / compact X presentation commonly used by SIGRA instead of forcing a square one-ohm-per-pixel plot. Zone geometry and trajectory coordinates remain in engineering ohms; only their display transform differs between axes.

## XRIO boundary

XRIO is extensible and may contain vendor-specific `Custom` formulas/converter scripts. P2 intentionally does **not** evaluate those proprietary or device-specific expressions.

P2 recognizes the standardized RIO Distance hierarchy/IDs needed for:

- RIO / DISTANCE;
- PROTECTEDOBJECT / PROTECTIONDEVICE;
- grounding-factor mode and values;
- ZONES / ZONE;
- MHOSHAPE;
- GENERICSHAPE LINE/ARC elements;
- `IMPPRIM` and standard device ratio metadata.

That standardized section is adapted into the same internal RIO parser used for classic `.rio` files. If a file depends on unsupported Custom logic, ardirec reports the limitation rather than guessing a characteristic.

## Raw V/I diagnostic mode

The old per-phase `Va/Ia`, `Vb/Ib`, `Vc/Ic` locus remains available only under **Raw V/I**. It is explicitly labelled diagnostic and hides protection zones. It must not be interpreted as the compensated relay measuring loop.

## Validation contract

Regression tests include:

1. a synthetic phase-phase case constructed from a known target complex impedance;
2. a synthetic phase-earth case constructed from a known target complex impedance, residual current and non-zero complex `kL`;
3. invalid near-zero measuring current;
4. independently worked `Z0/Z1 -> kL` and `RE/RL-XE/XL -> kL` values;
5. synthetic classic RIO with LL circle and LN quadrilateral geometry;
6. fault-loop filtering and Primary/Secondary zone scaling;
7. synthetic standardized XRIO adaptation through the desktop import boundary;
8. legacy SIGRA RIO conversion into LL/LN characteristics;
9. sample-aligned six-loop availability, timestamp preservation, decimation and post-open current-floor rejection through the desktop analysis boundary.

Cross-tool comparison with legally obtained records and relay settings remains required before claiming protection-algorithm parity with a commercial analysis package.
