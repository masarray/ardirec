# Project Status

**Current gate:** G0 — Foundation

## Verified locally

- CMake configure with desktop disabled.
- C++20 core compilation with GCC 14.
- Core regression test passes.
- ASCII DAT decode/scaling fixture passes.
- BINARY, BINARY32 and FLOAT32 reference decode/scaling fixtures pass.
- Packed digital status decode passes.
- CLI can inspect the bundled synthetic COMTRADE fixture.

## Not yet locally verified in this bootstrap environment

Qt is not installed in the bootstrap container, so the Qt Quick target has not been compiled locally. GitHub Actions is configured to install Qt 6.8.4 and compile the desktop target after publication.

## Next engineering milestone

G1 connects real COMTRADE samples to the Scene Graph renderer through a large-file SignalStore + min/max LOD path and adds shared pan/zoom plus dual cursors.
