# Cursor Snap Acceptance Criteria

1. Pointer changes to horizontal-resize when within 8 px of Cursor 1 or Cursor 2.
2. Dragging either cursor keeps the cursor on the common timebase.
3. Within 12 px of any COMTRADE digital transition, the cursor snaps to the exact transition sample timestamp.
4. Rising and falling digital transitions are both eligible snap points.
5. Outside the snap radius, cursor movement remains continuous/free.
6. Right-click placement/drag of Cursor 2 uses the same snapping behavior.
7. Snap tolerance scales with the visible time window so behavior remains consistent while zooming.
8. Existing pan, vertical signal scrolling and Ctrl+wheel time zoom remain unchanged.
