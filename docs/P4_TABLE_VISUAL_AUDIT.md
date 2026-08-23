# P4 Table visual audit

The alpha.10 table duplicates C1/C2 columns and leaves most of the workspace visually empty. For a standard 3V+3I record, only six rows are shown but the header consumes large vertical space and the engineer must mentally compare two sets of Instant/RMS/Angle columns.

The P4 redesign prioritizes scan speed:

- one cursor, one engineering state;
- dense 30 px rows;
- all electrical channels by default;
- direct engineering columns rather than duplicated cursor families;
- compact scope/sort bar;
- summary strip for highest THD and strongest RMS;
- subtle phase identity strip rather than full-row phase coloring;
- horizontal scrolling only when the viewport is too narrow;
- virtualized rows and cached snapshots.

The design intentionally uses SIGRA's engineering density as a capability reference but keeps ardirec's own modern neutral visual identity.
