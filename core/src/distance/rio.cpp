// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/distance/rio.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>

namespace ardirec::distance {
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kGeometryEpsilon = 1.0e-9;

struct LineElement {
    RxPoint point;
    double angle_degrees{};
    bool inside_left{true};
};

struct ArcElement {
    RxPoint center;
    double radius{};
    double start_angle{};
    double end_angle{360.0};
    bool inside_left{true};
};

struct PendingShape {
    std::vector<LineElement> lines;
    std::vector<ArcElement> arcs;
    bool invert{};
    bool mho{};
    bool lens_tomato{};
    double mho_angle{};
    double mho_reach{};
    double mho_offset{};
};

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string compact(std::string_view value) {
    std::string result;
    for (const char ch : value) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) == 0) continue;
        result.push_back(static_cast<char>(std::toupper(uch)));
    }
    return result;
}

std::string strip_quotes(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"')
                             || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::pair<std::string, std::string> keyword_value(const std::string& line) {
    const auto separator = line.find_first_of(" \t");
    if (separator == std::string::npos) return {uppercase(trim(line)), {}};
    return {uppercase(trim(line.substr(0, separator))), trim(line.substr(separator + 1))};
}

std::vector<std::string> csv_values(std::string_view raw) {
    std::vector<std::string> values;
    std::string current;
    bool quoted = false;
    char quote = 0;
    for (const char ch : raw) {
        if ((ch == '"' || ch == '\'') && (!quoted || ch == quote)) {
            if (!quoted) {
                quoted = true;
                quote = ch;
            } else {
                quoted = false;
            }
            current.push_back(ch);
        } else if (ch == ',' && !quoted) {
            values.push_back(strip_quotes(trim(current)));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    values.push_back(strip_quotes(trim(current)));
    while (!values.empty() && values.back().empty()) values.pop_back();
    return values;
}

std::optional<double> parse_double(std::string_view raw) {
    std::string value = trim(std::string(raw));
    if (value.empty()) return std::nullopt;
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || !std::isfinite(parsed)) return std::nullopt;
    return parsed;
}

bool parse_bool(std::string_view raw, bool fallback = false) {
    const std::string value = uppercase(trim(std::string(raw)));
    if (value == "YES" || value == "TRUE" || value == "1" || value == "ON") return true;
    if (value == "NO" || value == "FALSE" || value == "0" || value == "OFF") return false;
    return fallback;
}

std::complex<double> polar(double magnitude, double angle_degrees) {
    const double radians = angle_degrees * kPi / 180.0;
    return std::polar(magnitude, radians);
}

double signed_inside_value(const LineElement& line, const RxPoint& point) {
    const double radians = line.angle_degrees * kPi / 180.0;
    const double dx = std::cos(radians);
    const double dy = std::sin(radians);
    const double rel_r = point.r - line.point.r;
    const double rel_x = point.x - line.point.x;
    const double cross = dx * rel_x - dy * rel_r;
    return line.inside_left ? cross : -cross;
}

RxPoint segment_line_intersection(const RxPoint& a, const RxPoint& b, const LineElement& line) {
    const double fa = signed_inside_value(line, a);
    const double fb = signed_inside_value(line, b);
    const double denominator = fa - fb;
    if (std::abs(denominator) <= kGeometryEpsilon) return a;
    const double t = std::clamp(fa / denominator, 0.0, 1.0);
    return {a.r + (b.r - a.r) * t, a.x + (b.x - a.x) * t};
}

std::vector<RxPoint> clip_half_plane(const std::vector<RxPoint>& input, const LineElement& line) {
    std::vector<RxPoint> output;
    if (input.empty()) return output;
    output.reserve(input.size() + 2);

    RxPoint previous = input.back();
    bool previous_inside = signed_inside_value(line, previous) >= -kGeometryEpsilon;
    for (const RxPoint& current : input) {
        const bool current_inside = signed_inside_value(line, current) >= -kGeometryEpsilon;
        if (current_inside) {
            if (!previous_inside) output.push_back(segment_line_intersection(previous, current, line));
            output.push_back(current);
        } else if (previous_inside) {
            output.push_back(segment_line_intersection(previous, current, line));
        }
        previous = current;
        previous_inside = current_inside;
    }
    return output;
}

void deduplicate_polygon(std::vector<RxPoint>& points) {
    std::vector<RxPoint> unique;
    unique.reserve(points.size());
    for (const auto& point : points) {
        if (!unique.empty()) {
            const double dr = point.r - unique.back().r;
            const double dx = point.x - unique.back().x;
            if (std::hypot(dr, dx) <= 1.0e-7) continue;
        }
        unique.push_back(point);
    }
    if (unique.size() > 2) {
        const double dr = unique.front().r - unique.back().r;
        const double dx = unique.front().x - unique.back().x;
        if (std::hypot(dr, dx) <= 1.0e-7) unique.pop_back();
    }
    points = std::move(unique);
}

bool full_circle(double start_angle, double end_angle) {
    return std::abs(end_angle - start_angle) >= 359.999;
}

ZoneShape finalize_shape(const PendingShape& pending, std::vector<std::string>& diagnostics, const std::string& label) {
    ZoneShape result;
    if (pending.invert) {
        diagnostics.push_back("Zone '" + label + "': INVERT geometry is not rendered in P2.");
        return result;
    }
    if (pending.lens_tomato) {
        diagnostics.push_back("Zone '" + label + "': lens/tomato geometry is not rendered in P2.");
        return result;
    }
    if (pending.mho) {
        const double radius = (pending.mho_reach + pending.mho_offset) * 0.5;
        const double center_distance = (pending.mho_reach - pending.mho_offset) * 0.5;
        if (!std::isfinite(radius) || radius <= 0.0) {
            diagnostics.push_back("Zone '" + label + "': invalid mho reach/offset.");
            return result;
        }
        const auto center = polar(center_distance, pending.mho_angle);
        result.kind = ZoneShapeKind::Circle;
        result.center_r = center.real();
        result.center_x = center.imag();
        result.radius = radius;
        return result;
    }
    if (pending.lines.empty() && pending.arcs.size() == 1 && full_circle(pending.arcs.front().start_angle,
                                                                         pending.arcs.front().end_angle)
        && pending.arcs.front().inside_left) {
        const auto& arc = pending.arcs.front();
        if (arc.radius > 0.0 && std::isfinite(arc.radius)) {
            result.kind = ZoneShapeKind::Circle;
            result.center_r = arc.center.r;
            result.center_x = arc.center.x;
            result.radius = arc.radius;
            return result;
        }
    }
    if (!pending.arcs.empty()) {
        diagnostics.push_back("Zone '" + label + "': partial/mixed ARC geometry is not rendered in P2.");
        return result;
    }
    if (pending.lines.size() < 3) {
        diagnostics.push_back("Zone '" + label + "': open/insufficient LINE geometry is not rendered in P2.");
        return result;
    }

    double reference_extent = 1.0;
    for (const auto& line : pending.lines) {
        reference_extent = std::max({reference_extent, std::abs(line.point.r), std::abs(line.point.x)});
    }
    const double boundary = std::max(1000.0, reference_extent * 100.0 + 100.0);
    std::vector<RxPoint> polygon{{-boundary, -boundary}, {boundary, -boundary},
                                 {boundary, boundary}, {-boundary, boundary}};
    for (const auto& line : pending.lines) {
        polygon = clip_half_plane(polygon, line);
        if (polygon.empty()) break;
    }
    deduplicate_polygon(polygon);
    if (polygon.size() < 3) {
        diagnostics.push_back("Zone '" + label + "': LINE borders do not form a finite area.");
        return result;
    }
    const bool touches_boundary = std::any_of(polygon.begin(), polygon.end(), [boundary](const RxPoint& point) {
        return std::abs(point.r) > boundary * 0.98 || std::abs(point.x) > boundary * 0.98;
    });
    if (touches_boundary) {
        diagnostics.push_back("Zone '" + label + "': open/unbounded LINE shape is not rendered in P2.");
        return result;
    }

    result.kind = ZoneShapeKind::Polygon;
    result.points = std::move(polygon);
    return result;
}

bool parse_line_element(const std::string& keyword, const std::string& raw, LineElement& element) {
    const auto values = csv_values(raw);
    if (values.size() < 3) return false;
    const auto first = parse_double(values[0]);
    const auto second = parse_double(values[1]);
    const auto angle = parse_double(values[2]);
    if (!first || !second || !angle) return false;
    if (keyword == "LINEP") {
        const auto point = polar(*first, *second);
        element.point = {point.real(), point.imag()};
    } else {
        element.point = {*first, *second};
    }
    element.angle_degrees = *angle;
    element.inside_left = values.size() < 4 || uppercase(values[3]) != "RIGHT";
    return true;
}

bool parse_arc_element(const std::string& keyword, const std::string& raw, ArcElement& element) {
    const auto values = csv_values(raw);
    if (values.size() < 5) return false;
    const auto first = parse_double(values[0]);
    const auto second = parse_double(values[1]);
    const auto radius = parse_double(values[2]);
    const auto start = parse_double(values[3]);
    const auto end = parse_double(values[4]);
    if (!first || !second || !radius || !start || !end) return false;
    if (keyword == "ARCP") {
        const auto center = polar(*first, *second);
        element.center = {center.real(), center.imag()};
    } else {
        element.center = {*first, *second};
    }
    element.radius = *radius;
    element.start_angle = *start;
    element.end_angle = *end;
    const std::string inside = values.size() >= 7 ? uppercase(values[6])
                               : (values.size() >= 6 && (uppercase(values[5]) == "LEFT" || uppercase(values[5]) == "RIGHT")
                                      ? uppercase(values[5]) : "LEFT");
    element.inside_left = inside != "RIGHT";
    return true;
}

std::string normalized_zone_loop(std::string_view value) {
    std::string normalized = compact(value);
    if (normalized == "L1E") normalized = "L1N";
    if (normalized == "L2E") normalized = "L2N";
    if (normalized == "L3E") normalized = "L3N";
    return normalized;
}
} // namespace

bool RioDistanceModel::impedance_base_conversion_valid() const {
    return voltage_nominal_secondary && voltage_primary && current_nominal_secondary && current_primary
           && *voltage_nominal_secondary > 0.0 && *voltage_primary > 0.0
           && *current_nominal_secondary > 0.0 && *current_primary > 0.0;
}

double RioDistanceModel::primary_to_secondary_impedance_scale() const {
    if (!impedance_base_conversion_valid()) return 1.0;
    return (*voltage_nominal_secondary / *voltage_primary)
           * (*current_primary / *current_nominal_secondary);
}

double RioDistanceModel::zone_scale(bool target_primary) const {
    if (impedances_primary == target_primary) return 1.0;
    if (!impedance_base_conversion_valid()) return 1.0;
    const double p2s = primary_to_secondary_impedance_scale();
    if (!std::isfinite(p2s) || p2s <= 0.0) return 1.0;
    return impedances_primary ? p2s : 1.0 / p2s;
}

RioDistanceModel parse_rio(std::string_view text) {
    RioDistanceModel model;
    bool in_device = false;
    bool in_distance = false;
    bool in_zone = false;
    bool in_shape = false;
    bool in_mho = false;
    bool in_lens = false;
    bool saw_distance = false;

    DistanceZone zone;
    PendingShape pending;
    std::optional<std::pair<double, double>> kl_polar;
    std::optional<std::pair<double, double>> z0z1_polar;
    std::optional<std::pair<double, double>> rerl_xexl;

    std::istringstream stream{std::string(text)};
    std::string raw_line;
    while (std::getline(stream, raw_line)) {
        std::string line = trim(raw_line);
        if (line.empty()) continue;
        const auto [keyword, value] = keyword_value(line);
        if (keyword == "REM" || keyword == "#" || keyword == "//") continue;

        if (keyword == "BEGIN") {
            const std::string block = uppercase(strip_quotes(value));
            if (block == "DEVICE") in_device = true;
            else if (block == "DISTANCE") { in_distance = true; saw_distance = true; }
            else if (block == "ZONE" && in_distance) {
                in_zone = true;
                zone = DistanceZone{};
                pending = PendingShape{};
            } else if (block == "SHAPE" && in_zone) in_shape = true;
            else if (block == "MHOSHAPE" && in_zone) { in_mho = true; pending.mho = true; }
            else if (block == "LENSTOMATOSHAPE" && in_zone) { in_lens = true; pending.lens_tomato = true; }
            continue;
        }
        if (keyword == "END") {
            const std::string block = uppercase(strip_quotes(value));
            if (block == "DEVICE") in_device = false;
            else if (block == "DISTANCE") in_distance = false;
            else if (block == "SHAPE") in_shape = false;
            else if (block == "MHOSHAPE") in_mho = false;
            else if (block == "LENSTOMATOSHAPE") in_lens = false;
            else if (block == "ZONE" && in_zone) {
                zone.shape = finalize_shape(pending, model.diagnostics, zone.label.empty() ? std::to_string(zone.index) : zone.label);
                model.zones.push_back(zone);
                in_zone = false;
                in_shape = false;
                in_mho = false;
                in_lens = false;
            }
            continue;
        }

        if (in_device) {
            if (keyword == "NAME" || (keyword == "DEVICE_MODEL" && model.device_name.empty())) {
                model.device_name = strip_quotes(value);
            } else if (keyword == "VNOM") model.voltage_nominal_secondary = parse_double(value);
            else if (keyword == "VPRIM-LL" || keyword == "VPRIM_LL" || keyword == "VPRIM") model.voltage_primary = parse_double(value);
            else if (keyword == "INOM") model.current_nominal_secondary = parse_double(value);
            else if (keyword == "IPRIM") model.current_primary = parse_double(value);
            continue;
        }
        if (!in_distance) continue;

        if (in_zone) {
            if (keyword == "INDEX") {
                if (const auto parsed = parse_double(value)) zone.index = static_cast<int>(*parsed);
            } else if (keyword == "LABEL") zone.label = strip_quotes(value);
            else if (keyword == "TYPE") zone.type = uppercase(strip_quotes(value));
            else if (keyword == "FAULTLOOP") zone.fault_loop = uppercase(strip_quotes(value));
            else if (keyword == "TRIPTIME") {
                if (const auto parsed = parse_double(value)) zone.trip_time_seconds = *parsed;
            } else if (keyword == "ACTIVE") zone.active = parse_bool(value, true);
            else if (keyword == "INVERT" && (in_shape || in_mho || in_lens)) pending.invert = parse_bool(value, false);
            else if (in_mho && keyword == "ANGLE") {
                if (const auto parsed = parse_double(value)) pending.mho_angle = *parsed;
            } else if (in_mho && keyword == "REACH") {
                if (const auto parsed = parse_double(value)) pending.mho_reach = *parsed;
            } else if (in_mho && keyword == "OFFSET") {
                if (const auto parsed = parse_double(value)) pending.mho_offset = *parsed;
            } else if (in_shape && (keyword == "LINE" || keyword == "LINEP")) {
                LineElement element;
                if (parse_line_element(keyword, value, element)) pending.lines.push_back(element);
                else model.diagnostics.push_back("Zone '" + zone.label + "': malformed " + keyword + " element.");
            } else if (in_shape && (keyword == "ARC" || keyword == "ARCP")) {
                ArcElement element;
                if (parse_arc_element(keyword, value, element)) pending.arcs.push_back(element);
                else model.diagnostics.push_back("Zone '" + zone.label + "': malformed " + keyword + " element.");
            }
            continue;
        }

        if (keyword == "LINEANGLE") {
            if (const auto parsed = parse_double(value)) model.line_angle_degrees = *parsed;
        } else if (keyword == "IMPPRIM") {
            model.impedances_primary = parse_bool(value, false);
        } else if (keyword == "KL") {
            const auto values = csv_values(value);
            if (values.size() >= 2) {
                const auto magnitude = parse_double(values[0]);
                const auto angle = parse_double(values[1]);
                if (magnitude && angle) kl_polar = std::pair<double, double>{*magnitude, *angle};
            }
        } else if (keyword == "Z0Z1") {
            const auto values = csv_values(value);
            if (values.size() >= 2) {
                const auto magnitude = parse_double(values[0]);
                const auto angle = parse_double(values[1]);
                if (magnitude && angle) z0z1_polar = std::pair<double, double>{*magnitude, *angle};
            }
        } else if (keyword == "RERL_XEXL") {
            const auto values = csv_values(value);
            if (values.size() >= 2) {
                const auto first = parse_double(values[0]);
                const auto second = parse_double(values[1]);
                if (first && second) rerl_xexl = std::pair<double, double>{*first, *second};
            }
        }
    }

    if (kl_polar) {
        model.grounding_factor = polar(kl_polar->first, kl_polar->second);
        model.grounding_factor_valid = true;
        model.grounding_factor_source = "KL";
    } else if (z0z1_polar) {
        model.grounding_factor = grounding_factor_from_z0z1(polar(z0z1_polar->first, z0z1_polar->second));
        model.grounding_factor_valid = true;
        model.grounding_factor_source = "Z0/Z1";
    } else if (rerl_xexl) {
        model.grounding_factor = grounding_factor_from_rerl_xexl(rerl_xexl->first,
                                                                  rerl_xexl->second,
                                                                  model.line_angle_degrees);
        model.grounding_factor_valid = true;
        model.grounding_factor_source = "RE/RL XE/XL";
    }

    if (!model.impedance_base_conversion_valid()) {
        model.diagnostics.push_back("RIO device CT/VT metadata is incomplete; zone PRI/SEC conversion will remain 1:1.");
    }
    if (!model.grounding_factor_valid) {
        model.diagnostics.push_back("RIO grounding factor is absent; earth-loop kL must be entered manually.");
    }
    if (!saw_distance) model.diagnostics.push_back("No DISTANCE block found in RIO data.");
    model.valid = saw_distance && !model.zones.empty();
    return model;
}

bool zone_matches_loop(const DistanceZone& zone, FaultLoop loop) {
    if (!zone.active) return false;
    const std::string zone_loop = normalized_zone_loop(zone.fault_loop);
    if (zone_loop.empty() || zone_loop == "ALL") return true;

    if (is_earth_loop(loop)) {
        if (zone_loop == "LN") return true;
        if (loop == FaultLoop::L1E) return zone_loop == "L1N";
        if (loop == FaultLoop::L2E) return zone_loop == "L2N";
        if (loop == FaultLoop::L3E) return zone_loop == "L3N";
        return false;
    }

    if (zone_loop == "LL") return true;
    if (loop == FaultLoop::L1L2) return zone_loop == "L1L2";
    if (loop == FaultLoop::L2L3) return zone_loop == "L2L3";
    if (loop == FaultLoop::L3L1) return zone_loop == "L3L1" || zone_loop == "L1L3";
    return false;
}

} // namespace ardirec::distance
