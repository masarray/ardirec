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

A point is invalid when the required channels are unavailable, the calculation window cannot produce a valid phasor, or the measuring-current magnitude is effectively zero. ardirec does not replace an invalid point with an arbitrary large impedance.

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

## Primary / Secondary zone base

A measured loop impedance and a relay characteristic are comparable only on the same impedance base.

When RIO/XRIO provides secondary nominal voltage/current plus primary voltage/current, ardirec derives:

```text
Zsecondary / Zprimary = (Vsecondary / Vprimary) * (Iprimary / Isecondary)
```

and converts imported zone geometry to the global ardirec Primary/Secondary representation. If ratio metadata is incomplete, file-native zone values are retained at 1:1 and the UI raises a warning; the application must not claim that conversion was verified.

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

P2 regression tests include:

1. a synthetic phase-phase case constructed from a known target complex impedance;
2. a synthetic phase-earth case constructed from a known target complex impedance, residual current and non-zero complex `kL`;
3. invalid near-zero measuring current;
4. independently worked `Z0/Z1 -> kL` and `RE/RL-XE/XL -> kL` values;
5. synthetic classic RIO with LL circle and LN quadrilateral geometry;
6. fault-loop filtering and Primary/Secondary zone scaling;
7. synthetic standardized XRIO adaptation through the desktop import boundary.

Cross-tool comparison with legally obtained records and relay settings remains required before claiming protection-algorithm parity with a commercial analysis package.
