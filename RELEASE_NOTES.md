# ardirec 0.1.0-foundation

This is the project bootstrap release, not Viewer 1.0.

## What is ready

- Product direction and scope are locked through Viewer 1.0.
- C++20 COMTRADE configuration parser foundation.
- Reference DAT decoding for ASCII, BINARY, BINARY32 and FLOAT32.
- Packed digital/status decoding.
- Related-file auto-location with case-insensitive extension handling.
- CLI inspection path for parser regression work.
- Qt Quick desktop shell and custom Qt Scene Graph waveform renderer skeleton.
- Cross-platform core CI, Qt desktop CI and CodeQL workflows.
- GPL-3.0-or-later licensing and contributor/security policies.

## What comes next

G1 replaces the reference full-frame decoder in the interactive viewer path with large-file SignalStore access, a min/max LOD pyramid, real analog/digital rendering, synchronized pan/zoom and dual cursors.
