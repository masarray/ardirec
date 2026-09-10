// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec_bridge.h"

#include "ardirec/distance/distance.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr int32_t kInvalidArgument = -1;
constexpr int32_t kOutOfRange = -2;
constexpr int32_t kInsufficientBuffer = -4;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr uint64_t kScanChunkFrames = 65'536;
constexpr uint32_t kMinimumLocusPoints = 16;
constexpr uint32_t kMaximumLocusPoints = 4'000;

struct PhaseChannels {
    std::array<int32_t, 3> voltage{{-1, -1, -1}};
    std::array<int32_t, 3> current{{-1, -1, -1}};
};

bool valid_representation(int32_t representation) {
    return representation == ARDIREC_VALUE_RECORDED
        || representation == ARDIREC_VALUE_SECONDARY
        || representation == ARDIREC_VALUE_PRIMARY;
}

bool try_loop(int32_t value, ardirec::distance::FaultLoop& out) {
    switch (value) {
    case ARDIREC_DISTANCE_L1_E: out = ardirec::distance::FaultLoop::L1E; return true;
    case ARDIREC_DISTANCE_L2_E: out = ardirec::distance::FaultLoop::L2E; return true;
    case ARDIREC_DISTANCE_L3_E: out = ardirec::distance::FaultLoop::L3E; return true;
    case ARDIREC_DISTANCE_L1_L2: out = ardirec::distance::FaultLoop::L1L2; return true;
    case ARDIREC_DISTANCE_L2_L3: out = ardirec::distance::FaultLoop::L2L3; return true;
    case ARDIREC_DISTANCE_L3_L1: out = ardirec::distance::FaultLoop::L3L1; return true;
    default: return false;
    }
}

int32_t loop_id(ardirec::distance::FaultLoop loop) {
    switch (loop) {
    case ardirec::distance::FaultLoop::L1E: return ARDIREC_DISTANCE_L1_E;
    case ardirec::distance::FaultLoop::L2E: return ARDIREC_DISTANCE_L2_E;
    case ardirec::distance::FaultLoop::L3E: return ARDIREC_DISTANCE_L3_E;
    case ardirec::distance::FaultLoop::L1L2: return ARDIREC_DISTANCE_L1_L2;
    case ardirec::distance::FaultLoop::L2L3: return ARDIREC_DISTANCE_L2_L3;
    case ardirec::distance::FaultLoop::L3L1: return ARDIREC_DISTANCE_L3_L1;
    }
    return ARDIREC_DISTANCE_L1_E;
}

std::string normalized_unit(const char* value) {
    std::string result;
    if (value == nullptr) return result;
    while (*value != '\0') {
        const auto ch = static_cast<unsigned char>(*value++);
        if (std::isspace(ch) != 0) continue;
        result.push_back(static_cast<char>(std::toupper(ch)));
    }
    return result;
}

double unit_scale_to_si(const ardirec_analog_channel_info& info) {
    const auto unit = normalized_unit(info.units);
    if (unit == "KV" || unit == "KA") return 1.0e3;
    if (unit == "MV") return 1.0e6;
    return 1.0;
}

int32_t read_phase_channels(ardirec_record_handle handle, PhaseChannels& channels, ardirec_record_info* out_info = nullptr) {
    ardirec_record_info info{};
    const auto info_result = ardirec_record_get_info(handle, &info);
    if (info_result != 0) return info_result;

    for (uint32_t channel = 0; channel < info.analog_count; ++channel) {
        ardirec_analog_semantics_info semantics{};
        if (ardirec_record_get_analog_semantics(handle, channel, &semantics) != 0)
            continue;
        if (semantics.phase_role < ARDIREC_PHASE_L1 || semantics.phase_role > ARDIREC_PHASE_L3)
            continue;

        const auto phase = static_cast<std::size_t>(semantics.phase_role - ARDIREC_PHASE_L1);
        if (semantics.role == ARDIREC_ANALOG_VOLTAGE && channels.voltage[phase] < 0)
            channels.voltage[phase] = static_cast<int32_t>(channel);
        else if (semantics.role == ARDIREC_ANALOG_CURRENT && channels.current[phase] < 0)
            channels.current[phase] = static_cast<int32_t>(channel);
    }

    if (out_info != nullptr) *out_info = info;
    return 0;
}

bool loop_available(ardirec::distance::FaultLoop loop, const PhaseChannels& channels) {
    const auto has_v = [&](std::size_t phase) { return channels.voltage[phase] >= 0; };
    const auto has_i = [&](std::size_t phase) { return channels.current[phase] >= 0; };
    switch (loop) {
    case ardirec::distance::FaultLoop::L1E: return has_v(0) && has_i(0) && has_i(1) && has_i(2);
    case ardirec::distance::FaultLoop::L2E: return has_v(1) && has_i(0) && has_i(1) && has_i(2);
    case ardirec::distance::FaultLoop::L3E: return has_v(2) && has_i(0) && has_i(1) && has_i(2);
    case ardirec::distance::FaultLoop::L1L2: return has_v(0) && has_v(1) && has_i(0) && has_i(1);
    case ardirec::distance::FaultLoop::L2L3: return has_v(1) && has_v(2) && has_i(1) && has_i(2);
    case ardirec::distance::FaultLoop::L3L1: return has_v(2) && has_v(0) && has_i(2) && has_i(0);
    }
    return false;
}

int32_t channel_phasor(ardirec_record_handle handle,
                       int32_t channel,
                       uint64_t reference_frame,
                       int32_t representation,
                       std::complex<double>& out) {
    if (channel < 0) {
        out = {};
        return 0;
    }

    ardirec_phasor_info phasor{};
    const auto result = ardirec_record_get_phasor(
        handle, static_cast<uint32_t>(channel), reference_frame, &phasor);
    if (result != 0) return result;
    if (phasor.valid == 0) {
        out = {};
        return 0;
    }

    double representation_scale = 1.0;
    const auto scale_result = ardirec_record_get_representation_scale(
        handle, static_cast<uint32_t>(channel), representation, &representation_scale);
    if (scale_result != 0) return scale_result;

    ardirec_analog_channel_info metadata{};
    const auto metadata_result = ardirec_record_get_analog_channel(
        handle, static_cast<uint32_t>(channel), &metadata);
    if (metadata_result != 0) return metadata_result;

    const double scale = representation_scale * unit_scale_to_si(metadata);
    out = std::complex<double>(phasor.real * scale, phasor.imag * scale);
    return 0;
}

int32_t read_all_phasors(ardirec_record_handle handle,
                         const PhaseChannels& channels,
                         uint64_t reference_frame,
                         int32_t representation,
                         ardirec::distance::ThreePhasePhasors& phasors) {
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const auto v = channel_phasor(handle, channels.voltage[phase], reference_frame, representation, phasors.voltage[phase]);
        if (v != 0) return v;
        const auto i = channel_phasor(handle, channels.current[phase], reference_frame, representation, phasors.current[phase]);
        if (i != 0) return i;
    }
    return 0;
}

int32_t frame_timestamp(ardirec_record_handle handle,
                        const ardirec_record_info& info,
                        uint64_t frame,
                        uint32_t& raw_timestamp,
                        double& time_seconds) {
    const auto result = ardirec_record_copy_raw_timestamps(handle, frame, 1, &raw_timestamp);
    if (result != 0) return result;
    time_seconds = static_cast<double>(raw_timestamp) * info.time_multiplier * 1.0e-6;
    return 0;
}

void initialize_point(ardirec_distance_point& point,
                      ardirec::distance::FaultLoop loop,
                      uint64_t frame,
                      uint32_t raw_timestamp,
                      double time_seconds,
                      double minimum_current) {
    std::memset(&point, 0, sizeof(point));
    point.loop = loop_id(loop);
    point.reference_frame = frame;
    point.raw_timestamp = raw_timestamp;
    point.time_seconds = time_seconds;
    point.minimum_current = minimum_current;
}

void apply_distance_result(ardirec_distance_point& point,
                           const ardirec::distance::DistanceImpedance& result) {
    point.measuring_current = std::abs(result.measuring_current);
    if (!result.valid || !std::isfinite(result.impedance.real()) || !std::isfinite(result.impedance.imag()))
        return;

    point.valid = 1;
    point.r = result.impedance.real();
    point.x = result.impedance.imag();
    point.magnitude = std::abs(result.impedance);
    point.angle_degrees = std::atan2(result.impedance.imag(), result.impedance.real()) * 180.0 / kPi;
}

std::complex<double> grounding_factor(double magnitude, double angle_degrees) {
    const double radians = angle_degrees * kPi / 180.0;
    return std::polar(std::max(0.0, magnitude), radians);
}

std::vector<uint64_t> sampled_frames(uint64_t start_frame, uint64_t frame_count, uint32_t maximum_points) {
    std::vector<uint64_t> frames;
    if (frame_count == 0) return frames;

    maximum_points = std::clamp(maximum_points, kMinimumLocusPoints, kMaximumLocusPoints);
    const auto max_count = static_cast<uint64_t>(maximum_points);
    const uint64_t stride = frame_count <= max_count
        ? 1
        : static_cast<uint64_t>(std::ceil(static_cast<double>(frame_count - 1) /
                                          static_cast<double>(max_count - 1)));

    frames.reserve(static_cast<std::size_t>(std::min<uint64_t>(frame_count, max_count) + 1));
    uint64_t relative = 0;
    while (relative < frame_count) {
        frames.push_back(start_frame + relative);
        if (frame_count - relative <= stride) break;
        relative += stride;
    }

    const uint64_t final_frame = start_frame + frame_count - 1;
    if (frames.empty() || frames.back() != final_frame)
        frames.push_back(final_frame);
    return frames;
}

} // namespace

extern "C" {

int32_t ardirec_record_get_distance_current_floor(
    ardirec_record_handle handle,
    int32_t representation,
    double* out_minimum_current) {
    if (handle == nullptr || out_minimum_current == nullptr || !valid_representation(representation))
        return kInvalidArgument;

    PhaseChannels channels;
    ardirec_record_info info{};
    const auto map_result = read_phase_channels(handle, channels, &info);
    if (map_result != 0) return map_result;

    double peak = 0.0;
    std::vector<double> buffer;
    buffer.resize(static_cast<std::size_t>(std::min<uint64_t>(kScanChunkFrames, std::max<uint64_t>(1, info.frame_count))));

    for (const auto channel : channels.current) {
        if (channel < 0) continue;

        double representation_scale = 1.0;
        const auto scale_result = ardirec_record_get_representation_scale(
            handle, static_cast<uint32_t>(channel), representation, &representation_scale);
        if (scale_result != 0) return scale_result;

        ardirec_analog_channel_info metadata{};
        const auto metadata_result = ardirec_record_get_analog_channel(
            handle, static_cast<uint32_t>(channel), &metadata);
        if (metadata_result != 0) return metadata_result;
        const double scale = representation_scale * unit_scale_to_si(metadata);

        uint64_t start = 0;
        while (start < info.frame_count) {
            const auto count = std::min<uint64_t>(kScanChunkFrames, info.frame_count - start);
            if (buffer.size() < count) buffer.resize(static_cast<std::size_t>(count));
            const auto copy_result = ardirec_record_copy_analog(
                handle, static_cast<uint32_t>(channel), start, count, buffer.data());
            if (copy_result != 0) return copy_result;
            for (uint64_t index = 0; index < count; ++index) {
                const double value = buffer[static_cast<std::size_t>(index)] * scale;
                if (std::isfinite(value)) peak = std::max(peak, std::abs(value));
            }
            start += count;
        }
    }

    *out_minimum_current = std::max(1.0e-6, peak > 0.0 ? peak * 1.0e-3 : 1.0e-6);
    return 0;
}

int32_t ardirec_record_get_distance_loops(
    ardirec_record_handle handle,
    uint64_t reference_frame,
    int32_t representation,
    double grounding_factor_magnitude,
    double grounding_factor_angle_degrees,
    double minimum_current,
    ardirec_distance_point* points,
    uint32_t point_capacity) {
    if (handle == nullptr || points == nullptr || point_capacity < 6 ||
        !valid_representation(representation) ||
        !std::isfinite(grounding_factor_magnitude) || !std::isfinite(grounding_factor_angle_degrees))
        return kInvalidArgument;

    PhaseChannels channels;
    ardirec_record_info info{};
    const auto map_result = read_phase_channels(handle, channels, &info);
    if (map_result != 0) return map_result;
    if (reference_frame >= info.frame_count) return kOutOfRange;

    if (!std::isfinite(minimum_current) || minimum_current <= 0.0) {
        const auto floor_result = ardirec_record_get_distance_current_floor(handle, representation, &minimum_current);
        if (floor_result != 0) return floor_result;
    }

    ardirec::distance::ThreePhasePhasors phasors;
    const auto phasor_result = read_all_phasors(handle, channels, reference_frame, representation, phasors);
    if (phasor_result != 0) return phasor_result;

    uint32_t raw_timestamp = 0;
    double time_seconds = 0.0;
    const auto timestamp_result = frame_timestamp(handle, info, reference_frame, raw_timestamp, time_seconds);
    if (timestamp_result != 0) return timestamp_result;

    const auto kl = grounding_factor(grounding_factor_magnitude, grounding_factor_angle_degrees);
    const std::array<ardirec::distance::FaultLoop, 6> loops{{
        ardirec::distance::FaultLoop::L1E,
        ardirec::distance::FaultLoop::L2E,
        ardirec::distance::FaultLoop::L3E,
        ardirec::distance::FaultLoop::L1L2,
        ardirec::distance::FaultLoop::L2L3,
        ardirec::distance::FaultLoop::L3L1
    }};

    for (std::size_t index = 0; index < loops.size(); ++index) {
        const auto loop = loops[index];
        initialize_point(points[index], loop, reference_frame, raw_timestamp, time_seconds, minimum_current);
        if (!loop_available(loop, channels)) continue;
        apply_distance_result(points[index], ardirec::distance::distance_impedance(loop, phasors, kl, minimum_current));
    }
    return 0;
}

int32_t ardirec_record_get_distance_locus(
    ardirec_record_handle handle,
    int32_t loop_value,
    uint64_t start_frame,
    uint64_t frame_count,
    uint32_t maximum_points,
    int32_t representation,
    double grounding_factor_magnitude,
    double grounding_factor_angle_degrees,
    double minimum_current,
    ardirec_distance_point* points,
    uint32_t point_capacity,
    uint32_t* out_point_count) {
    if (handle == nullptr || out_point_count == nullptr || !valid_representation(representation) ||
        !std::isfinite(grounding_factor_magnitude) || !std::isfinite(grounding_factor_angle_degrees))
        return kInvalidArgument;

    ardirec::distance::FaultLoop loop;
    if (!try_loop(loop_value, loop)) return kInvalidArgument;

    PhaseChannels channels;
    ardirec_record_info info{};
    const auto map_result = read_phase_channels(handle, channels, &info);
    if (map_result != 0) return map_result;
    if (start_frame > info.frame_count || frame_count > info.frame_count - start_frame)
        return kOutOfRange;

    const auto frames = sampled_frames(start_frame, frame_count, maximum_points);
    if (frames.size() > std::numeric_limits<uint32_t>::max()) return kOutOfRange;
    *out_point_count = static_cast<uint32_t>(frames.size());
    if (points == nullptr || point_capacity == 0)
        return 0;
    if (point_capacity < *out_point_count)
        return kInsufficientBuffer;

    if (!std::isfinite(minimum_current) || minimum_current <= 0.0) {
        const auto floor_result = ardirec_record_get_distance_current_floor(handle, representation, &minimum_current);
        if (floor_result != 0) return floor_result;
    }

    const auto kl = grounding_factor(grounding_factor_magnitude, grounding_factor_angle_degrees);
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const auto frame = frames[index];
        uint32_t raw_timestamp = 0;
        double time_seconds = 0.0;
        const auto timestamp_result = frame_timestamp(handle, info, frame, raw_timestamp, time_seconds);
        if (timestamp_result != 0) return timestamp_result;

        initialize_point(points[index], loop, frame, raw_timestamp, time_seconds, minimum_current);
        if (!loop_available(loop, channels)) continue;

        ardirec::distance::ThreePhasePhasors phasors;
        const auto phasor_result = read_all_phasors(handle, channels, frame, representation, phasors);
        if (phasor_result != 0) return phasor_result;
        apply_distance_result(points[index], ardirec::distance::distance_impedance(loop, phasors, kl, minimum_current));
    }
    return 0;
}

} // extern "C"
