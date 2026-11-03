# ArDiRec R8 Installer Distribution

The v0.2.0 public Stable release follows owner-reported manual acceptance, full CI,
CodeQL and packaged installer smoke testing. CI success alone does not certify SIGRA
parity or every vendor COMTRADE format.

## Windows (x64, Windows 10/11)

The Windows ZIP contains an Inno Setup EXE and README. The wizard installs
per-user under LocalAppData, requires no administrator rights, registers a
Start menu shortcut, offers an optional desktop shortcut, upgrades using the
same AppId and provides standard Windows uninstall. It does not register
automatic COMTRADE file associations or run unwanted software.

Before packaging, Windows Build verifies the Qt-deployed folder and VC
runtime. The real Inno installer is silently installed in a test-only path,
launched offscreen for 5 seconds and uninstalled. All failures block ZIP
creation. The installer is unsigned; ZIP does not suppress SmartScreen.

## Linux (x86_64)

The Linux ZIP contains:
- a .deb graphical installer for Ubuntu/Debian-compatible desktops;
- a portable .AppImage for other compatible Linux desktop distributions;
- beginner-friendly installation instructions.

The .deb packages the **same** Qt-bundled AppImage and a standard desktop
launcher; users install it using their graphical package manager and uninstall
using it too. This operation may require system administrator authorization.
The launcher uses APPIMAGE_EXTRACT_AND_RUN=1 to avoid a libfuse2 requirement.
Extraction mode does not provide a sandbox.

Build AppImage on Ubuntu 22.04 (glibc 2.35) with linuxdeploy and the Qt 6
plugin, including all imported QML modules. Both QML and platform plugin
deployment and a real timed startup test must pass. Distros older than Ubuntu
22.04 and uncommon desktop stacks are not guaranteed to work.

## Publishing safeguard

Build workflows upload CI artifacts for pull requests and main; they do not
create public releases. A manual release must match VERSION, pass the
strict numerical/R5/R6 gates, and successfully build **both** installers.
The publishing job downloads artifacts from that same workflow run, validates
the two ZIP filenames, calculates SHA256SUMS.txt, and publishes only an
explicitly requested, non-prerelease stable SemVer version. It never publishes
on source pushes. The owner has reported product testing and approved the
initial stable publication. This does not assert full SIGRA parity or complete
multi-vendor COMTRADE certification; broader acceptance remains ongoing.
