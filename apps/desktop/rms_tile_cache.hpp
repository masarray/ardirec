// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/indexed_dat.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ardirec::desktop {

// Bounded derived-data cache for one-cycle RMS. Tiles are resolution-aware:
// level N stores values sampled every 2^N source frames. Values remain in the
// recorded representation; UI primary/secondary scaling is applied afterwards.
class RmsTileCache final {
public:
    static constexpr std::size_t kTilePoints = 256u;

    struct Tile final {
        std::size_t firstSample{0};
        std::size_t stride{1};
        std::vector<float> values;
    };

    RmsTileCache(std::shared_ptr<const ardirec::comtrade::IndexedDatFile> data,
                 std::shared_ptr<const std::vector<double>> times,
                 double nominalFrequency);

    [[nodiscard]] static std::size_t levelFor(std::size_t visibleSamples,
                                              std::size_t targetPoints) noexcept;
    [[nodiscard]] static std::size_t strideForLevel(std::size_t level) noexcept;

    [[nodiscard]] std::shared_ptr<const Tile> tile(std::size_t channel,
                                                   std::size_t level,
                                                   std::size_t tileIndex,
                                                   const std::atomic_bool* cancel = nullptr);

    void clear();

private:
    struct Key final {
        std::size_t channel{0};
        std::size_t level{0};
        std::size_t tileIndex{0};
        bool operator==(const Key&) const noexcept = default;
    };
    struct KeyHash final {
        std::size_t operator()(const Key& key) const noexcept;
    };
    struct Entry final {
        std::shared_ptr<const Tile> tile;
        std::uint64_t touch{0};
    };

    [[nodiscard]] std::shared_ptr<const Tile> buildTile(const Key& key,
                                                        const std::atomic_bool* cancel) const;
    void trimLocked();

    std::shared_ptr<const ardirec::comtrade::IndexedDatFile> m_data;
    std::shared_ptr<const std::vector<double>> m_times;
    double m_nominalFrequency{50.0};
    std::mutex m_mutex;
    std::unordered_map<Key, Entry, KeyHash> m_cache;
    std::uint64_t m_touchCounter{0};
    std::size_t m_maxTiles{768u};
};

} // namespace ardirec::desktop
