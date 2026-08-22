// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/record.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace ardirec::comtrade {

class DatReader {
public:
    // Reference decoder used by tests and the CLI. The production viewer will
    // use memory-mapped/lazy channel access so large records are not duplicated.
    [[nodiscard]] std::vector<SampleFrame> read(
        const RecordConfig& config,
        const std::filesystem::path& dat_path,
        std::size_t max_frames = 0) const;
};

} // namespace ardirec::comtrade
