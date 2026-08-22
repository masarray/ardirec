# Locked Product Decisions

These decisions are intentionally stable. Changing one requires an explicit architecture decision and rationale.

## D001 — Product scope order
Viewer 1.0 reaches manual disturbance-analysis parity before multi-record fault analysis or AI/protection intelligence.

## D002 — Desktop stack
C++20 owns engineering logic. Qt Quick/QML owns presentation and interaction composition.

## D003 — Waveform renderer
The production waveform path uses a custom `QQuickItem` / Qt Scene Graph backend with LOD. Qt Charts, QML Canvas, QQuickPaintedItem and QCustomPlot are not the primary waveform architecture.

## D004 — Data integrity
Source COMTRADE data is immutable. Semantic mapping, corrections and derived calculations are separate and visible.

## D005 — Normative behavior
IEEE/IEC COMTRADE specifications are normative. Open-source parsers are references/test sources, never the source of truth.

## D006 — Licensing
ardirec is GPL-3.0-or-later. New dependencies and reused code must be license-compatible and documented.

## D007 — UX
Default language is **Precision Industrial**: compact, dense, restrained, smooth and waveform-first; no oversized dashboard cards or gaming-style futurism.
