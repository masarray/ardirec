// SPDX-License-Identifier: GPL-3.0-or-later
#include "analog_lod_pyramid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ardirec::desktop {
namespace {
constexpr std::size_t kMaximumAdditionalLodCells = 2'000'000u;

[[nodiscard]] bool finite_pair(float low, float high) noexcept {
    return std::isfinite(low) && std::isfinite(high) && low <= high;
}
} // namespace

std::size_t AnalogLodPyramid::levelCount() const noexcept {
    return m_base ? 1u + m_coarseLevels.size() : 0u;
}

std::size_t AnalogLodPyramid::blockSize(std::size_t level) const noexcept {
    if (!m_base) return 0u;
    if (level == 0u) return m_base->block_size;
    const std::size_t index = level - 1u;
    return index < m_coarseLevels.size() ? m_coarseLevels[index].blockSize : 0u;
}

std::size_t AnalogLodPyramid::blockCount(std::size_t level) const noexcept {
    if (!m_base) return 0u;
    if (level == 0u) return m_base->block_count;
    const std::size_t index = level - 1u;
    return index < m_coarseLevels.size() ? m_coarseLevels[index].blockCount : 0u;
}

std::size_t AnalogLodPyramid::bestLevel(std::size_t samplesPerPixel) const noexcept {
    if (!m_base || levelCount() == 0u) return 0u;
    samplesPerPixel = std::max<std::size_t>(1u, samplesPerPixel);
    const std::size_t target = samplesPerPixel > std::numeric_limits<std::size_t>::max() / 2u
                                   ? std::numeric_limits<std::size_t>::max()
                                   : samplesPerPixel * 2u;
    std::size_t best = 0u;
    for (std::size_t level = 1u; level < levelCount(); ++level) {
        if (blockSize(level) > target) break;
        best = level;
    }
    return best;
}

bool AnalogLodPyramid::blockExtrema(std::size_t level,
                                    std::size_t channel,
                                    std::size_t block,
                                    double& minimum,
                                    double& maximum) const noexcept {
    if (!m_base || channel >= m_channelCount) return false;
    if (level == 0u) return m_base->blockExtrema(channel, block, minimum, maximum);

    const std::size_t levelIndex = level - 1u;
    if (levelIndex >= m_coarseLevels.size()) return false;
    const auto& selected = m_coarseLevels[levelIndex];
    if (block >= selected.blockCount || m_channelCount == 0u) return false;
    if (block > (std::numeric_limits<std::size_t>::max() - channel) / m_channelCount) return false;
    const std::size_t cell = block * m_channelCount + channel;
    if (cell >= selected.minima.size() || cell >= selected.maxima.size()) return false;
    const float low = selected.minima[cell];
    const float high = selected.maxima[cell];
    if (!finite_pair(low, high)) return false;
    minimum = static_cast<double>(low);
    maximum = static_cast<double>(high);
    return true;
}

std::shared_ptr<const AnalogLodPyramid>
buildAnalogLodPyramid(std::shared_ptr<const ardirec::comtrade::AnalogLodIndex> base) {
    if (!base || base->channel_count == 0u || base->block_size == 0u || base->block_count == 0u) return {};

    auto pyramid = std::make_shared<AnalogLodPyramid>();
    pyramid->m_base = std::move(base);
    pyramid->m_channelCount = pyramid->m_base->channel_count;

    std::size_t previousBlockSize = pyramid->m_base->block_size;
    std::size_t previousBlockCount = pyramid->m_base->block_count;
    std::size_t additionalCells = 0u;

    while (previousBlockCount > 1u) {
        const std::size_t nextBlockCount = (previousBlockCount + 1u) / 2u;
        if (nextBlockCount > std::numeric_limits<std::size_t>::max() / pyramid->m_channelCount) break;
        const std::size_t cells = nextBlockCount * pyramid->m_channelCount;
        if (cells > kMaximumAdditionalLodCells - std::min(additionalCells, kMaximumAdditionalLodCells)) break;

        AnalogLodPyramid::Level level;
        level.blockSize = previousBlockSize > std::numeric_limits<std::size_t>::max() / 2u
                              ? std::numeric_limits<std::size_t>::max()
                              : previousBlockSize * 2u;
        level.blockCount = nextBlockCount;
        level.minima.assign(cells, std::numeric_limits<float>::infinity());
        level.maxima.assign(cells, -std::numeric_limits<float>::infinity());

        const std::size_t sourceLevel = pyramid->levelCount() - 1u;
        for (std::size_t block = 0u; block < nextBlockCount; ++block) {
            for (std::size_t channel = 0u; channel < pyramid->m_channelCount; ++channel) {
                double low = std::numeric_limits<double>::infinity();
                double high = -std::numeric_limits<double>::infinity();
                bool valid = false;
                for (std::size_t childOffset = 0u; childOffset < 2u; ++childOffset) {
                    const std::size_t child = block * 2u + childOffset;
                    if (child >= previousBlockCount) break;
                    double childLow = 0.0;
                    double childHigh = 0.0;
                    if (!pyramid->blockExtrema(sourceLevel, channel, child, childLow, childHigh)) continue;
                    low = std::min(low, childLow);
                    high = std::max(high, childHigh);
                    valid = true;
                }
                if (!valid) continue;
                const std::size_t cell = block * pyramid->m_channelCount + channel;
                level.minima[cell] = static_cast<float>(low);
                level.maxima[cell] = static_cast<float>(high);
            }
        }

        additionalCells += cells;
        previousBlockSize = level.blockSize;
        previousBlockCount = level.blockCount;
        pyramid->m_coarseLevels.push_back(std::move(level));
    }

    return pyramid;
}

} // namespace ardirec::desktop
