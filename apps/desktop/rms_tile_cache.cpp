// SPDX-License-Identifier: GPL-3.0-or-later
#include "rms_tile_cache.hpp"

#include "rms_cycle_window.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ardirec::desktop {
namespace {
[[nodiscard]] bool cancelled(const std::atomic_bool* cancel) noexcept {
    return cancel && cancel->load(std::memory_order_relaxed);
}
} // namespace

RmsTileCache::RmsTileCache(std::shared_ptr<const ardirec::comtrade::IndexedDatFile> data,
                           std::shared_ptr<const std::vector<double>> times,
                           double nominalFrequency)
    : m_data(std::move(data)), m_times(std::move(times)) {
    if (std::isfinite(nominalFrequency) && nominalFrequency > 1.0) {
        m_nominalFrequency = nominalFrequency;
    }
}

std::size_t RmsTileCache::strideForLevel(std::size_t level) noexcept {
    constexpr std::size_t bits = sizeof(std::size_t) * 8u;
    if (level >= bits - 1u) return std::size_t{1} << (bits - 2u);
    return std::size_t{1} << level;
}

std::size_t RmsTileCache::levelFor(std::size_t visibleSamples,
                                   std::size_t targetPoints) noexcept {
    targetPoints = std::max<std::size_t>(1u, targetPoints);
    const std::size_t requiredStride = std::max<std::size_t>(
        1u, (visibleSamples + targetPoints - 1u) / targetPoints);
    std::size_t level = 0u;
    std::size_t stride = 1u;
    while (stride < requiredStride && stride <= std::numeric_limits<std::size_t>::max() / 2u) {
        stride *= 2u;
        ++level;
    }
    return level;
}

std::size_t RmsTileCache::KeyHash::operator()(const Key& key) const noexcept {
    std::size_t hash = key.channel + 0x9e3779b97f4a7c15ULL;
    hash ^= key.level + 0x9e3779b97f4a7c15ULL + (hash << 6u) + (hash >> 2u);
    hash ^= key.tileIndex + 0x9e3779b97f4a7c15ULL + (hash << 6u) + (hash >> 2u);
    return hash;
}

std::shared_ptr<const RmsTileCache::Tile>
RmsTileCache::buildTile(const Key& key, const std::atomic_bool* cancel) const {
    if (!m_data || !m_times || m_times->empty() || key.channel >= m_data->analogCount()
        || cancelled(cancel)) {
        return {};
    }

    const std::size_t count = std::min(m_data->frameCount(), m_times->size());
    if (count == 0u) return {};
    const std::size_t stride = strideForLevel(key.level);
    if (stride == 0u || stride > std::numeric_limits<std::size_t>::max() / kTilePoints) return {};
    const std::size_t tileSpan = stride * kTilePoints;
    if (key.tileIndex > std::numeric_limits<std::size_t>::max() / tileSpan) return {};
    const std::size_t firstSample = key.tileIndex * tileSpan;
    if (firstSample >= count) return {};

    auto result = std::make_shared<Tile>();
    result->firstSample = firstSample;
    result->stride = stride;
    result->values.reserve(kTilePoints);

    for (std::size_t point = 0u; point < kTilePoints; ++point) {
        if ((point & 0x0Fu) == 0u && cancelled(cancel)) return {};
        if (point > (std::numeric_limits<std::size_t>::max() - firstSample) / stride) break;
        const std::size_t sampleIndex = firstSample + point * stride;
        if (sampleIndex >= count) break;

        const auto window = rms_cycle_window_for_sample(*m_times, sampleIndex, m_nominalFrequency);
        if (!window.valid()) {
            result->values.push_back(0.0F);
            continue;
        }

        long double sumSquares = 0.0L;
        std::size_t finiteCount = 0u;
        std::size_t checked = 0u;
        for (std::size_t sample = window.first; sample < window.end; ++sample) {
            if ((checked++ & 0xFFu) == 0u && cancelled(cancel)) return {};
            const double value = m_data->analogValue(sample, key.channel);
            if (!std::isfinite(value)) continue;
            sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
            ++finiteCount;
        }

        double rms = 0.0;
        if (finiteCount > 0u) {
            const std::size_t presentCount = window.presentSamples();
            const std::size_t invalidPresent = presentCount > finiteCount
                                                   ? presentCount - finiteCount
                                                   : 0u;
            const std::size_t denominator = window.normalizationSamples > invalidPresent
                                                ? window.normalizationSamples - invalidPresent
                                                : finiteCount;
            if (denominator > 0u) {
                rms = std::sqrt(static_cast<double>(
                    sumSquares / static_cast<long double>(denominator)));
            }
        }
        if (!std::isfinite(rms) || rms < 0.0) rms = 0.0;
        const double limited = std::min(rms, static_cast<double>(std::numeric_limits<float>::max()));
        result->values.push_back(static_cast<float>(limited));
    }

    if (cancelled(cancel) || result->values.empty()) return {};
    return result;
}

std::shared_ptr<const RmsTileCache::Tile>
RmsTileCache::tile(std::size_t channel,
                   std::size_t level,
                   std::size_t tileIndex,
                   const std::atomic_bool* cancel) {
    const Key key{channel, level, tileIndex};
    {
        std::lock_guard lock(m_mutex);
        const auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            it->second.touch = ++m_touchCounter;
            return it->second.tile;
        }
    }

    auto built = buildTile(key, cancel);
    if (!built || cancelled(cancel)) return {};

    std::lock_guard lock(m_mutex);
    const auto [it, inserted] = m_cache.emplace(key, Entry{built, ++m_touchCounter});
    if (!inserted) {
        it->second.touch = ++m_touchCounter;
        return it->second.tile;
    }
    trimLocked();
    return built;
}

void RmsTileCache::trimLocked() {
    while (m_cache.size() > m_maxTiles) {
        auto victim = m_cache.end();
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (victim == m_cache.end() || it->second.touch < victim->second.touch) victim = it;
        }
        if (victim == m_cache.end()) break;
        m_cache.erase(victim);
    }
}

void RmsTileCache::clear() {
    std::lock_guard lock(m_mutex);
    m_cache.clear();
    m_touchCounter = 0u;
}

} // namespace ardirec::desktop
