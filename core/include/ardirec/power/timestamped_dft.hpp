// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ardirec::power {

// A causal trailing measurement window. `end` is an exclusive sample index and
// never includes a sample later than end_seconds. `first` may include one older
// sample so its midpoint cell can contribute to the exact window start.
struct TimestampWindow final {
    std::size_t first{0};
    std::size_t end{0};
    double start_seconds{0.0};
    double end_seconds{0.0};

    [[nodiscard]] bool valid() const noexcept {
        return end > first && std::isfinite(start_seconds) && std::isfinite(end_seconds)
               && end_seconds > start_seconds;
    }

    [[nodiscard]] double duration() const noexcept {
        return valid() ? end_seconds - start_seconds : 0.0;
    }
};

// Build a one-cycle backward-looking window from the actual COMTRADE timestamp
// index. No global sample interval is inferred, so a window can cross sample-rate
// section boundaries without changing its time span.
inline TimestampWindow trailing_cycle_window(const std::vector<double>& times,
                                              double absolute_time_seconds,
                                              double frequency_hz) noexcept {
    TimestampWindow window;
    if (times.size() < 2 || !std::isfinite(absolute_time_seconds)
        || !std::isfinite(frequency_hz) || frequency_hz <= 1.0) {
        return window;
    }

    const double period = 1.0 / frequency_hz;
    const double finish = std::clamp(absolute_time_seconds, times.front(), times.back());
    const double start = std::max(times.front(), finish - period);
    if (!(finish > start)) return window;

    auto first_it = std::lower_bound(times.begin(), times.end(), start);
    std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), first_it));
    if (first > 0) --first; // Past-only neighbour for the exact start boundary.

    const auto end_it = std::upper_bound(times.begin(), times.end(), finish);
    const std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), end_it));
    if (end <= first) return window;

    window.first = first;
    window.end = std::min(end, times.size());
    window.start_seconds = start;
    window.end_seconds = finish;
    return window;
}

// Causal midpoint-cell quadrature weight for one timestamp. The first/last cells
// are clipped to the exact measurement boundaries. Summed across a complete valid
// window these weights equal the window duration, independent of local sample rate.
inline double timestamp_cell_weight(const std::vector<double>& times,
                                    const TimestampWindow& window,
                                    std::size_t sample) noexcept {
    if (!window.valid() || sample < window.first || sample >= window.end
        || sample >= times.size() || !std::isfinite(times[sample])) {
        return 0.0;
    }

    double left = window.start_seconds;
    if (sample > window.first) {
        const double previous = times[sample - 1];
        if (!std::isfinite(previous)) return 0.0;
        left = 0.5 * (previous + times[sample]);
    }

    double right = window.end_seconds;
    if (sample + 1 < window.end) {
        const double following = times[sample + 1];
        if (!std::isfinite(following)) return 0.0;
        right = 0.5 * (times[sample] + following);
    }

    left = std::max(left, window.start_seconds);
    right = std::min(right, window.end_seconds);
    return right > left ? right - left : 0.0;
}

} // namespace ardirec::power
