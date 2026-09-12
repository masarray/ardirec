// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <complex>
#include <optional>

namespace ardirec::power {

struct ThreePhasePhasors {
    std::complex<double> l1{};
    std::complex<double> l2{};
    std::complex<double> l3{};
};

struct SequenceComponents {
    std::complex<double> zero{};
    std::complex<double> positive{};
    std::complex<double> negative{};
};

// Fortescue symmetrical components for the L1-L2-L3 phase order.
// Inputs and outputs preserve the caller's complex phasor convention (for ArdIREC,
// fundamental phasors are RMS engineering phasors). Non-finite inputs are rejected.
[[nodiscard]] std::optional<SequenceComponents>
symmetrical_components(const ThreePhasePhasors& phases) noexcept;

// Inverse Fortescue transform for the same L1-L2-L3 convention.
// This is kept beside the forward transform so tests and future calculated-signal
// features can verify lossless phase/sequence reconstruction explicitly.
[[nodiscard]] std::optional<ThreePhasePhasors>
phase_phasors(const SequenceComponents& components) noexcept;

} // namespace ardirec::power
