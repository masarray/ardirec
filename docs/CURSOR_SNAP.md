# Cursor digital-edge snapping

ardirec cursors are engineering measurement tools, not free-floating decorative markers.

## Interaction

- Hover within 8 px of Cursor 1 or Cursor 2: pointer changes to a horizontal resize cursor (`↔`).
- Drag a cursor horizontally: it follows the pointer on the shared timebase.
- If a rising or falling edge from any loaded COMTRADE digital/status channel comes within 12 px of the pointer, the cursor snaps to that exact sample time.
- Outside the snap radius, cursor motion remains free.
- Right-click placement of Cursor 2 uses the same digital-edge snapping rule.

The snap tolerance is defined in pixels and converted to seconds using the current visible time window. This keeps the interaction visually consistent at every zoom level.

## Engineering use cases

This makes timing measurements such as Pickup → Trip, Pickup → Dropoff, Trip → CB Open, Start → Operate, and protection/breaker sequence checks fast and repeatable.

Both rising and falling transitions are indexed. Multiple digital signals transitioning at the same sample share one edge timestamp.
