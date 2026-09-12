// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/power/symmetrical_components.hpp"

#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kSqrt3Over2 = 0.866025403784438646763723170752936183;
constexpr std::complex<double> kA{-0.5, kSqrt3Over2};
constexpr std::complex<double> kA2{-0.5, -kSqrt3Over2};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_complex_near(std::complex<double> actual,
                          std::complex<double> expected,
                          double tolerance,
                          const char* message) {
    if (!std::isfinite(actual.real()) || !std::isfinite(actual.imag())
        || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

std::complex<double> polar_degrees(double magnitude, double angleDegrees) {
    return std::polar(magnitude, angleDegrees * kPi / 180.0);
}
} // namespace

int main() {
    try {
        using ardirec::power::SequenceComponents;
        using ardirec::power::ThreePhasePhasors;
        using ardirec::power::phase_phasors;
        using ardirec::power::symmetrical_components;

        const auto positiveReference = polar_degrees(100.0, 30.0);
        const ThreePhasePhasors positivePhases{
            positiveReference,
            kA2 * positiveReference,
            kA * positiveReference,
        };
        const auto positive = symmetrical_components(positivePhases);
        require(positive.has_value(), "balanced positive sequence is valid");
        require_complex_near(positive->zero, {}, 1.0e-12, "positive sequence has zero V0");
        require_complex_near(positive->positive, positiveReference, 1.0e-12,
                             "positive sequence is recovered in V1");
        require_complex_near(positive->negative, {}, 1.0e-12, "positive sequence has zero V2");

        const auto negativeReference = polar_degrees(45.0, -17.0);
        const ThreePhasePhasors negativePhases{
            negativeReference,
            kA * negativeReference,
            kA2 * negativeReference,
        };
        const auto negative = symmetrical_components(negativePhases);
        require(negative.has_value(), "balanced negative sequence is valid");
        require_complex_near(negative->zero, {}, 1.0e-12, "negative sequence has zero V0");
        require_complex_near(negative->positive, {}, 1.0e-12, "negative sequence has zero V1");
        require_complex_near(negative->negative, negativeReference, 1.0e-12,
                             "negative sequence is recovered in V2");

        const auto zeroReference = polar_degrees(12.0, 73.0);
        const ThreePhasePhasors zeroPhases{zeroReference, zeroReference, zeroReference};
        const auto zero = symmetrical_components(zeroPhases);
        require(zero.has_value(), "balanced zero sequence is valid");
        require_complex_near(zero->zero, zeroReference, 1.0e-12,
                             "equal phase phasors are recovered in V0");
        require_complex_near(zero->positive, {}, 1.0e-12, "zero sequence has zero V1");
        require_complex_near(zero->negative, {}, 1.0e-12, "zero sequence has zero V2");

        const SequenceComponents mixedReference{
            polar_degrees(8.0, 11.0),
            polar_degrees(110.0, 22.0),
            polar_degrees(14.0, -41.0),
        };
        const auto mixedPhases = phase_phasors(mixedReference);
        require(mixedPhases.has_value(), "mixed components reconstruct into finite phase phasors");
        const auto mixed = symmetrical_components(*mixedPhases);
        require(mixed.has_value(), "reconstructed mixed phase phasors are valid");
        require_complex_near(mixed->zero, mixedReference.zero, 1.0e-11,
                             "mixed V0 survives round-trip");
        require_complex_near(mixed->positive, mixedReference.positive, 1.0e-11,
                             "mixed V1 survives round-trip");
        require_complex_near(mixed->negative, mixedReference.negative, 1.0e-11,
                             "mixed V2 survives round-trip");

        const auto roundTripPhases = phase_phasors(*mixed);
        require(roundTripPhases.has_value(), "mixed components reconstruct after extraction");
        require_complex_near(roundTripPhases->l1, mixedPhases->l1, 1.0e-11,
                             "L1 survives phase/sequence round-trip");
        require_complex_near(roundTripPhases->l2, mixedPhases->l2, 1.0e-11,
                             "L2 survives phase/sequence round-trip");
        require_complex_near(roundTripPhases->l3, mixedPhases->l3, 1.0e-11,
                             "L3 survives phase/sequence round-trip");

        const double nan = std::numeric_limits<double>::quiet_NaN();
        require(!symmetrical_components(ThreePhasePhasors{{nan, 0.0}, {}, {}}).has_value(),
                "non-finite phase input is rejected");
        require(!phase_phasors(SequenceComponents{{}, {std::numeric_limits<double>::infinity(), 0.0}, {}})
                     .has_value(),
                "non-finite sequence input is rejected");

        std::cout << "ardirec symmetrical component tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec symmetrical component tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
