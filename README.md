<p align="center">
  <img src="icon/android-chrome-192x192.png" alt="ArDiRec" width="84" height="84">
</p>
<h1 align="center">ArDiRec</h1>
<p align="center">Open-source COMTRADE disturbance analysis for protection and power-system engineers.</p>
<p align="center">
  <a href="https://github.com/masarray/ardirec/releases/latest">Download stable</a> ·
  <a href="docs/GETTING_STARTED.md">Get started</a> ·
  <a href="docs/FAQ.md">FAQ</a> ·
  <a href="https://github.com/masarray/ardirec/issues">Report an issue</a>
</p>

ArDiRec (Ari Disturbance Recorder) is a vendor-neutral desktop workstation for opening, inspecting and understanding disturbance recordings. It combines time-domain signals, digital events and electrical analysis in a responsive workspace. Your source record is never silently rewritten.

**Current public release:** [v0.2.0 Stable](https://github.com/masarray/ardirec/releases/tag/v0.2.0) · Windows 10/11 x64 · Linux x86_64

## Download

| Platform | Package | Installation |
| --- | --- | --- |
| Windows 10/11 (x64) | [Windows installer ZIP](https://github.com/masarray/ardirec/releases/download/v0.2.0/ArDiRec-v0.2.0-Windows-x64-Setup.zip) | Extract ZIP, run the setup wizard; installs for the current user |
| Linux (x86_64) | [Linux installer ZIP](https://github.com/masarray/ardirec/releases/download/v0.2.0/ArDiRec-v0.2.0-Linux-x86_64-Setup.zip) | Extract ZIP; choose the Debian `.deb` installer or portable `.AppImage` |

Verify downloads against [SHA256SUMS.txt](https://github.com/masarray/ardirec/releases/download/v0.2.0/SHA256SUMS.txt). Windows binaries are **not code-signed**; SmartScreen may ask for confirmation. See [Getting started](docs/GETTING_STARTED.md) for detailed instructions.

## Explore a disturbance record

- **Time signals:** synchronized analog waveforms and digital pickup/trip tracks, with trigger-relative timing, zoom, pan and dual measurement cursors.
- **Electrical views:** instantaneous values, one-cycle RMS, phasors, R–X impedance locus, harmonics and a value table.
- **Measurement context:** C1/C2 readings, differences, primary/secondary representation when valid transformer metadata is present, and phase-aware channel handling.
- **Engineering workflow:** multi-window analysis workspace, fault-loop selection, protection-zone overlays for supported RIO/XRIO data, and explicit diagnostics where an interpretation is unsupported.
- **Performance:** indexed/lazy record access, bounded waveform level-of-detail rendering and asynchronous analysis. CI exercises large-record benchmarks, including 10-million-sample and 120-channel scenarios.

The first Stable release focuses on **manual investigation**. It is not an automatic fault-classification system or a guarantee that every vendor extension can be interpreted. Supported formats and remaining gaps are documented in [COMTRADE compatibility](docs/COMTRADE_COMPATIBILITY.md).

## Quick start

1. Download the package for your operating system and extract the ZIP.
2. Install or launch ArDiRec.
3. Open a COMTRADE `.cfg` file. Keep the matching `.dat` and any available companion files in the same folder.
4. Select relevant voltage, current and digital signals. Navigate the time axis, then position C1/C2 around an event.
5. Switch to RMS, phasor, locus, harmonics or table views as needed. Check scaling and units before making an engineering decision.

See the [step-by-step guide](docs/GETTING_STARTED.md), [FAQ](docs/FAQ.md) and [known limitations](docs/COMTRADE_COMPATIBILITY.md).

## Building from source

Requires CMake 3.24+, a C++20 compiler, and Qt 6.8+ with Qt Quick/QML for the desktop application.

```bash
cmake -S . -B build -DARDIREC_BUILD_DESKTOP=ON -DARDIREC_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For a core-only build, set `-DARDIREC_BUILD_DESKTOP=OFF`. Use a compatible Qt installation and CMake toolchain for your platform. See [architecture](docs/ARCHITECTURE.md), [performance contract](docs/PERFORMANCE_CONTRACT.md), and [contribution guide](CONTRIBUTING.md).

## Project and trust

ArDiRec is written in C++20 with Qt Quick and custom Qt Scene Graph rendering. The source is licensed under [GPL-3.0-or-later](LICENSE). Automated CI covers numerical regression, core builds across operating systems, desktop behavior, Windows installation/uninstallation, Linux packaging and code scanning.

**Be precise about validation:** successful tests and a Stable release do not establish compatibility with every field recording or certify third-party engineering parity. Verify calculated values against your equipment context and approved engineering procedures.

- [Releases and changelog](CHANGELOG.md)
- [Project status](PROJECT_STATUS.md)
- [Security and responsible disclosure](SECURITY.md)
- [Contributing](CONTRIBUTING.md)
- [Issue tracker](https://github.com/masarray/ardirec/issues)

Copyright and trademarks remain with their respective owners. ArDiRec is an independent open-source project.
