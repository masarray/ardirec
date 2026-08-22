// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"

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

        const auto bundle = ardirec::comtrade::locate_bundle(dir / "minimal_1999.cfg");
        require(!bundle.dat.empty(), "bundle auto-location");
        std::cout << "ardirec core tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec core tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
