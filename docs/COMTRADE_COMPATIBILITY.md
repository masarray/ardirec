# COMTRADE compatibility

ArDiRec is designed for vendor-neutral disturbance recordings. This page separates **current application capabilities** from **compatibility targets** so that users know what is tested and what still needs field evidence.

## Current v0.2.0 capabilities

- Open a COMTRADE `.cfg` record and locate its companion `.dat`.
- Decode ASCII, BINARY, BINARY32 and FLOAT32 DAT formats through the core parser.
- Read analog and packed digital/status signals for the engineering workstation.
- Use metadata for signal channels, timestamps and primary/secondary representation when those fields are valid.
- Present diagnostics for unsupported or malformed input rather than silently inventing values.

Specific vendor revisions, extensions and sidecar conventions vary. Report an example if a record fails.

## Compatibility objectives, not certification

The longer-term validation matrix includes COMTRADE 1991, 1999, IEC 60255-24:2001 and 2013; varying sample rates, damaged/truncated files, timestamp quality, character encodings and proprietary metadata. Support for 2013 CFF containers and every legacy text encoding is **not guaranteed in v0.2.0**.

Optional RIO/XRIO data may be interpreted for supported zone shapes and compensation conventions. Unsupported proprietary scripts/converters are not executed or guessed.

## Engineering validation

Automated tests cover deterministic parser and numerical cases, plus large-record and desktop performance. This is not an exhaustive survey of vendor devices or every operating environment. In field work, verify channel association, timebase, frequency, units, CT/VT scaling and the validity of calculated quantities.

## Reporting an incompatible record

Open a [GitHub issue](https://github.com/masarray/ardirec/issues) and provide: application version, operating system, COMTRADE revision, DAT type, sample rates, analog/digital channel counts, reproduction steps, diagnostic text, and a minimally shareable sample where authorized.

Do not upload confidential utility or substation recordings without explicit permission.
