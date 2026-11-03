# Frequently asked questions

### Is ArDiRec free and open source?

Yes. The source is available under [GPL-3.0-or-later](../LICENSE). Download the official builds from [GitHub Releases](https://github.com/masarray/ardirec/releases/latest).

### What files should I open?

Open the COMTRADE `.cfg` and keep the corresponding `.dat` in the same folder. Companion files may add metadata. Supported interpretation varies by revision and vendor extension; see [compatibility](COMTRADE_COMPATIBILITY.md).

### Which operating systems have packaged builds?

Windows 10/11 x64 and Linux x86_64 are packaged for v0.2.0. Other operating systems may require building from source; a public macOS installer is not offered in this release.

### Why does Windows warn about the installer?

The Windows executable is not code-signed. ZIP packaging does not suppress SmartScreen. Download only from the project's official release and verify the SHA-256 checksum.

### Does the software automatically diagnose a fault?

No. The initial Stable release concentrates on manual waveform, event, phasor, harmonic and impedance analysis. It does not claim automatic fault diagnosis or certified fault location.

### Why do some calculated values disappear or show gaps?

Analysis requires valid samples, channel semantics and an adequate measurement window. Missing/invalid data or a status transition can invalidate a result. This is safer than displaying a made-up value.

### Are all RIO and XRIO files supported?

No. Standardized and supported constructs are interpreted when possible. Proprietary formulas, converters and unsupported attributes are not silently guessed.

### Can I share a COMTRADE sample in a GitHub issue?

Only if you own the data or have permission to redistribute it. Remove sensitive identifiers and operational information. For vulnerabilities, use the process in [Security](../SECURITY.md).

### How do I request a feature or report a problem?

Use the [issue tracker](https://github.com/masarray/ardirec/issues). Include version, platform, expected result, actual result and reproducible steps.
