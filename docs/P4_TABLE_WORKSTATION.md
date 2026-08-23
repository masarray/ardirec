# P4 Table Workstation

The Table view is a single-cursor engineering snapshot workspace. It is optimized for fast protection/disturbance reading rather than duplicating C1/C2 columns.

## Single cursor

Table uses shared Cursor 1 only. The cursor remains on the common investigation timebase and keeps digital-edge snapping. Time/Phasor/Locus retain dual C1/C2 workflows.

## Default columns

- Signal
- Phase
- Instantaneous
- RMS (true one-cycle RMS)
- H1 fundamental RMS
- H1 phase angle
- Cycle extremum (signed sample with greatest absolute magnitude in the trailing rated-frequency cycle)
- DC (signed arithmetic mean of the same cycle)
- THD
- H2/H1
- H3/H1
- H5/H1

Absolute quantities follow the global Primary/Secondary representation. Dimensionless harmonic percentages and THD do not change with representation.

## Workflow

Default scope is Electrical: recognized Voltage channels followed by recognized Current channels. Scope controls also provide Voltage, Current, Visible, and All Analog. Rows are compact and virtualized. Sorting supports Record, Signal, RMS, and THD.

A compact summary strip shows cursor time relative to trigger, selected representation, signal count, highest THD, and the row with the highest RMS. The table must preserve scope/sort/scroll when the cursor moves.

## Engineering policy

All windowed metrics are calculated in C++ from one immutable trailing-cycle snapshot. QML only formats and presents the snapshot. The table snapshot is cached by channel, cursor time, and representation so scrolling does not repeat DFT/RMS work.
