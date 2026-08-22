# Validation Strategy

ardirec is engineering software. A plot that “looks right” is not validation.

## Layers

1. **Parser conformance** — synthetic fixtures with exact expected metadata and samples.
2. **Calculation unit tests** — analytical signals with known RMS, phase, sequence, frequency, power and impedance.
3. **Cross-tool comparison** — legally obtained records compared with trusted engineering tools; differences are investigated, not normalized away.
4. **Regression corpus** — every real parser/calculation bug gets a minimized redistributable test where possible.
5. **Performance regression** — representative large records and generated stress inputs.

## Example analytical fixture

A balanced 50 Hz three-phase current set:

- IA = 1000 A RMS ∠0°
- IB = 1000 A RMS ∠−120°
- IC = 1000 A RMS ∠+120°

Expected fundamental sequence result is approximately I1 = 1000 A with I0 and I2 near zero, within the algorithm tolerance documented by the test.

## Rule for derived values

Every displayed calculated quantity must be able to expose:

- source channels;
- algorithm;
- calculation window;
- frequency basis;
- primary/secondary scaling context;
- missing-data behavior.
