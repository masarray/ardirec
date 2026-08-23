# P4.1 Table investigation polish

Goal: make Table faster to investigate than a legacy flat value table by separating compact analysis columns from optional detail, surfacing abnormal channels, and synchronizing row selection with the shared measuring signal.

## Decisions

- Table remains single-cursor.
- Default `Analysis` columns fit common 1080p workspaces without horizontal scrolling where practical: Signal, Phase, H1 RMS, H1 angle, cycle extremum, DC %H1, THD, H2/H1, H3/H1, H5/H1.
- `Detailed` adds Instantaneous, true one-cycle RMS, crest factor, and signed DC engineering magnitude.
- `Only abnormal` filters to channels with THD >= 5%, |DC|/H1 >= 5%, or crest factor >= 2.0.
- Sort modes include Record, Signal, RMS, THD, DC bias, and Crest.
- Clicking a row selects that channel as the shared measuring signal so switching to Time/Harmonics/Phasor keeps investigation context.
- Summary strip highlights highest THD, highest DC bias, highest crest factor, plus separate Max V RMS and Max I RMS.

Thresholds are investigation affordances only, not protection or standards compliance verdicts.
