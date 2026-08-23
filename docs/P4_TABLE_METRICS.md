# Table metric semantics

All Table metrics are evaluated at the single Table Cursor.

- **Instant**: nearest recorded sample at the cursor timestamp.
- **True RMS**: RMS over the trailing nominal-frequency cycle ending at the cursor.
- **H1 RMS**: full-cycle DFT fundamental over that same trailing cycle.
- **Phase angle**: sine-wave phase position at the cursor/reference instant. The DFT coefficient is cosine-referenced internally and is converted to the protection-tool phase convention before presentation.
- **Last extremum**: most recent completed local maximum or minimum before the cursor. It is not the largest absolute sample in the whole cycle.
- **DC absolute**: signed arithmetic mean of the same finite cycle samples, available in Detailed columns.
- **DC %**: absolute DC divided by H1 RMS, percent. This is the default analysis-table presentation because it matches common disturbance-analysis workflows.
- **THD**: `sqrt(sum(H2..Hn^2)) / H1`, where `n` is never higher than the harmonic order physically resolvable below/at Nyquist. H0/DC and H1 are excluded.
- **H2/H1, H3/H1, H5/H1**: harmonic RMS magnitude divided by H1 RMS, percent.

## Nyquist rule

A record sampled at 1 kHz with nominal frequency 50 Hz can resolve harmonics only through H10. Requests for H15/H25/H50 are therefore clipped to H10 before the DFT is accumulated and before THD is calculated. This prevents aliased H11+ bins from being counted again as distortion.

## Representation

Absolute quantities use the active Primary/Secondary representation. Percentage quantities are invariant. A negative engineering scale rotates the displayed phasor angle by 180 degrees while preserving magnitude.
