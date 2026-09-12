// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/power/symmetrical_components.hpp"

#include <cmath>

namespace ardirec::power {
namespace {

constexpr double kSqrt3Over2 = 0.866025403784438646763723170752936183;
constexpr std::complex<double> kA{-0.5, kSqrt3Over2};
constexpr std::complex<double> kA2{-0.5, -kSqrt3Over2};
constexpr double kOneThird = 1.0 / 3.0;

[[nodiscard]] bool finite(const std::complex<double>& value) noexcept {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

} // namespace

std::optional<SequenceComponents>
symmetrical_components(const ThreePhasePhasors& phases) noexcept {
    if (!finite(phases.l1) || !finite(phases.l2) || !finite(phases.l3)) {
        return std::nullopt;
    }

    SequenceComponents result;
    result.zero = (phases.l1 + phases.l2 + phases.l3) * kOneThird;
    result.positive = (phases.l1 + kA * phases.l2 + kA2 * phases.l3) * kOneThird;
    result.negative = (phases.l1 + kA2 * phases.l2 + kA * phases.l3) * kOneThird;

    if (!finite(result.zero) || !finite(result.positive) || !finite(result.negative)) {
        return std::nullopt;
    }
    return result;
}

std::optional<ThreePhasePhasors>
phase_phasors(const SequenceComponents& components) noexcept {
    if (!finite(components.zero) || !finite(components.positive) || !finite(components.negative)) {
        return std::nullopt;
    }

    ThreePhasePhasors result;
    result.l1 = components.zero + components.positive + components.negative;
    result.l2 = components.zero + kA2 * components.positive + kA * components.negative;
    result.l3 = components.zero + kA * components.positive + kA2 * components.negative;

    if (!finite(result.l1) || !finite(result.l2) || !finite(result.l3)) {
        return std::nullopt;
    }
    return result;
}

} // namespace ardirec::power
