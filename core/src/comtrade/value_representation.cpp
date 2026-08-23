// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/value_representation.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ardirec::comtrade {

ValueRepresentation recorded_representation(const AnalogChannel& channel) noexcept {
    const auto first = std::find_if(channel.primary_secondary.begin(),
                                    channel.primary_secondary.end(),
                                    [](unsigned char value) { return !std::isspace(value); });
    if (first != channel.primary_secondary.end()
        && std::toupper(static_cast<unsigned char>(*first)) == 'P') {
        return ValueRepresentation::Primary;
    }
    return ValueRepresentation::Secondary;
}

bool has_valid_transformer_ratio(const AnalogChannel& channel) noexcept {
    return channel.primary.has_value() && channel.secondary.has_value()
           && std::isfinite(*channel.primary) && std::isfinite(*channel.secondary)
           && *channel.primary > 0.0 && *channel.secondary > 0.0;
}

double representation_scale(const AnalogChannel& channel, ValueRepresentation target) noexcept {
    if (!has_valid_transformer_ratio(channel)) return 1.0;

    const ValueRepresentation recorded = recorded_representation(channel);
    if (recorded == target) return 1.0;

    const double ratio = *channel.primary / *channel.secondary;
    if (!std::isfinite(ratio) || ratio <= 0.0) return 1.0;
    return target == ValueRepresentation::Primary ? ratio : 1.0 / ratio;
}

} // namespace ardirec::comtrade
