// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/record.hpp"

namespace ardirec::comtrade {

enum class ValueRepresentation { Primary, Secondary };

[[nodiscard]] ValueRepresentation recorded_representation(const AnalogChannel& channel) noexcept;
[[nodiscard]] bool has_valid_transformer_ratio(const AnalogChannel& channel) noexcept;
[[nodiscard]] double representation_scale(const AnalogChannel& channel,
                                          ValueRepresentation target) noexcept;

} // namespace ardirec::comtrade
