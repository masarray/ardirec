// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Canonical desktop density. 7 analog channels + 4 digital rows fit within
// about 760 logical px (including ruler/section headers). Do not redefine in
// Main → MDI → AnalysisHost → TimeSignalsView: those bindings MUST forward.
var analogTrackHeight = 72
var digitalTrackHeight = 24
