// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/record.hpp"

namespace ardirec::comtrade {

enum class AnalogRole : int {
    Other = 0,
    Voltage = 1,
    Current = 2,
};

enum class PhaseRole : int {
    Other = 0,
    L1 = 1,
    L2 = 2,
    L3 = 3,
    Neutral = 4,
};

[[nodiscard]] AnalogRole analog_role(const AnalogChannel& channel) noexcept;
[[nodiscard]] PhaseRole phase_role(const AnalogChannel& channel) noexcept;

} // namespace ardirec::comtrade
