# Disturbance Stack UX

ardirec uses a familiar disturbance-record workflow without copying SIGRA's visual design.

## Core model

A fault record is read vertically as one synchronized event stack:

1. event/trigger strip
2. voltage analog tracks
3. current analog tracks
4. other analog/calculated tracks
5. binary/digital event tracks

All rows share the same horizontal timebase, trigger, Cursor 1 and Cursor 2.

## Default first-open behavior

- Recognize voltage channels from engineering unit/name and show them first.
- Recognize current channels and show them directly below voltage.
- Show binary channels below analog signals. The user can switch between `Active` and `All` binary channels.
- Use a vertically scrollable workspace; never compress an arbitrary number of signals into the viewport height.
- Preserve a pinned time ruler so horizontal interpretation remains obvious while scrolling.

## ardirec identity

The product should feel like a precision event timeline rather than a legacy multi-document Windows tool:

- compact neutral chrome
- fixed left signal rail
- section headers with counts
- amber active-state bars for binary signals
- blue Cursor 1, amber Cursor 2, green trigger
- high information density without oversized cards
- direct signal visibility controls rather than modal configuration for routine work

## Familiarity contract

A SIGRA/TransView user should immediately understand:

- time increases left to right
- every visible row is synchronized
- trigger is the common time reference
- Cursor 1/2 cross all tracks
- analog signals are waveform plots
- binary signals are state/timing bars
- scrolling moves through the signal stack, not through independent charts

## Next layers

After this stack is reliable, add RMS/phasor views, vector, R-X/locus, harmonics, formula/calculated channels and multi-record synchronization without changing this basic investigation model.
