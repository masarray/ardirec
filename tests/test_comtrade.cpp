// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/indexed_dat.hpp"
#include "ardirec/comtrade/parser.hpp"
#include "ardirec/comtrade/value_representation.hpp"

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
    require(static_cast<bool>(out), "temporary field-robustness fixture opens");
    out << text;
    require(static_cast<bool>(out), "temporary field-robustness fixture writes");
}
}

int main() {
    try {
        const std::filesystem::path dir = ARDIREC_TEST_DATA_DIR;
        const auto cfg = ardirec::comtrade::ConfigParser{}.parse_file(dir / "minimal_1999.cfg");
        require(cfg.station_name == "ARDIREC TEST", "station parse");
        require(cfg.revision_year == 1999, "revision parse");
        require(cfg.analog_channels.size() == 3, "analog count");
        require(cfg.status_channels.size() == 2, "status count");
        require(cfg.data_format == ardirec::comtrade::DataFormat::Ascii, "format parse");
        require(std::abs(cfg.analog_channels[0].a - 0.1) < 1e-12, "scale parse");

        using ardirec::comtrade::AnalogChannel;
        using ardirec::comtrade::ValueRepresentation;
        using ardirec::comtrade::recorded_representation;
        using ardirec::comtrade::representation_scale;

        require(cfg.analog_channels[0].primary.has_value()
                    && std::abs(*cfg.analog_channels[0].primary - 2000.0) < 1e-12,
                "parsed primary transformer value");
        require(cfg.analog_channels[0].secondary.has_value()
                    && std::abs(*cfg.analog_channels[0].secondary - 1.0) < 1e-12,
                "parsed secondary transformer value");
        require(recorded_representation(cfg.analog_channels[0]) == ValueRepresentation::Primary,
                "parsed recorded-side P metadata");
        require(std::abs(representation_scale(cfg.analog_channels[0], ValueRepresentation::Primary) - 1.0) < 1e-12,
                "parsed primary-recorded data stays primary");
        require(std::abs(representation_scale(cfg.analog_channels[0], ValueRepresentation::Secondary) - 0.0005) < 1e-12,
                "parsed primary-recorded data scales to secondary");

        const auto frames = ardirec::comtrade::DatReader{}.read(cfg, dir / "minimal_1999.dat");
        require(frames.size() == 4, "reference DAT frame count");
        require(std::abs(frames[1].analog[0] - 10.0) < 1e-12, "reference analog scaling");
        require(frames[2].status[0], "reference digital state");

        const auto asciiOpened = ardirec::comtrade::IndexedDatFile::open(cfg, dir / "minimal_1999.dat");
        require(asciiOpened.file != nullptr, "indexed ASCII DAT opens");
        require(asciiOpened.file->frameCount() == 4, "indexed ASCII frame count");
        require(std::abs(asciiOpened.file->analogValue(1, 0) - 10.0) < 1e-12,
                "indexed ASCII analog scaling matches reference decoder");
        require(asciiOpened.file->statusValue(2, 0), "indexed ASCII digital decode");
        const auto asciiIndex = asciiOpened.file->buildIndex(cfg.time_multiplier * 1.0e-6);
        require(asciiIndex.time_seconds.size() == 4, "indexed ASCII timestamp count");
        require(asciiIndex.analog_abs_peaks.size() == cfg.analog_channels.size(), "indexed ASCII peak summary");
        require(asciiIndex.status_active.size() == cfg.status_channels.size(), "indexed ASCII status summary");

        for (const auto* stem : {"binary", "binary32", "float32"}) {
            const std::string base(stem);
            const auto binary_cfg = ardirec::comtrade::ConfigParser{}.parse_file(dir / (base + ".cfg"));
            const auto binary_frames = ardirec::comtrade::DatReader{}.read(binary_cfg, dir / (base + ".dat"));
            require(binary_frames.size() == 2, "binary-family reference frame count");
            require(std::abs(binary_frames[1].analog[0] - 10.0) < 1e-5, "binary-family reference analog scaling");
            require(binary_frames[1].status[0] && binary_frames[1].status[1], "binary-family reference status decode");

            const auto indexed = ardirec::comtrade::IndexedDatFile::open(binary_cfg, dir / (base + ".dat"));
            require(indexed.file != nullptr, "binary-family indexed DAT opens");
            require(indexed.file->frameCount() == 2, "binary-family indexed frame count");
            require(std::abs(indexed.file->analogValue(1, 0) - binary_frames[1].analog[0]) < 1e-5,
                    "binary-family lazy analog equals reference decoder");
            require(indexed.file->statusValue(1, 0) && indexed.file->statusValue(1, 1),
                    "binary-family lazy packed status decode");
            const auto index = indexed.file->buildIndex(binary_cfg.time_multiplier * 1.0e-6);
            require(index.time_seconds.size() == 2, "binary-family compact index count");
        }

        // A non-integral final binary frame must not discard valid frames or throw.
        const auto binaryCfg = ardirec::comtrade::ConfigParser{}.parse_file(dir / "binary.cfg");
        const auto sourcePath = dir / "binary.dat";
        const auto temporaryPath = std::filesystem::temp_directory_path() / "ardirec_truncated_tail_binary.dat";
        {
            std::ifstream source(sourcePath, std::ios::binary);
            std::ofstream target(temporaryPath, std::ios::binary | std::ios::trunc);
            require(static_cast<bool>(source) && static_cast<bool>(target), "temporary truncated-tail fixture opens");
            target << source.rdbuf();
            const char damage = static_cast<char>(0x5a);
            target.write(&damage, 1);
        }
        const auto truncated = ardirec::comtrade::IndexedDatFile::open(binaryCfg, temporaryPath);
        require(truncated.file != nullptr, "truncated-tail binary DAT remains readable");
        require(truncated.file->frameCount() == 2, "truncated-tail binary DAT salvages complete frames");
        require(!truncated.diagnostics.empty(), "truncated-tail binary DAT reports a diagnostic");
        std::error_code removeError;
        std::filesystem::remove(temporaryPath, removeError);

        // P0 Field Robustness: recoverable CFG field damage must not abort the record.
        const auto robustCfgPath = std::filesystem::temp_directory_path() / "ardirec_field_robust.cfg";
        write_text(robustCfgPath,
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
        const auto robustParsed = ardirec::comtrade::ConfigParser{}.try_parse_file(robustCfgPath);
        require(robustParsed.ok(), "recoverable CFG field damage is salvaged");
        require(robustParsed.config->revision_year == 1991, "invalid revision falls back to 1991 semantics");
        require(robustParsed.config->analog_channels.size() == 1, "damaged analog definition is retained");
        require(robustParsed.config->status_channels.size() == 1, "damaged status definition is retained");
        require(robustParsed.config->analog_channels[0].id == "A1", "missing analog id gets deterministic fallback");
        require(std::abs(robustParsed.config->analog_channels[0].a - 1.0) < 1e-12,
                "invalid analog scale uses safe unity fallback");
        require(robustParsed.config->status_channels[0].normal_state == 0,
                "invalid digital normal state falls back safely");
        require(std::abs(robustParsed.config->nominal_frequency - 50.0) < 1e-12,
                "invalid nominal frequency uses 50 Hz fallback");
        require(std::abs(robustParsed.config->time_multiplier - 1.0) < 1e-12,
                "invalid time multiplier uses unity fallback");
        require(!robustParsed.config->diagnostics.empty(), "salvaged CFG records diagnostics");

        // Structurally truncated CFG is rejected as a result, not by an uncaught parser exception.
        const auto brokenCfgPath = std::filesystem::temp_directory_path() / "ardirec_structurally_broken.cfg";
        write_text(brokenCfgPath, "BROKEN,RECORDER,1999\n1,1A,0D\n");
        const auto brokenParsed = ardirec::comtrade::ConfigParser{}.try_parse_file(brokenCfgPath);
        require(!brokenParsed.ok(), "structurally truncated CFG is rejected");
        require(!brokenParsed.error.empty(), "structurally truncated CFG reports an error");

        // Corrupt ASCII DAT rows: bad sample/timestamp rows are skipped; valid-time rows
        // remain indexed and bad analog values become NaN instead of crashing.
        const auto robustDatPath = std::filesystem::temp_directory_path() / "ardirec_field_robust.dat";
        write_text(robustDatPath,
                   "1,0,1,0\n"
                   "junk-row\n"
                   "2,1000,bad,1\n"
                   "3,2000\n"
                   "4,3000,4,0\n");
        const auto robustDat = ardirec::comtrade::IndexedDatFile::open(*robustParsed.config, robustDatPath);
        require(robustDat.file != nullptr, "corrupt ASCII DAT retains readable rows");
        require(robustDat.file->frameCount() == 4, "invalid sample/timestamp row is skipped only");
        require(!robustDat.diagnostics.empty(), "skipped ASCII row reports diagnostics");
        require(std::isnan(robustDat.file->analogValue(1, 0)), "invalid analog field maps to NaN");
        require(std::isnan(robustDat.file->analogValue(2, 0)), "missing analog field maps to NaN");
        require(std::abs(robustDat.file->analogValue(3, 0) - 4.0) < 1e-12,
                "valid data after damaged rows remains accessible");
        const auto robustIndex = robustDat.file->buildIndex(1.0e-6);
        require(robustIndex.time_seconds.size() == 4, "corrupt ASCII DAT builds a valid-time index");

        std::filesystem::remove(robustCfgPath, removeError);
        std::filesystem::remove(brokenCfgPath, removeError);
        std::filesystem::remove(robustDatPath, removeError);

        AnalogChannel secondary_recorded;
        secondary_recorded.primary = 500000.0;
        secondary_recorded.secondary = 100.0;
        secondary_recorded.primary_secondary = "S";
        require(std::abs(representation_scale(secondary_recorded, ValueRepresentation::Secondary) - 1.0) < 1e-12,
                "secondary-recorded data stays unchanged in secondary representation");
        require(std::abs(representation_scale(secondary_recorded, ValueRepresentation::Primary) - 5000.0) < 1e-12,
                "secondary-recorded data scales to primary ratio");

        AnalogChannel primary_recorded = secondary_recorded;
        primary_recorded.primary_secondary = "P";
        require(std::abs(representation_scale(primary_recorded, ValueRepresentation::Primary) - 1.0) < 1e-12,
                "primary-recorded data stays unchanged in primary representation");
        require(std::abs(representation_scale(primary_recorded, ValueRepresentation::Secondary) - 0.0002) < 1e-12,
                "primary-recorded data scales down to secondary ratio");

        AnalogChannel no_ratio;
        no_ratio.primary_secondary = "S";
        require(std::abs(representation_scale(no_ratio, ValueRepresentation::Primary) - 1.0) < 1e-12,
                "missing ratio safely falls back to one-to-one");

        const auto bundle = ardirec::comtrade::locate_bundle(dir / "minimal_1999.cfg");
        require(!bundle.dat.empty(), "bundle DAT auto-location");
        require(!bundle.hdr.empty(), "bundle HDR auto-location");
        require(!bundle.rio.empty(), "bundle RIO auto-location");
        require(bundle.xrio.empty(), "bundle does not invent XRIO sidecar");

        std::cout << "ardirec core tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec core tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
