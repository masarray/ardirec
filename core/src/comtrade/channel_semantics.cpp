// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/channel_semantics.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace ardirec::comtrade {
namespace {

std::string uppercase_compact(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](unsigned char ch) { return !std::isalnum(ch); }),
                value.end());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

std::string normalized_unit(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](unsigned char ch) { return std::isspace(ch) != 0; }),
                value.end());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

bool contains(const std::string& value, const char* token) {
    return value.find(token) != std::string::npos;
}

bool ends_with(const std::string& value, const char* suffix) {
    const std::string token{suffix};
    return value.size() >= token.size()
           && value.compare(value.size() - token.size(), token.size(), token) == 0;
}

PhaseRole phase_from_text(const std::string& raw) {
    const std::string value = uppercase_compact(raw);
    if (value.empty()) return PhaseRole::Other;

    if (value == "A" || value == "1" || value == "L1"
        || contains(value, "L1") || ends_with(value, "AN")
        || ends_with(value, "IA") || ends_with(value, "VA") || ends_with(value, "UA")) {
        return PhaseRole::L1;
    }
    if (value == "B" || value == "2" || value == "L2"
        || contains(value, "L2") || ends_with(value, "BN")
        || ends_with(value, "IB") || ends_with(value, "VB") || ends_with(value, "UB")) {
        return PhaseRole::L2;
    }
    if (value == "C" || value == "3" || value == "L3"
        || contains(value, "L3") || ends_with(value, "CN")
        || ends_with(value, "IC") || ends_with(value, "VC") || ends_with(value, "UC")) {
        return PhaseRole::L3;
    }
    if (value == "N" || value == "E" || value == "0"
        || contains(value, "3I0") || contains(value, "3V0") || contains(value, "3U0")
        || contains(value, "RES") || contains(value, "NEUTRAL")
        || contains(value, "GROUND") || contains(value, "EARTH")
        || ends_with(value, "IN") || ends_with(value, "VN") || ends_with(value, "UN")
        || ends_with(value, "IE") || ends_with(value, "VE") || ends_with(value, "UE")) {
        return PhaseRole::Neutral;
    }
    return PhaseRole::Other;
}

} // namespace

AnalogRole analog_role(const AnalogChannel& channel) noexcept {
    const std::string unit = normalized_unit(channel.units);
    const std::string name = uppercase_compact(channel.id);

    if (unit == "V" || unit == "KV" || unit == "MV" || contains(unit, "VOLT")) {
        return AnalogRole::Voltage;
    }
    if (unit == "A" || unit == "KA" || unit == "MA" || contains(unit, "AMP")) {
        return AnalogRole::Current;
    }

    if (!name.empty()
        && (name.front() == 'V' || name.front() == 'U'
            || contains(name, "UL1") || contains(name, "UL2") || contains(name, "UL3"))) {
        return AnalogRole::Voltage;
    }
    if (!name.empty()
        && (name.front() == 'I'
            || contains(name, "IL1") || contains(name, "IL2") || contains(name, "IL3"))) {
        return AnalogRole::Current;
    }
    return AnalogRole::Other;
}

PhaseRole phase_role(const AnalogChannel& channel) noexcept {
    const PhaseRole explicit_phase = phase_from_text(channel.phase);
    if (explicit_phase != PhaseRole::Other) return explicit_phase;
    return phase_from_text(channel.id);
}

} // namespace ardirec::comtrade
