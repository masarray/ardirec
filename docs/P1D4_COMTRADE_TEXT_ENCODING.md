# P1D.4 — COMTRADE text encoding boundary

The ArdIrec native bridge exposes textual COMTRADE metadata as UTF-8. Older protection records, especially COMTRADE 1991/1999 exports, may contain Western-European Windows-1252 bytes instead of valid UTF-8.

P1D.4 normalizes textual CFG metadata at the parser boundary:

- valid UTF-8 is preserved byte-for-byte;
- invalid UTF-8 is interpreted with a deterministic Windows-1252 fallback;
- station/recorder names, analog channel id/phase/circuit/units/representation and status channel id/phase/circuit are normalized;
- numeric COMTRADE fields are not altered;
- the bridge continues to copy already-normalized UTF-8 and its ABI does not change.

The regression coverage writes both an actual binary CFG containing CP1252 `0xE9` in `déclenchement` and an already-valid UTF-8 variant. It verifies both parse to the same UTF-8 `d\xC3\xA9clenchement` channel id.

This fixes replacement-character labels in ARSAS without introducing UI-specific language substitutions or lossy ASCII cleanup. The implementation is layered onto the current field-robust parser, so existing structural recovery and diagnostics remain unchanged.
