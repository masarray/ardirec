# Why equal cursor timestamps could still disagree

A shared cursor timestamp is necessary but not sufficient for numerical parity. Two disturbance-analysis tools must also agree on:

1. measurement window placement;
2. phasor reference convention;
3. harmonic bandwidth/Nyquist limit;
4. definition of extrema;
5. display representation of DC and other derived values.

The line1.CFG comparison showed that ardirec and the reference tool already agreed on H1 magnitude and H2/H3/H5 ratios. The remaining disagreements were therefore semantic/reference issues, not a COMTRADE parser scaling failure.
