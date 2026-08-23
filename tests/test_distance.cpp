// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/distance/distance.hpp"
#include "ardirec/distance/rio.hpp"

#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using Complex = std::complex<double>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

void require_complex_near(Complex actual, Complex expected, double tolerance, const char* message) {
    if (!std::isfinite(actual.real()) || !std::isfinite(actual.imag())
        || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main() {
    try {
        using ardirec::distance::FaultLoop;
        using ardirec::distance::ThreePhasePhasors;
        using ardirec::distance::distance_impedance;
        using ardirec::distance::grounding_factor_from_rerl_xexl;
        using ardirec::distance::grounding_factor_from_z0z1;

        ThreePhasePhasors ll{};
        ll.current[0] = {2.0, 1.0};
        ll.current[1] = {0.5, -0.5};
        const Complex targetLl{4.0, 3.0};
        ll.voltage[0] = targetLl * (ll.current[0] - ll.current[1]);
        ll.voltage[1] = {0.0, 0.0};
        const auto llResult = distance_impedance(FaultLoop::L1L2, ll, {});
        require(llResult.valid, "L1-L2 loop is valid");
        require_complex_near(llResult.impedance, targetLl, 1.0e-12, "L1-L2 formula recovers target impedance");

        ThreePhasePhasors ln{};
        ln.current[0] = {1.0, 0.2};
        ln.current[1] = {-0.3, 0.1};
        ln.current[2] = {-0.1, -0.05};
        const Complex kL{0.4, 0.2};
        const Complex residual = ln.current[0] + ln.current[1] + ln.current[2];
        const Complex targetLn{5.0, 7.0};
        ln.voltage[0] = targetLn * (ln.current[0] + kL * residual);
        const auto lnResult = distance_impedance(FaultLoop::L1E, ln, kL);
        require(lnResult.valid, "L1-E compensated loop is valid");
        require_complex_near(lnResult.impedance, targetLn, 1.0e-12, "L1-E compensation recovers target impedance");

        ThreePhasePhasors weak{};
        weak.voltage[0] = {100.0, 0.0};
        const auto weakResult = distance_impedance(FaultLoop::L1E, weak, kL);
        require(!weakResult.valid, "near-zero measuring current is invalid");

        require_complex_near(grounding_factor_from_z0z1({4.0, 3.0}), {1.0, 1.0}, 1.0e-12,
                             "Z0/Z1 conversion to kL");
        require_complex_near(grounding_factor_from_rerl_xexl(2.0, 3.0, 45.0), {2.5, 0.5}, 1.0e-12,
                             "RE/RL XE/XL conversion uses line angle");

        const std::string rio = R"RIO(
BEGIN TESTOBJECT
BEGIN DEVICE
NAME "P2 TEST RELAY"
VNOM 110
VPRIM-LL 110000
INOM 1
IPRIM 500
FNOM 50
END DEVICE
BEGIN DISTANCE
ACTIVE YES
LINEANGLE 75
IMPPRIM NO
Z0Z1 4, 0
BEGIN ZONE
INDEX 1
TYPE TRIPPING
FAULTLOOP LL
LABEL "Z1 LL"
TRIPTIME 0.03
ACTIVE YES
BEGIN SHAPE
ARCP 2, 0, 2, 0, 360, CCW, LEFT
AUTOCLOSE YES
INVERT NO
END SHAPE
END ZONE
BEGIN ZONE
INDEX 1
TYPE TRIPPING
FAULTLOOP LN
LABEL "Z1 LN"
TRIPTIME 0.03
ACTIVE YES
BEGIN SHAPE
LINE 0, 0, 0, LEFT
LINE 4, 0, 90, LEFT
LINE 0, 6, 180, LEFT
LINE 0, 0, -90, LEFT
AUTOCLOSE YES
INVERT NO
END SHAPE
END ZONE
END DISTANCE
END TESTOBJECT
)RIO";

        const auto model = ardirec::distance::parse_rio(rio);
        require(model.valid, "synthetic RIO parses");
        require(model.zones.size() == 2, "RIO zone count");
        require(model.grounding_factor_valid, "RIO grounding factor available");
        require_complex_near(model.grounding_factor, {1.0, 0.0}, 1.0e-12, "RIO Z0Z1 converts to kL");
        require(!model.impedances_primary, "RIO IMPPRIM NO is secondary");
        require(model.impedance_base_conversion_valid(), "RIO device data enables impedance base conversion");
        require_near(model.primary_to_secondary_impedance_scale(), 0.5, 1.0e-12,
                     "primary-to-secondary impedance scale");
        require_near(model.zone_scale(true), 2.0, 1.0e-12,
                     "secondary RIO zones scale to primary display");
        require_near(model.zone_scale(false), 1.0, 1.0e-12,
                     "secondary RIO zones stay native in secondary display");

        const auto& mho = model.zones[0];
        require(mho.shape.kind == ardirec::distance::ZoneShapeKind::Circle, "ARCP full circle becomes circle overlay");
        require_near(mho.shape.center_r, 2.0, 1.0e-12, "mho circle center R");
        require_near(mho.shape.center_x, 0.0, 1.0e-12, "mho circle center X");
        require_near(mho.shape.radius, 2.0, 1.0e-12, "mho circle radius");
        require(ardirec::distance::zone_matches_loop(mho, FaultLoop::L1L2), "LL zone applies to phase-phase loop");
        require(!ardirec::distance::zone_matches_loop(mho, FaultLoop::L1E), "LL zone excluded from earth loop");

        const auto& quad = model.zones[1];
        require(quad.shape.kind == ardirec::distance::ZoneShapeKind::Polygon, "LINE shape becomes polygon overlay");
        require(quad.shape.points.size() == 4, "quadrilateral has four vertices");
        require(ardirec::distance::zone_matches_loop(quad, FaultLoop::L1E), "LN zone applies to L1-E");
        require(ardirec::distance::zone_matches_loop(quad, FaultLoop::L3E), "LN zone applies to all earth loops");
        require(!ardirec::distance::zone_matches_loop(quad, FaultLoop::L2L3), "LN zone excluded from phase-phase loop");

        std::cout << "ardirec distance tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec distance tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
