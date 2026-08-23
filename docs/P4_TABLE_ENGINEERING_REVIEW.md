# P4 engineering review notes

The redesign removes duplicated cursor families from Table and consolidates all windowed metrics behind `TableSnapshotController`.

Review points:
- raw COMTRADE arrays stay immutable;
- QML does no electrical calculation;
- cache invalidates on document or Primary/Secondary representation changes;
- cursor movement changes only the snapshot key;
- THD uses H2..H25 so Table remains compact while Harmonics remains the detailed H50 tool;
- signed extremum and signed DC preserve polarity information;
- one-cycle window semantics match the existing analysis convention.
