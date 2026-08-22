// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/record.hpp"

#include <filesystem>

namespace ardirec::comtrade {

class ConfigParser {
public:
    [[nodiscard]] RecordConfig parse_file(const std::filesystem::path& path) const;
};

[[nodiscard]] const char* to_string(DataFormat format) noexcept;

} // namespace ardirec::comtrade
