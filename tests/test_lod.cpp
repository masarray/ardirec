// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/indexed_dat.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
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
        auto opened = ardirec::comtrade::IndexedDatFile::open(cfg, dir / "minimal_1999.dat");
        require(opened.file != nullptr, "LOD fixture DAT opens");

        auto index = opened.file->buildIndex(cfg.time_multiplier * 1.0e-6);
        require(index.analog_lod != nullptr, "background index builds visual LOD");
        require(index.analog_lod->channel_count == cfg.analog_channels.size(), "LOD channel count matches CFG");
        require(index.analog_lod->block_size > 0 && index.analog_lod->block_count > 0,
                "LOD has a bounded block layout");

        double expectedLow = std::numeric_limits<double>::infinity();
        double expectedHigh = -std::numeric_limits<double>::infinity();
        const std::size_t blockEnd = std::min(opened.file->frameCount(), index.analog_lod->block_size);
        for (std::size_t frame = 0; frame < blockEnd; ++frame) {
            const double value = opened.file->analogValue(frame, 0);
            if (!std::isfinite(value)) continue;
            expectedLow = std::min(expectedLow, value);
            expectedHigh = std::max(expectedHigh, value);
        }

        double low = 0.0;
        double high = 0.0;
        require(index.analog_lod->blockExtrema(0, 0, low, high), "LOD exposes first block extrema");
        require(std::abs(low - expectedLow) < 1.0e-4, "LOD preserves minimum excursion");
        require(std::abs(high - expectedHigh) < 1.0e-4, "LOD preserves maximum excursion");

        opened.file->publishAnalogLod(index.analog_lod);
        const auto published = opened.file->analogLodSnapshot();
        require(published == index.analog_lod, "published immutable LOD snapshot is retained by DAT source");

        std::cout << "ardirec LOD tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec LOD tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
