// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ardirec::desktop {

// One-cycle RMS uses a fixed number of sample slots. At the left edge of a finite
// record, slots that precede the first available COMTRADE sample are intentionally
// zero-filled by keeping normalizationSamples at the full-cycle count. Once a full
// cycle is available, the window is half-open (t - T, t], so an exact endpoint does
// not accidentally produce N+1 samples and a visible 2*f ripple.
struct RmsCycleWindow final {
    std::size_t first{0};
    std::size_t end{0};
    std::size_t normalizationSamples{0};

    [[nodiscard]] bool valid() const noexcept {
        return first < end && normalizationSamples > 0;
    }

    [[nodiscard]] std::size_t presentSamples() const noexcept {
        return end >= first ? end - first : 0;
    }
};

[[nodiscard]] inline double local_sample_interval(const std::vector<double>& times,
                                                  std::size_t sampleIndex) noexcept {
    if (times.size() < 2 || sampleIndex >= times.size()) return 0.0;

    // A small fixed-size neighborhood is robust to timestamp quantization without
    // allocating in the RMS render hot path. It also follows COMTRADE sample-rate
    // changes locally instead of assuming one rate for the whole record.
    std::array<double, 8> deltas{};
    std::size_t count = 0;
    const std::size_t firstPair = sampleIndex > 4 ? sampleIndex - 4 : 0;
    const std::size_t lastPair = std::min(times.size() - 1, sampleIndex + 4);
    for (std::size_t i = firstPair; i < lastPair && count < deltas.size(); ++i) {
        const double dt = times[i + 1] - times[i];
        if (std::isfinite(dt) && dt > 0.0) deltas[count++] = dt;
    }
    if (count == 0) return 0.0;

    std::sort(deltas.begin(), deltas.begin() + static_cast<std::ptrdiff_t>(count));
    const std::size_t middle = count / 2;
    return count % 2 == 0 ? 0.5 * (deltas[middle - 1] + deltas[middle]) : deltas[middle];
}

[[nodiscard]] inline std::size_t cycle_sample_count(const std::vector<double>& times,
                                                    std::size_t sampleIndex,
                                                    double nominalFrequency) noexcept {
    if (times.empty() || sampleIndex >= times.size() || !std::isfinite(nominalFrequency)
        || nominalFrequency <= 1.0) {
        return 0;
    }
    const double dt = local_sample_interval(times, sampleIndex);
    if (!(dt > 0.0) || !std::isfinite(dt)) return 0;

    const double samples = (1.0 / nominalFrequency) / dt;
    if (!std::isfinite(samples) || samples < 1.0) return 1;
    constexpr double kMaxReasonableCycleSamples = 1.0e7;
    return static_cast<std::size_t>(std::llround(std::min(samples, kMaxReasonableCycleSamples)));
}

[[nodiscard]] inline RmsCycleWindow rms_cycle_window_for_sample(const std::vector<double>& times,
                                                                std::size_t sampleIndex,
                                                                double nominalFrequency) noexcept {
    RmsCycleWindow result;
    if (times.empty() || sampleIndex >= times.size() || !std::isfinite(nominalFrequency)
        || nominalFrequency <= 1.0) {
        return result;
    }

    const std::size_t normalization = cycle_sample_count(times, sampleIndex, nominalFrequency);
    if (normalization == 0) return result;

    const std::size_t end = sampleIndex + 1;
    const double period = 1.0 / nominalFrequency;
    const double startExclusive = times[sampleIndex] - period;

    // Fast path for regular sampling: the expected N-sample candidate is already
    // the exact half-open time window. Fall back to binary search only around rate
    // changes or irregular timestamps.
    std::size_t first = end > normalization ? end - normalization : 0;
    const bool candidateInside = first < end && times[first] > startExclusive;
    const bool predecessorOutside = first == 0 || times[first - 1] <= startExclusive;
    if (!candidateInside || !predecessorOutside) {
        const auto endIt = times.begin() + static_cast<std::ptrdiff_t>(end);
        first = static_cast<std::size_t>(std::distance(
            times.begin(), std::upper_bound(times.begin(), endIt, startExclusive)));
    }

    // Never allow timestamp irregularities to turn a nominal one-cycle RMS into an
    // N+1 (or larger) average. Keep the newest full-cycle sample slots.
    if (end - first > normalization) first = end - normalization;

    result.first = first;
    result.end = end;
    result.normalizationSamples = normalization;
    return result;
}

[[nodiscard]] inline RmsCycleWindow rms_cycle_window_at(const std::vector<double>& times,
                                                        double absoluteTimeSeconds,
                                                        double nominalFrequency) noexcept {
    RmsCycleWindow result;
    if (times.empty() || !std::isfinite(absoluteTimeSeconds) || !std::isfinite(nominalFrequency)
        || nominalFrequency <= 1.0) {
        return result;
    }

    const double clampedTime = std::clamp(absoluteTimeSeconds, times.front(), times.back());
    const auto endIt = std::upper_bound(times.begin(), times.end(), clampedTime);
    if (endIt == times.begin()) return result;
    const std::size_t sampleIndex = static_cast<std::size_t>(std::distance(times.begin(), endIt) - 1);
    return rms_cycle_window_for_sample(times, sampleIndex, nominalFrequency);
}

} // namespace ardirec::desktop
