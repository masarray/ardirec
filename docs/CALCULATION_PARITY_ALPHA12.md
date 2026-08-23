# Calculation parity — alpha.12

The Siemens SIGRA `line1.CFG` sample exposed several semantic differences that were not COMTRADE decoding errors.

## What already matched

At the same 0 ms trigger reference in Secondary representation, ardirec alpha.11 matched SIGRA for:

- H1 RMS magnitude on IL1/IL2/IL3 and UL1E/UL2E/UL3E;
- H2/H1, H3/H1 and H5/H1 ratios.

That evidence isolates the remaining discrepancies to calculation/presentation semantics rather than CFG/DAT scaling.

## Corrected in alpha.12

1. **Phase reference** — harmonic and table phase is now the sine-wave phase position at the cursor/reference instant. The previous implementation referenced DFT phase to record start and exposed the cosine coefficient directly.
2. **Nyquist harmonic limit** — requested harmonics are clipped to `floor((sample_rate / 2) / nominal_frequency)`. A 1 kHz, 50 Hz record therefore stops at H10; H11+ are never accumulated or included in THD.
3. **DC presentation** — the compact Table presents DC as `% of H1`. Signed engineering DC remains available in Detailed mode.
4. **Last extremum** — Table `Last extremum` means the most recent completed local maximum/minimum before the cursor, not the maximum absolute sample anywhere in the trailing cycle.
5. **Transparency** — Table shows estimated sample rate and the harmonic order actually used for THD.

## line1.CFG benchmark at trigger 0 ms

Reference values observed in SIGRA Secondary Table:

| Signal | H1 RMS | Phase | Last extremum | DC % | H2/H1 | H3/H1 | H5/H1 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| IL1 | 3.3656 A | -58.4° | -5.4108 A | 16.4% | 21.9% | 16.7% | 6.9% |
| IL2 | 0.9919 A | -120.5° | 1.3908 A | 0.3% | 0.3% | 0.2% | 0.1% |
| IL3 | 1.0094 A | 119.7° | 1.4214 A | 0.1% | 0.3% | 0.2% | 0.1% |
| UL1E | 24.858 V | 16.0° | -21.991 V | 31.4% | 36.1% | 27.3% | 10.6% |
| UL2E | 65.160 V | -134.4° | 98.202 V | 5.9% | 6.8% | 5.1% | 2.0% |
| UL3E | 70.156 V | 130.4° | 98.742 V | 5.5% | 6.3% | 4.8% | 1.9% |

These values are a black-box interoperability benchmark only; the Siemens sample file itself is not redistributed by ardirec.
