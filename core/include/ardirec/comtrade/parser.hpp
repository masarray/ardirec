// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/record.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace ardirec::comtrade {

struct ConfigParseResult {
    std::optional<RecordConfig> config;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return config.has_value() && error.empty(); }
    explicit operator bool() const noexcept { return ok(); }
};

class ConfigParser {
public:
    // Production path: never throws for malformed field data. Unrecoverable
    // structural errors are returned in `error`, while recoverable field issues
    // are preserved as RecordConfig::diagnostics.
    [[nodiscard]] ConfigParseResult try_parse_file(const std::filesystem::path& path) const noexcept;

    // Backwards-compatible convenience wrapper for CLI/tests that still expect
    // exception semantics. Desktop/background loading should use try_parse_file().
    [[nodiscard]] RecordConfig parse_file(const std::filesystem::path& path) const;
};

[[nodiscard]] const char* to_string(DataFormat format) noexcept;

} // namespace ardirec::comtrade
