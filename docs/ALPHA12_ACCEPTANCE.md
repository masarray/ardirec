# alpha.12 acceptance gate

The calculation-parity hotfix is not releasable unless all of these hold:

- Existing COMTRADE parser/decoder/core tests remain green on Windows, Linux and macOS.
- Synthetic 1 kHz / 50 Hz spectrum requests H25 but returns only H1..H10.
- The same fixture does not double-count aliased H11+ energy in THD.
- Fundamental and harmonic phase values are referenced to the requested cursor time using sine-wave phase convention.
- `last_extreme_value` returns the most recent completed turning point rather than the maximum absolute sample in the cycle.
- Table presents DC percentage independently from signed absolute DC.
- Qt/QML desktop build and tests pass.
- CodeQL passes.
- Windows Release build, CTest, windeployqt and portable ZIP packaging pass.
- line1.CFG manual parity check keeps H1/H2/H3/H5 values unchanged while phase/DC/extremum semantics converge with the reference tool.
