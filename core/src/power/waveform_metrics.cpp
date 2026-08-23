// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/power/waveform_metrics.hpp"

#include <algorithm>
#include <cmath>

namespace ardirec::power {

std::optional<double> last_extreme_value(std::span<const double> samples,
                                         std::span<const double> times_seconds,
                                         double reference_time_seconds) {
    const std::size_t count = std::min(samples.size(), times_seconds.size());
    if (count == 0 || !std::isfinite(reference_time_seconds)) return std::nullopt;

    const auto endIt = std::upper_bound(times_seconds.begin(),
                                        times_seconds.begin() + static_cast<std::ptrdiff_t>(count),
                                        reference_time_seconds);
    std::size_t end = static_cast<std::size_t>(std::distance(times_seconds.begin(), endIt));
    if (end == 0) return std::nullopt;

    if (end >= 3) {
        for (std::size_t center = end - 2; center > 0; --center) {
            const double left = samples[center - 1];
            const double value = samples[center];
            const double right = samples[center + 1];
            if (!std::isfinite(left) || !std::isfinite(value) || !std::isfinite(right)) continue;

            const double incoming = value - left;
            const double outgoing = right - value;
            const bool localMax = incoming > 0.0 && outgoing <= 0.0;
            const bool localMin = incoming < 0.0 && outgoing >= 0.0;
            if (localMax || localMin) return value;
        }
    }

    for (std::size_t index = end; index > 0; --index) {
        const double value = samples[index - 1];
        if (std::isfinite(value)) return value;
    }
    return std::nullopt;
}

} // namespace ardirec::power
