# R10: smooth waveforms and Google Material Icons

## Waveforms (engineering-safe)
- Original one-pixel GPU line primitives were dependent on the graphics backend
  and appeared jagged, especially during diagonal transitions on Windows.
- Instantaneous and one-cycle RMS use the **same** screen-space mesh renderer,
  with a **1.85-logical-pixel core** and **0.70-logical-pixel feather** on
  each side (fade from full premultiplied alpha to zero).
- The Qt Quick Scene Graph uses `QSGGeometry::DrawTriangles` and
  `QSGVertexColorMaterial`; no runtime shader compilation, QPainter overlays,
  software image cache, fake smoothing/interpolation or per-frame allocation
  of a new item. The geometry node is reused while the point count is stable.
- The CPU snapshot remains authoritative: real COMTRADE samples, local extrema,
  RMS calculation, zero lines (Instant 50%, RMS 90%), C1/C2, measured values and
  waveform time coordinates are **unchanged**. Snapshot caps limit upload cost.
- Envelope mode connects only each original min/max pair; it never joins
  unrelated adjacent buckets. Unit regression checks this and per-vertex AA.

## Google Material Icons — one offline source of truth
- Migrated **all legacy Lucide SVG references** in Qt's toolbar, view selector,
  menu and MDI window chrome to Google's published Material Icons.
- 19 local 24px Google SVG assets are compiled as QML resources. No internet
  connection, icon font installation, web CDN, SVG runtime generation, or
  proprietary asset required.
- Qt Quick Controls' built-in `icon.source` and `icon.color` provide native
  tinting: engineering views use restrained phase accent colors; inactive icons
  remain grey, hover/selection use richer shades, window close uses red on hover.
  Application logo is retained as branding, not a Lucide icon.
- Google assets remain **Apache-2.0** licensed; notice and full license are in
  `licenses/`. SVGs are copied without hand-redrawing.
- `scripts/check_r10_visual_contract.py` ensures all referenced SVGs exist
  and are embedded, no Lucide paths survive, and both renderers remain shared.
  Every desktop Qt job also runs the CTest R10 antialias geometry regression.

## Acceptance before R9/R10 merge
1. Exact-head CI, Windows Build, Linux Installer Build and CodeQL pass.
2. Windows: compare the same COMTRADE file at 100%, 125%, and 150% DPI.
   Both Instant and RMS strokes must be thicker and smooth, **without changing
   zero positions, clipped maxima, or time alignment**.
3. Verify zoomed high-frequency fault transients retain all original local
   extrema and low-frequency continuous waves have no gaps at turns.
4. Verify every enabled/disabled/hover/selected icon is legible, and the
   installed Windows/LINUX ZIP loads all assets offline.
5. PR remains Draft until human screenshot and engineering acceptance.

Primary sources: https://github.com/google/material-design-icons,
https://developers.google.com/fonts/docs/material_symbols,
https://doc.qt.io/qt-6.8/qsgvertexcolormaterial.html.
