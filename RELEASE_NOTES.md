# ardirec v0.2.0-alpha.2 — Analysis Workspace Alpha

This alpha replaces the original single-waveform viewer with a disturbance-analysis workspace designed around the workflow of established tools such as SIGRA, while keeping ardirec's own implementation and visual identity.

## What changed

- Multiple analog channels are displayed as synchronized stacked tracks instead of one fullscreen waveform.
- A shared trigger-relative time ruler spans every track.
- The COMTRADE trigger is drawn as a common vertical reference through all traces.
- Dual cursors now represent absolute record time, so zooming and panning no longer change the measured timestamps.
- A compact cursor table shows Cursor 1, Cursor 2, C2-C1, instantaneous values and time-derived frequency.
- The measuring signal can be changed independently from the visible track set.
- The signal browser is now an on-demand drawer rather than a permanent sidebar that steals plot width.
- Up to eight analog tracks can be added or removed from the workspace.
- Fit Record and Trigger Focus navigation are available from the main toolbar.
- Mouse-wheel zoom is centered around the pointer position and all tracks pan together.
- The Scene Graph renderer no longer switches to a min/max envelope for ordinary 3–4 samples-per-pixel AC waveforms; envelope rendering is reserved for genuinely dense views.
- Vertical scaling is centered around zero for electrical waveforms.
- Record station, recorder, COMTRADE revision/format, nominal frequency, start time and trigger time remain visible during analysis.
- Windows packaging remains a one-file portable EXE.

## Alpha limitations

- Desktop loading remains capped at 500,000 sample frames until the memory-mapped SignalStore and persistent LOD cache land.
- Digital channels are decoded by the core but dedicated digital transition tracks are not yet shown in the workspace.
- Per-track manual Y scaling, cursor sample snapping, RMS/phasor/vector/R-X/harmonics and workspace persistence remain future gates.
- The Windows binary is unsigned and may trigger SmartScreen.

## Packaging note

The one-file portable EXE is a self-extracting Qt bundle. It extracts its private runtime to a temporary location and launches `ardirec.exe`; it does not install ardirec into Windows.
