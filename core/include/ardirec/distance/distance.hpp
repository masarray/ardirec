// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <complex>
#include <optional>
#include <string_view>

namespace ardirec::distance {

enum class FaultLoop {
    L1E,
    L2E,
    L3E,
    L1L2,
    L2L3,
    L3L1,
};

struct ThreePhasePhasors {
    std::array<std::complex<double>, 3> voltage{};
    std::array<std::complex<double>, 3> current{};
};

struct DistanceImpedance {
    bool valid{};
    std::complex<double> impedance{};
    std::complex<double> measuring_current{};
};

// residual_current, when supplied, is 3I0 = IL1 + IL2 + IL3 in the phase-current
// reference direction. This lets callers use a dedicated measured residual/earth
// current channel without changing the protection-loop convention. If omitted,
// the residual is reconstructed from the three phase currents.
[[nodiscard]] DistanceImpedance distance_impedance(
    FaultLoop loop,
    const ThreePhasePhasors& phasors,
    std::complex<double> grounding_factor_kl = {},
    double minimum_current = 1.0e-9,
    std::optional<std::complex<double>> residual_current = std::nullopt);

[[nodiscard]] std::complex<double> grounding_factor_from_z0z1(std::complex<double> z0_over_z1);
[[nodiscard]] std::complex<double> grounding_factor_from_rerl_xexl(
    double re_over_rl,
    double xe_over_xl,
    double line_angle_degrees);

[[nodiscard]] bool is_earth_loop(FaultLoop loop);
[[nodiscard]] std::string_view fault_loop_id(FaultLoop loop);
[[nodiscard]] FaultLoop fault_loop_from_id(std::string_view value, FaultLoop fallback = FaultLoop::L1E);

} // namespace ardirec::distance
