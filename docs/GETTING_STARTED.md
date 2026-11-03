# Getting started with ArDiRec

ArDiRec is a desktop application for inspecting disturbance recordings in COMTRADE format. This guide covers the first manual investigation on v0.2.0.

## 1. Download and install

Visit the [official Stable release](https://github.com/masarray/ardirec/releases/tag/v0.2.0). Choose one ZIP:

- **Windows 10/11 x64:** extract `ArDiRec-v0.2.0-Windows-x64-Setup.zip`, then run the setup executable. The installer supports current-user installation, Start menu launch and standard uninstall.
- **Linux x86_64:** extract `ArDiRec-v0.2.0-Linux-x86_64-Setup.zip`. On compatible Debian/Ubuntu systems, install the `.deb` with your package manager. Alternatively, mark the `.AppImage` as executable and run it on a compatible desktop.

The Windows executable is unsigned and Windows SmartScreen may warn. Check the source of the ZIP and compare its SHA-256 value with the official [checksums](https://github.com/masarray/ardirec/releases/download/v0.2.0/SHA256SUMS.txt). A ZIP does not disable SmartScreen.

## 2. Prepare a record

Keep the matching `.cfg` and `.dat` files together in a directory. Other accompanying files (`.hdr`, `.inf`, RIO/XRIO where applicable) can provide useful context. ArDiRec does not silently change the original source files.

Open the `.cfg` from the application. If the file is incomplete or carries unsupported metadata, inspect the diagnostics before relying on calculated values.

## 3. Review the event timeline

1. Identify recorded voltage, current, and digital pickup/trip channels.
2. Use the time ruler and trigger reference to locate the disturbance.
3. Zoom or pan to the event interval.
4. Position the two cursors C1 and C2 around the transition and read the time difference.

Record sample rate, nominal frequency, phase allocation and relevant channel units before interpreting results.

## 4. Inspect electrical quantities

- Switch between **Instantaneous** and **one-cycle RMS** for the waveform view.
- Use **Phasor** to compare phase magnitude and angle at the selected time.
- Use **R–X Locus** to inspect supported impedance loops and imported protection-zone overlays.
- Use **Harmonics** for spectral observations and **Value Table** for exact time-point quantities.
- Where available, select **Primary** or **Secondary** representation; check transformer-ratio metadata and channel semantics first.

The views are an aid to engineering interpretation, not a substitute for approved protection test procedures.

## 5. Troubleshooting

**Record fails to open:** confirm that the companion DAT exists and filenames match; check whether files are truncated or damaged.

**Unexpected values:** verify primary/secondary selection, units, nominal frequency, channel phase mapping, signal validity and any imported protection metadata.

**Nothing appears in the Locus:** confirm a supported loop, adequate one-cycle measurement window and valid samples; invalid or incomplete windows may correctly produce gaps.

**Windows warning:** the installer is not code-signed. Verify the official release and SHA-256 checksum before choosing how to proceed.

**Linux will not launch:** confirm x86_64, desktop compatibility, execute permissions and required operating-system support. Older distributions are not guaranteed.

## 6. Getting help

Read the [FAQ](FAQ.md) and [compatibility notes](COMTRADE_COMPATIBILITY.md). For bugs, [open an issue](https://github.com/masarray/ardirec/issues) with exact app version, operating system, COMTRADE revision/DAT format, reproduction steps and a sanitized sample if you have permission to share it. Do not publish private substation records.
