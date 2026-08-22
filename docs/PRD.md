# ardirec — Master Product Requirements Document

## 1. Product identity

**ardirec** is a vendor-neutral COMTRADE workstation for protection, commissioning and disturbance-analysis engineers.

**Core promise:** *Open any disturbance record. Understand it immediately.*

The first public product objective is not automatic fault diagnosis. It is **manual disturbance-analysis parity** with established engineering tools while delivering a faster, cleaner, more modern interaction model.

## 2. Primary users

- Protection engineers investigating relay trips and system faults.
- Commissioning engineers validating schemes and IED behavior.
- Utility disturbance-analysis teams working across multiple vendors.
- EPC/OEM engineers exchanging COMTRADE records between organizations.
- Researchers and students who need a transparent analysis toolchain.

## 3. Viewer 1.0 definition of done

A protection engineer can receive a multi-vendor COMTRADE record and complete normal manual investigation without a functional need to reopen a proprietary viewer for:

- analog and digital time signals;
- synchronized dual cursors and event timing;
- primary/secondary scaling;
- RMS and fundamental phasors;
- vector diagrams and symmetrical components;
- frequency, P/Q/S/PF;
- impedance loops and R-X locus;
- harmonics/spectrum;
- calculated/derived signals;
- RIO/XRIO characteristic overlays;
- value tables, workspace persistence and export.

Specialized vendor-only relay internals are outside parity unless represented in portable data.

## 4. Product principles

### Compatibility first
A record that is standards-compliant should load. A recoverable vendor deviation should open with a diagnostic rather than fail silently.

### Data is immutable
Raw record metadata and samples are never silently rewritten. Normalized interpretations exist in a separate model.

### Waveform is the hero
The main canvas receives screen space. Panels collapse. There are no oversized dashboard cards.

### Deterministic before intelligent
Fault intelligence may explain deterministic facts later; it must never replace validated calculations.

### Explain every number
Derived quantities expose source signals, algorithm, window, frequency basis and scaling context.

## 5. Functional gates

### G0 — Foundation
- Public repository quality baseline.
- Core parser architecture.
- CLI smoke path.
- Qt Quick shell and Scene Graph renderer path.
- Tests, validation plan and performance contract.

### G1 — Real Viewer
- Open CFG and auto-locate DAT.
- Analog + packed digital rendering from real records.
- Shared horizontal timebase.
- GPU pan/zoom.
- Dual cursors.
- LOD pyramid and progressive loading.

### G2 — Compatibility
- 1991 / 1999 / IEC 2001 / 2013.
- ASCII / BINARY / BINARY32 / FLOAT32 / CFF.
- Multiple sample rates.
- skew, timemult, time quality, missing values, encodings.
- compatibility diagnostics and torture suite.

### G3 — Electrical core
- RMS variants.
- fundamental phasors.
- symmetrical components.
- frequency/ROCOF basis.
- P/Q/S/PF.

### G4 — SIGRA-class views
- vector view.
- R-X/locus.
- harmonics and spectrum.
- value table.
- synchronized global investigation context.

### G5 — Professional workflow
- calculated-signal/formula engine.
- RIO/XRIO import and overlay.
- device/channel profiles.
- workspace persistence.
- image/data export.
- record-health diagnostics.

### G6 — Viewer 1.0 release gate
- regression suite passes.
- supported-format matrix verified.
- performance budget met.
- no silent calculation fallback.

### G7+ — After viewer parity
Multi-record combination and synchronization, then fault classification/location, then deterministic protection-sequence intelligence.

## 6. UX requirements

Design language: **Precision Industrial**.

- dark graphite default; light theme architecture remains possible;
- compact type scale and tabular numbers;
- thin dividers/gridlines;
- signal color indicates data, not decoration;
- animation only for spatial continuity, typically 100–180 ms;
- collapsible signal browser and inspector;
- no modal wizard for normal record open;
- opening a CFG immediately searches for sibling files.

## 7. Engineering safety requirements

- No calculation may be presented as validated until covered by reference tests.
- Invalid/missing data must propagate explicitly.
- User overrides must be visible in the investigation model.
- Auto channel classification must expose confidence and never modify source metadata.
- Fault-analysis claims later must distinguish measured fact, calculated result and heuristic interpretation.
