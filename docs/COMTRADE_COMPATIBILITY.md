# COMTRADE Compatibility Strategy

## Canonical target

Implementation behavior is grounded in the applicable IEEE C37.111 / IEC 60255-24 specifications. Open-source projects are compatibility references and test-corpus sources, not the normative standard.

## Viewer 1.0 target matrix

- Revisions: 1991, 1999, IEC 60255-24:2001, 2013.
- Containers: CFG+DAT and 2013 CFF.
- DAT: ASCII, BINARY, BINARY32, FLOAT32.
- Channels: analog and packed digital/status.
- Timing: time multiplier, multiple sample-rate segments, timestamp quality/time-code fields where defined.
- Analog metadata: a/b scaling, skew, primary/secondary CT/VT values.
- Optional companion metadata: HDR/INF preserved; DMF/equipment mapping evaluated as extension input.
- Text: robust UTF-8 plus configurable legacy encodings.

## Compatibility diagnostics

Loading should produce three severities:

- **Info** — unusual but valid record.
- **Warning** — recoverable inconsistency; record can still be investigated.
- **Error** — safe interpretation is impossible.

Examples include DAT length mismatch, missing samples, unsupported extensions, invalid ratios and inconsistent channel counts.

## Torture suite

The repository will grow a synthetic redistributable suite covering revisions × data formats × sample-rate patterns × missing values × encodings × malformed boundaries. Vendor-provided records are included only with redistribution permission.
