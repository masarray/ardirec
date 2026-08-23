// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/distance/distance.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace ardirec::distance {
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

std::string compact(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch)) == 0) continue;
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return result;
}

std::pair<int, int> phase_pair(FaultLoop loop) {
    switch (loop) {
    case FaultLoop::L1L2: return {0, 1};
    case FaultLoop::L2L3: return {1, 2};
    case FaultLoop::L3L1: return {2, 0};
    case FaultLoop::L1E: return {0, -1};
    case FaultLoop::L2E: return {1, -1};
    case FaultLoop::L3E: return {2, -1};
    }
    return {0, -1};
}
} // namespace

DistanceImpedance distance_impedance(FaultLoop loop,
                                     const ThreePhasePhasors& phasors,
                                     std::complex<double> grounding_factor_kl,
                                     double minimum_current) {
    DistanceImpedance result;
    if (!std::isfinite(minimum_current) || minimum_current <= 0.0) minimum_current = 1.0e-9;

    std::complex<double> voltage;
    std::complex<double> measuring_current;
    const auto [first, second] = phase_pair(loop);

    if (is_earth_loop(loop)) {
        const std::complex<double> residual = phasors.current[0] + phasors.current[1] + phasors.current[2];
        voltage = phasors.voltage[static_cast<std::size_t>(first)];
        measuring_current = phasors.current[static_cast<std::size_t>(first)] + grounding_factor_kl * residual;
    } else {
        voltage = phasors.voltage[static_cast<std::size_t>(first)]
                  - phasors.voltage[static_cast<std::size_t>(second)];
        measuring_current = phasors.current[static_cast<std::size_t>(first)]
                            - phasors.current[static_cast<std::size_t>(second)];
    }

    if (!std::isfinite(voltage.real()) || !std::isfinite(voltage.imag())
        || !std::isfinite(measuring_current.real()) || !std::isfinite(measuring_current.imag())
        || std::abs(measuring_current) <= minimum_current) {
        return result;
    }

    const std::complex<double> impedance = voltage / measuring_current;
    if (!std::isfinite(impedance.real()) || !std::isfinite(impedance.imag())) return result;

    result.valid = true;
    result.impedance = impedance;
    result.measuring_current = measuring_current;
    return result;
}

std::complex<double> grounding_factor_from_z0z1(std::complex<double> z0_over_z1) {
    return (z0_over_z1 - std::complex<double>{1.0, 0.0}) / 3.0;
}

std::complex<double> grounding_factor_from_rerl_xexl(double re_over_rl,
                                                      double xe_over_xl,
                                                      double line_angle_degrees) {
    if (!std::isfinite(re_over_rl) || !std::isfinite(xe_over_xl)
        || !std::isfinite(line_angle_degrees)) {
        return {};
    }
    const double radians = line_angle_degrees * kPi / 180.0;
    const std::complex<double> line{std::cos(radians), std::sin(radians)};
    if (std::abs(line) <= 1.0e-12) return {};
    const std::complex<double> earth{re_over_rl * line.real(), xe_over_xl * line.imag()};
    return earth / line;
}

bool is_earth_loop(FaultLoop loop) {
    return loop == FaultLoop::L1E || loop == FaultLoop::L2E || loop == FaultLoop::L3E;
}

std::string_view fault_loop_id(FaultLoop loop) {
    switch (loop) {
    case FaultLoop::L1E: return "L1-E";
    case FaultLoop::L2E: return "L2-E";
    case FaultLoop::L3E: return "L3-E";
    case FaultLoop::L1L2: return "L1-L2";
    case FaultLoop::L2L3: return "L2-L3";
    case FaultLoop::L3L1: return "L3-L1";
    }
    return "L1-E";
}

FaultLoop fault_loop_from_id(std::string_view value, FaultLoop fallback) {
    const std::string normalized = compact(value);
    if (normalized == "L1E" || normalized == "L1N" || normalized == "AN" || normalized == "AE") return FaultLoop::L1E;
    if (normalized == "L2E" || normalized == "L2N" || normalized == "BN" || normalized == "BE") return FaultLoop::L2E;
    if (normalized == "L3E" || normalized == "L3N" || normalized == "CN" || normalized == "CE") return FaultLoop::L3E;
    if (normalized == "L1L2" || normalized == "AB") return FaultLoop::L1L2;
    if (normalized == "L2L3" || normalized == "BC") return FaultLoop::L2L3;
    if (normalized == "L3L1" || normalized == "CA" || normalized == "L1L3") return FaultLoop::L3L1;
    return fallback;
}

} // namespace ardirec::distance
