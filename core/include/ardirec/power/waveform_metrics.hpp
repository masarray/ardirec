// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <span>

namespace ardirec::power {

// Returns the last local maximum or minimum whose full turning-point triple is
// at or before reference_time_seconds. Falls back to the latest finite sample
// at/before the reference when no turning point exists.
[[nodiscard]] std::optional<double> last_extreme_value(std::span<const double> samples,
                                                       std::span<const double> times_seconds,
                                                       double reference_time_seconds);

} // namespace ardirec::power
