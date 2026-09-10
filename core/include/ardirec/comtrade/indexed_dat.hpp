// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/record.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ardirec::comtrade {

// Compact visual level-of-detail cache built during the single DAT index scan.
// Extrema are stored as floats because this cache drives screen geometry only;
// engineering calculations continue to read original double-precision samples.
// Layout is block-major so each index-scan frame updates adjacent channel cells.
struct AnalogLodIndex {
    std::size_t block_size{0};
    std::size_t block_count{0};
    std::size_t channel_count{0};
    std::vector<float> minima;
    std::vector<float> maxima;

    [[nodiscard]] bool blockExtrema(std::size_t channel,
                                    std::size_t block,
                                    double& minimum,
                                    double& maximum) const noexcept;
};

struct DatIndexSummary {
    std::vector<double> time_seconds;
    std::vector<double> analog_abs_peaks;
    std::vector<std::uint8_t> status_active;
    std::vector<double> digital_edge_times;
    std::vector<std::string> diagnostics;
    std::shared_ptr<const AnalogLodIndex> analog_lod;
    bool cancelled{false};
};

// Immutable, random-access COMTRADE DAT source for the desktop viewer.
//
// Binary-family DAT files are accessed through a read-only memory mapping when
// the platform permits it. The operating system pages data on demand, so file
// size does not translate into an equivalent heap allocation. If mapping is
// unavailable, reads fall back to bounded streaming/seek operations.
//
// ASCII DAT files keep only a compact row-offset index and parse channel values
// lazily. No per-frame SampleFrame or eager per-channel sample arrays are kept.
class IndexedDatFile final {
public:
    struct OpenResult {
        std::shared_ptr<IndexedDatFile> file;
        std::vector<std::string> diagnostics;
    };

    [[nodiscard]] static OpenResult open(const RecordConfig& config,
                                         const std::filesystem::path& dat_path);

    ~IndexedDatFile();
    IndexedDatFile(const IndexedDatFile&) = delete;
    IndexedDatFile& operator=(const IndexedDatFile&) = delete;
    IndexedDatFile(IndexedDatFile&&) noexcept;
    IndexedDatFile& operator=(IndexedDatFile&&) noexcept;

    [[nodiscard]] std::size_t frameCount() const noexcept;
    [[nodiscard]] std::size_t analogCount() const noexcept;
    [[nodiscard]] std::size_t statusCount() const noexcept;
    [[nodiscard]] bool memoryMapped() const noexcept;
    [[nodiscard]] bool binaryFamily() const noexcept;
    [[nodiscard]] DataFormat dataFormat() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

    [[nodiscard]] std::uint32_t sampleNumber(std::size_t frame) const noexcept;
    [[nodiscard]] std::uint32_t rawTimestamp(std::size_t frame) const noexcept;
    [[nodiscard]] double analogValue(std::size_t frame,
                                     std::size_t channel) const noexcept;

    // Tri-state field access for damaged ASCII rows. Binary complete frames are
    // always known; malformed/missing ASCII status fields return nullopt.
    [[nodiscard]] std::optional<bool> statusState(std::size_t frame,
                                                  std::size_t channel) const noexcept;

    // Compatibility/safe-display accessor. Unknown status values fall back to
    // the CFG normal state rather than inventing an active transition.
    [[nodiscard]] bool statusValue(std::size_t frame,
                                   std::size_t channel) const noexcept;

    // Small-window helper for phasor/harmonic/table calculations. Reuses the
    // caller-owned vector capacity and never materializes an entire record
    // unless the caller explicitly asks for that range.
    void copyAnalogRange(std::size_t channel,
                         std::size_t first,
                         std::size_t end,
                         std::vector<double>& destination) const;

    // Builds compact metadata and a bounded visual LOD cache in one background
    // pass: timestamps, channel peaks, digital activity/edges and analog block
    // extrema. Invalid fields are diagnosed without aborting the valid record.
    [[nodiscard]] DatIndexSummary buildIndex(double timestamp_scale_seconds,
                                             const std::atomic_bool* cancel = nullptr) const;

private:
    struct Impl;
    explicit IndexedDatFile(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};

} // namespace ardirec::comtrade
