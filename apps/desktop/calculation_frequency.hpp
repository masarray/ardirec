// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/indexed_dat.hpp"

#include <memory>
#include <string>

struct CalculationFrequencySelection final {
    double hz{0.0};
    std::string provenance{"COMTRADE nominal"};
};

// Lookup is O(number of live open documents), normally one. The selection is
// published by the background loader and keyed by the immutable DAT store so UI
// consumers never rerun frequency estimation while scrubbing.
[[nodiscard]] CalculationFrequencySelection calculationFrequencySelectionFor(
    const std::shared_ptr<const ardirec::comtrade::IndexedDatFile>& data);
