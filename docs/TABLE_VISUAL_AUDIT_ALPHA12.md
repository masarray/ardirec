# Table visual audit — alpha.12

## What alpha.11 already fixed

- single Table Cursor with digital-edge snapping;
- all recognized Voltage + Current channels by default;
- 30 px virtualized rows;
- one-cycle C++ engineering snapshots and caching;
- Primary/Secondary-safe absolute values;
- RMS/THD sorting and summary strip.

## Remaining usability gaps

1. Too many columns are always visible, so narrower displays require horizontal navigation before the engineer can read the core fault quantities.
2. A flat table makes abnormal channels visually compete with normal channels.
3. DC offset is shown only as engineering magnitude, which is difficult to compare across channels with different H1 magnitude.
4. Crest behavior is not surfaced even though it is useful for spotting heavily distorted/asymmetric current.
5. Hover exists, but clicking a row does not yet carry the selected signal into the rest of the investigation workflow.

## Alpha.12 response

- `Analysis` is the default compact column set; `Detailed` adds secondary diagnostic columns.
- `Only abnormal` is an investigation filter using transparent heuristic thresholds.
- add DC %H1 and crest factor;
- add sort by DC bias and crest;
- selected row receives a subtle highlight and synchronizes the shared measuring signal;
- keep phase colors as thin identity accents, not full-row decoration.
