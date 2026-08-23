// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/distance/distance.hpp"

#include <complex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ardirec::distance {

struct RxPoint {
    double r{};
    double x{};
};

enum class ZoneShapeKind {
    Unsupported,
    Polygon,
    Circle,
};

struct ZoneShape {
    ZoneShapeKind kind{ZoneShapeKind::Unsupported};
    std::vector<RxPoint> points;
    double center_r{};
    double center_x{};
    double radius{};
};

struct DistanceZone {
    int index{};
    std::string label;
    std::string type;
    std::string fault_loop{"ALL"};
    double trip_time_seconds{};
    bool active{true};
    ZoneShape shape;
};

struct RioDistanceModel {
    bool valid{};
    std::string device_name;
    double line_angle_degrees{75.0};
    bool impedances_primary{};

    std::optional<double> voltage_nominal_secondary;
    std::optional<double> voltage_primary;
    std::optional<double> current_nominal_secondary;
    std::optional<double> current_primary;

    bool grounding_factor_valid{};
    std::complex<double> grounding_factor{};
    std::string grounding_factor_source;

    std::vector<DistanceZone> zones;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool impedance_base_conversion_valid() const;
    [[nodiscard]] double primary_to_secondary_impedance_scale() const;
    [[nodiscard]] double zone_scale(bool target_primary) const;
};

[[nodiscard]] RioDistanceModel parse_rio(std::string_view text);
[[nodiscard]] bool zone_matches_loop(const DistanceZone& zone, FaultLoop loop);

} // namespace ardirec::distance
