# Table analysis contract

The Table workspace is a point-in-time engineering snapshot, not a generic spreadsheet.

At the selected Table Cursor, each analog channel may expose:

- instantaneous sample;
- true one-cycle RMS;
- H1 fundamental RMS and phase angle;
- signed cycle extremum;
- crest factor = |cycle extremum| / RMS;
- signed DC and |DC| / H1;
- THD through H25;
- H2/H1, H3/H1 and H5/H1.

`Only abnormal` is an investigation filter only. Current heuristic thresholds are THD >= 5%, |DC|/H1 >= 5%, or crest factor >= 2.0. These thresholds are not standards compliance or relay-operate verdicts.
