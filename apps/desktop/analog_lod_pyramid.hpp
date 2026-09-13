// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/indexed_dat.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace ardirec::desktop {

// Immutable multi-resolution visual extrema cache. Level 0 reuses the bounded
// single-pass DAT LOD produced by IndexedDatFile; coarser levels are derived from
// it in the background loader without touching raw DAT again.
class AnalogLodPyramid final {
public:
    struct Level final {
        std::size_t blockSize{0};
        std::size_t blockCount{0};
        std::vector<float> minima;
        std::vector<float> maxima;
    };

    [[nodiscard]] std::size_t levelCount() const noexcept;
    [[nodiscard]] std::size_t channelCount() const noexcept { return m_channelCount; }
    [[nodiscard]] std::size_t blockSize(std::size_t level) const noexcept;
    [[nodiscard]] std::size_t blockCount(std::size_t level) const noexcept;

    // Select the coarsest level that still resolves the requested screen bucket.
    // Callers can fall back to raw samples when even level 0 is too coarse.
    [[nodiscard]] std::size_t bestLevel(std::size_t samplesPerPixel) const noexcept;

    [[nodiscard]] bool blockExtrema(std::size_t level,
                                    std::size_t channel,
                                    std::size_t block,
                                    double& minimum,
                                    double& maximum) const noexcept;

private:
    friend std::shared_ptr<const AnalogLodPyramid>
    buildAnalogLodPyramid(std::shared_ptr<const ardirec::comtrade::AnalogLodIndex> base);

    std::shared_ptr<const ardirec::comtrade::AnalogLodIndex> m_base;
    std::vector<Level> m_coarseLevels;
    std::size_t m_channelCount{0};
};

[[nodiscard]] std::shared_ptr<const AnalogLodPyramid>
buildAnalogLodPyramid(std::shared_ptr<const ardirec::comtrade::AnalogLodIndex> base);

} // namespace ardirec::desktop
