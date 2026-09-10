// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/indexed_dat.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "temporary fixture opens");
    out << text;
    require(static_cast<bool>(out), "temporary fixture writes");
}
} // namespace

int main() {
    try {
        const auto base = std::filesystem::temp_directory_path();
        const auto cfgPath = base / "ardirec_p0_field_robustness.cfg";
        const auto datPath = base / "ardirec_p0_field_robustness.dat";
        const auto brokenCfgPath = base / "ardirec_p0_field_structural.cfg";

        write_text(cfgPath,
                   "FIELD TEST,,not-a-year\n"
                   "2,1A,1D\n"
                   "oops,,A,LINE,V,bad,,oops,,bad\n"
                   "oops,,A,LINE,2\n"
                   "bad-frequency\n"
                   "1\n"
                   "bad-rate,not-a-sample\n"
                   "01/01/2020,00:00:00.000000\n"
                   "01/01/2020,00:00:00.010000\n"
                   "ASCII\n"
                   "bad-multiplier\n");

        auto parsed = ardirec::comtrade::ConfigParser{}.try_parse_file(cfgPath);
        require(parsed.ok(), "recoverable CFG corruption is salvaged");
        const auto& cfg = *parsed.config;
        require(cfg.revision_year == 1991, "invalid revision uses safe 1991 semantics");
        require(cfg.analog_channels.size() == 1 && cfg.status_channels.size() == 1,
                "declared channel structure is retained");
        require(cfg.analog_channels[0].id == "A1", "missing analog id has deterministic fallback");
        require(std::abs(cfg.analog_channels[0].a - 1.0) < 1e-12,
                "invalid analog scale uses unity fallback");
        require(cfg.status_channels[0].normal_state == 0,
                "invalid digital normal state uses safe zero fallback");
        require(std::abs(cfg.nominal_frequency - 50.0) < 1e-12,
                "invalid nominal frequency uses 50 Hz fallback");
        require(std::abs(cfg.time_multiplier - 1.0) < 1e-12,
                "invalid time multiplier uses unity fallback");
        require(!cfg.diagnostics.empty(), "CFG salvage emits diagnostics");

        write_text(brokenCfgPath, "BROKEN,RECORDER,1999\n1,1A,0D\n");
        const auto broken = ardirec::comtrade::ConfigParser{}.try_parse_file(brokenCfgPath);
        require(!broken.ok() && !broken.error.empty(),
                "structural CFG truncation returns a controlled error");

        // Field 0/1 corruption makes the row unusable and is skipped. Once a
        // valid sample/timestamp exists, partial payload corruption must not
        // throw or discard later valid frames.
        write_text(datPath,
                   "1,0,1,0\n"
                   "junk-row\n"
                   "2,1000,bad,1\n"
                   "3,2000,,bad\n"
                   "4,3000,4,0\n");

        const auto opened = ardirec::comtrade::IndexedDatFile::open(cfg, datPath);
        require(opened.file != nullptr, "corrupt ASCII DAT retains readable rows");
        require(opened.file->frameCount() == 4,
                "only the row with invalid sample/timestamp is rejected");
        require(!opened.diagnostics.empty(), "skipped DAT row is diagnosed");
        require(std::isnan(opened.file->analogValue(1, 0)),
                "invalid analog token maps to NaN");
        require(std::isnan(opened.file->analogValue(2, 0)),
                "empty analog token maps to NaN");
        require(std::abs(opened.file->analogValue(3, 0) - 4.0) < 1e-12,
                "valid data after damage remains accessible");

        const auto knownHigh = opened.file->statusState(1, 0);
        const auto unknown = opened.file->statusState(2, 0);
        require(knownHigh.has_value() && *knownHigh,
                "valid digital state survives corrupt neighboring rows");
        require(!unknown.has_value(), "invalid digital field is represented as unknown");
        require(!opened.file->statusValue(2, 0),
                "unknown digital state falls back to configured normal state");

        const auto index = opened.file->buildIndex(1.0e-6);
        require(index.time_seconds.size() == 4,
                "valid-time frames remain in the compact index");
        require(index.digital_edge_times.size() == 1,
                "unknown digital sample does not create a synthetic edge");
        require(std::abs(index.digital_edge_times.front() - 0.003) < 1e-12,
                "real post-gap digital edge is retained");
        require(index.diagnostics.size() >= 2,
                "invalid analog and digital payloads produce aggregated diagnostics");

        std::error_code ignored;
        std::filesystem::remove(cfgPath, ignored);
        std::filesystem::remove(datPath, ignored);
        std::filesystem::remove(brokenCfgPath, ignored);

        std::cout << "ardirec field robustness tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec field robustness tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
