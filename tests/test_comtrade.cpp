// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"
#include "ardirec/comtrade/value_representation.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
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
        require(frames.size() == 4, "DAT frame count");
        require(std::abs(frames[1].analog[0] - 10.0) < 1e-12, "analog scaling");
        require(frames[2].status[0], "digital state");

        for (const auto* stem : {"binary", "binary32", "float32"}) {
            const auto binary_cfg = ardirec::comtrade::ConfigParser{}.parse_file(dir / (std::string(stem) + ".cfg"));
            const auto binary_frames = ardirec::comtrade::DatReader{}.read(binary_cfg, dir / (std::string(stem) + ".dat"));
            require(binary_frames.size() == 2, "binary-family frame count");
            require(std::abs(binary_frames[1].analog[0] - 10.0) < 1e-5, "binary-family analog scaling");
            require(binary_frames[1].status[0] && binary_frames[1].status[1], "packed status decode");
        }

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
        require(!bundle.dat.empty(), "bundle auto-location");
        std::cout << "ardirec core tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec core tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
