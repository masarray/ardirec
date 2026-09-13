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

bool finite_complex(const std::complex<double>& value) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}
} // namespace

DistanceImpedance distance_impedance(FaultLoop loop,
                                     const ThreePhasePhasors& phasors,
                                     std::complex<double> grounding_factor_kl,
                                     double minimum_current,
                                     std::optional<std::complex<double>> residual_current) {
    DistanceImpedance result;
    if (!std::isfinite(minimum_current) || minimum_current <= 0.0) minimum_current = 1.0e-9;

    std::complex<double> voltage;
    std::complex<double> measuring_current;
    const auto [first, second] = phase_pair(loop);

    if (is_earth_loop(loop)) {
        const std::complex<double> residual = residual_current.value_or(
            phasors.current[0] + phasors.current[1] + phasors.current[2]);
        voltage = phasors.voltage[static_cast<std::size_t>(first)];
        measuring_current = phasors.current[static_cast<std::size_t>(first)] + grounding_factor_kl * residual;
    } else {
        voltage = phasors.voltage[static_cast<std::size_t>(first)]
                  - phasors.voltage[static_cast<std::size_t>(second)];
        measuring_current = phasors.current[static_cast<std::size_t>(first)]
                            - phasors.current[static_cast<std::size_t>(second)];
    }

    if (!finite_complex(voltage) || !finite_complex(measuring_current)
        || std::abs(measuring_current) <= minimum_current) {
        return result;
    }

    const std::complex<double> impedance = voltage / measuring_current;
    if (!finite_complex(impedance)) return result;

    result.valid = true;
    result.impedance = impedance;
    result.measuring_current = measuring_current;
    return result;
}

DistanceImpedance distance_impedance_rerl_xexl(FaultLoop loop,
                                                const ThreePhasePhasors& phasors,
                                                std::complex<double> earth_current_ie,
                                                double re_over_rl,
                                                double xe_over_xl,
                                                double minimum_current) {
    if (!is_earth_loop(loop)) {
        return distance_impedance(loop, phasors, {}, minimum_current);
    }

    DistanceImpedance result;
    if (!std::isfinite(minimum_current) || minimum_current <= 0.0) minimum_current = 1.0e-9;
    if (!std::isfinite(re_over_rl) || !std::isfinite(xe_over_xl)
        || !finite_complex(earth_current_ie)) {
        return result;
    }

    const auto [first, second] = phase_pair(loop);
    (void)second;
    const std::complex<double> voltage = phasors.voltage[static_cast<std::size_t>(first)];
    const std::complex<double> phase_current = phasors.current[static_cast<std::size_t>(first)];
    if (!finite_complex(voltage) || !finite_complex(phase_current)) return result;

    // Siemens/SIGRA classical earth-loop equation:
    // U = (Ip - kr*IE) R + j (Ip - kx*IE) X.
    // R and X are real unknowns, so the two complex coefficients form a 2x2
    // real system. Keeping kr and kx separate is essential; collapsing them to a
    // complex kL requires a line angle and changes the result when kr != kx.
    const std::complex<double> resistance_current = phase_current - re_over_rl * earth_current_ie;
    const std::complex<double> reactance_current = phase_current - xe_over_xl * earth_current_ie;
    if (!finite_complex(resistance_current) || !finite_complex(reactance_current)) return result;

    const double determinant = resistance_current.real() * reactance_current.real()
                               + resistance_current.imag() * reactance_current.imag();
    const double conditioning_current = std::sqrt(std::abs(determinant));
    if (!std::isfinite(determinant) || !std::isfinite(conditioning_current)
        || conditioning_current <= minimum_current) {
        return result;
    }

    const double resistance = (voltage.real() * reactance_current.real()
                               + reactance_current.imag() * voltage.imag()) / determinant;
    const double reactance = (resistance_current.real() * voltage.imag()
                              - resistance_current.imag() * voltage.real()) / determinant;
    if (!std::isfinite(resistance) || !std::isfinite(reactance)) return result;

    result.valid = true;
    result.impedance = {resistance, reactance};
    // Classical kr/kx compensation has no single complex measuring current.
    // Preserve the actual phase current for diagnostics/current-floor context.
    result.measuring_current = phase_current;
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
