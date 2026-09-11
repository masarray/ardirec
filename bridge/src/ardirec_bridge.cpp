// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec_bridge.h"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/channel_semantics.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"
#include "ardirec/comtrade/value_representation.hpp"
#include "ardirec/distance/distance.hpp"
#include "ardirec/power/harmonics.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct NativeRecord {
    ardirec::comtrade::RecordConfig config;
    std::vector<ardirec::comtrade::SampleFrame> frames;
    std::array<int, 3> voltage_channels{{-1, -1, -1}};
    std::array<int, 3> current_channels{{-1, -1, -1}};
};

constexpr int32_t kInvalidArgument = -1;
constexpr int32_t kOutOfRange = -2;
constexpr int32_t kOpenFailed = -3;
constexpr int32_t kInsufficientBuffer = -4;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr uint32_t kDistanceLoopCount = 6;

void copy_text(char* destination, std::size_t capacity, const std::string& value) {
    if (destination == nullptr || capacity == 0) return;
    const auto count = std::min(capacity - 1, value.size());
    std::memcpy(destination, value.data(), count);
    destination[count] = '\0';
}

std::filesystem::path utf8_path(const char* value) {
    if (value == nullptr || *value == '\0') return {};
#if defined(__cpp_lib_char8_t)
    const auto* begin = reinterpret_cast<const char8_t*>(value);
    const auto* end = begin + std::strlen(value);
    return std::filesystem::path(std::u8string(begin, end));
#else
    return std::filesystem::u8path(value);
#endif
}

NativeRecord* as_record(ardirec_record_handle handle) {
    return static_cast<NativeRecord*>(handle);
}

bool valid_range(const NativeRecord& record, std::uint64_t start, std::uint64_t count) {
    const auto size = static_cast<std::uint64_t>(record.frames.size());
    return start <= size && count <= (size - start);
}

double frame_time_seconds(const NativeRecord& record, std::size_t frame_index) {
    const double time_scale = record.config.time_multiplier * 1.0e-6;
    return static_cast<double>(record.frames[frame_index].raw_timestamp) * time_scale;
}

double wrap_degrees(double angle) {
    while (angle <= -180.0) angle += 360.0;
    while (angle > 180.0) angle -= 360.0;
    return angle;
}

bool representation_scale_for(const ardirec::comtrade::AnalogChannel& channel,
                              int32_t representation,
                              double* out_scale) {
    if (out_scale == nullptr) return false;
    switch (representation) {
    case ARDIREC_VALUE_RECORDED:
        *out_scale = 1.0;
        return true;
    case ARDIREC_VALUE_SECONDARY:
        *out_scale = ardirec::comtrade::representation_scale(
            channel, ardirec::comtrade::ValueRepresentation::Secondary);
        return true;
    case ARDIREC_VALUE_PRIMARY:
        *out_scale = ardirec::comtrade::representation_scale(
            channel, ardirec::comtrade::ValueRepresentation::Primary);
        return true;
    default:
        return false;
    }
}

int32_t recorded_representation_for(const ardirec::comtrade::AnalogChannel& channel) {
    return ardirec::comtrade::recorded_representation(channel)
                   == ardirec::comtrade::ValueRepresentation::Primary
               ? ARDIREC_VALUE_PRIMARY
               : ARDIREC_VALUE_SECONDARY;
}

std::string normalized_unit(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](unsigned char ch) { return std::isspace(ch) != 0; }),
                value.end());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

double unit_scale_to_si(const ardirec::comtrade::AnalogChannel& channel) {
    const auto unit = normalized_unit(channel.units);
    if (unit == "KV" || unit == "KA") return 1.0e3;
    if (unit == "MV") return 1.0e6;
    return 1.0;
}

int phase_slot(ardirec::comtrade::PhaseRole phase) {
    switch (phase) {
    case ardirec::comtrade::PhaseRole::L1: return 0;
    case ardirec::comtrade::PhaseRole::L2: return 1;
    case ardirec::comtrade::PhaseRole::L3: return 2;
    default: return -1;
    }
}

void resolve_distance_channels(NativeRecord& record) {
    record.voltage_channels.fill(-1);
    record.current_channels.fill(-1);
    for (std::size_t index = 0; index < record.config.analog_channels.size(); ++index) {
        const auto& channel = record.config.analog_channels[index];
        const int slot = phase_slot(ardirec::comtrade::phase_role(channel));
        if (slot < 0) continue;
        switch (ardirec::comtrade::analog_role(channel)) {
        case ardirec::comtrade::AnalogRole::Voltage:
            if (record.voltage_channels[static_cast<std::size_t>(slot)] < 0)
                record.voltage_channels[static_cast<std::size_t>(slot)] = static_cast<int>(index);
            break;
        case ardirec::comtrade::AnalogRole::Current:
            if (record.current_channels[static_cast<std::size_t>(slot)] < 0)
                record.current_channels[static_cast<std::size_t>(slot)] = static_cast<int>(index);
            break;
        default:
            break;
        }
    }
}

std::pair<std::size_t, std::size_t> one_cycle_window(const NativeRecord& record,
                                                      std::uint64_t reference_frame) {
    if (record.frames.empty() || reference_frame >= record.frames.size()) return {0, 0};

    const double frequency = record.config.nominal_frequency > 1.0
                                 ? record.config.nominal_frequency
                                 : 50.0;
    const double period = 1.0 / frequency;
    const std::size_t reference = static_cast<std::size_t>(reference_frame);
    const double data_start = frame_time_seconds(record, 0);
    const double end_time = frame_time_seconds(record, reference);
    const double start_time = std::max(data_start, end_time - period);
    const double time_scale = record.config.time_multiplier * 1.0e-6;

    const auto search_end = record.frames.begin() + static_cast<std::ptrdiff_t>(reference + 1);
    const auto first_it = std::lower_bound(
        record.frames.begin(), search_end, start_time,
        [time_scale](const ardirec::comtrade::SampleFrame& frame, double target) {
            return static_cast<double>(frame.raw_timestamp) * time_scale < target;
        });

    std::size_t first = static_cast<std::size_t>(std::distance(record.frames.begin(), first_it));
    const std::size_t end = reference + 1;
    if (end > first + 2
        && frame_time_seconds(record, end - 1) - frame_time_seconds(record, first)
               >= period * (1.0 - 1.0e-8)) {
        ++first;
    }
    if (end <= first) return {0, 0};
    return {first, end};
}

ardirec::power::HarmonicSpectrum analyze_spectrum(const NativeRecord& record,
                                                   std::uint32_t channel_index,
                                                   std::uint64_t reference_frame,
                                                   int maximum_order,
                                                   std::size_t* out_first,
                                                   std::size_t* out_end) {
    const auto [first, end] = one_cycle_window(record, reference_frame);
    if (out_first != nullptr) *out_first = first;
    if (out_end != nullptr) *out_end = end;
    if (first >= end || end - first < 4) return {};

    std::vector<double> samples;
    std::vector<double> times;
    samples.reserve(end - first);
    times.reserve(end - first);
    for (std::size_t i = first; i < end; ++i) {
        samples.push_back(record.frames[i].analog[channel_index]);
        times.push_back(frame_time_seconds(record, i));
    }

    const double frequency = record.config.nominal_frequency > 1.0
                                 ? record.config.nominal_frequency
                                 : 50.0;
    return ardirec::power::harmonic_spectrum(
        std::span<const double>(samples.data(), samples.size()),
        std::span<const double>(times.data(), times.size()),
        frequency,
        maximum_order,
        frame_time_seconds(record, 0));
}

std::complex<double> phasor_complex(const NativeRecord& record,
                                    int channel_index,
                                    std::uint64_t reference_frame,
                                    int32_t representation) {
    if (channel_index < 0
        || static_cast<std::size_t>(channel_index) >= record.config.analog_channels.size()
        || reference_frame >= record.frames.size()) {
        return {};
    }
    const auto [first, end] = one_cycle_window(record, reference_frame);
    if (first >= end || end - first < 4) return {};

    double representation_scale = 1.0;
    if (!representation_scale_for(record.config.analog_channels[static_cast<std::size_t>(channel_index)],
                                  representation, &representation_scale)) {
        return {};
    }

    const double frequency = record.config.nominal_frequency > 1.0
                                 ? record.config.nominal_frequency
                                 : 50.0;
    const double omega = 2.0 * kPi * frequency;
    const double reference_time = frame_time_seconds(record, 0);
    std::complex<long double> accumulator{0.0L, 0.0L};
    std::size_t count = 0;
    for (std::size_t i = first; i < end; ++i) {
        const double value = record.frames[i].analog[static_cast<std::size_t>(channel_index)];
        if (!std::isfinite(value)) continue;
        const long double angle = -static_cast<long double>(omega * (frame_time_seconds(record, i) - reference_time));
        accumulator += static_cast<long double>(value)
                       * std::complex<long double>{std::cos(angle), std::sin(angle)};
        ++count;
    }
    if (count < 4) return {};

    const long double scale = std::sqrt(2.0L) / static_cast<long double>(count)
                              * static_cast<long double>(representation_scale);
    accumulator *= scale;
    return {static_cast<double>(accumulator.real()), static_cast<double>(accumulator.imag())};
}

bool distance_loop_available(const NativeRecord& record, int32_t loop) {
    const auto& v = record.voltage_channels;
    const auto& i = record.current_channels;
    switch (loop) {
    case ARDIREC_DISTANCE_L1_E: return v[0] >= 0 && i[0] >= 0 && i[1] >= 0 && i[2] >= 0;
    case ARDIREC_DISTANCE_L2_E: return v[1] >= 0 && i[0] >= 0 && i[1] >= 0 && i[2] >= 0;
    case ARDIREC_DISTANCE_L3_E: return v[2] >= 0 && i[0] >= 0 && i[1] >= 0 && i[2] >= 0;
    case ARDIREC_DISTANCE_L1_L2: return v[0] >= 0 && v[1] >= 0 && i[0] >= 0 && i[1] >= 0;
    case ARDIREC_DISTANCE_L2_L3: return v[1] >= 0 && v[2] >= 0 && i[1] >= 0 && i[2] >= 0;
    case ARDIREC_DISTANCE_L3_L1: return v[2] >= 0 && v[0] >= 0 && i[2] >= 0 && i[0] >= 0;
    default: return false;
    }
}

ardirec::distance::FaultLoop native_fault_loop(int32_t loop) {
    switch (loop) {
    case ARDIREC_DISTANCE_L2_E: return ardirec::distance::FaultLoop::L2E;
    case ARDIREC_DISTANCE_L3_E: return ardirec::distance::FaultLoop::L3E;
    case ARDIREC_DISTANCE_L1_L2: return ardirec::distance::FaultLoop::L1L2;
    case ARDIREC_DISTANCE_L2_L3: return ardirec::distance::FaultLoop::L2L3;
    case ARDIREC_DISTANCE_L3_L1: return ardirec::distance::FaultLoop::L3L1;
    case ARDIREC_DISTANCE_L1_E:
    default: return ardirec::distance::FaultLoop::L1E;
    }
}

bool fill_distance_phasors(const NativeRecord& record,
                           std::uint64_t reference_frame,
                           int32_t representation,
                           ardirec::distance::ThreePhasePhasors& phasors) {
    if (reference_frame >= record.frames.size()) return false;
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const int voltage_channel = record.voltage_channels[phase];
        const int current_channel = record.current_channels[phase];
        if (voltage_channel >= 0) {
            phasors.voltage[phase] = phasor_complex(record, voltage_channel, reference_frame, representation)
                                     * unit_scale_to_si(record.config.analog_channels[static_cast<std::size_t>(voltage_channel)]);
        }
        if (current_channel >= 0) {
            phasors.current[phase] = phasor_complex(record, current_channel, reference_frame, representation)
                                     * unit_scale_to_si(record.config.analog_channels[static_cast<std::size_t>(current_channel)]);
        }
    }
    return true;
}

void clear_distance_point(ardirec_distance_point& point, int32_t loop, const NativeRecord& record,
                          std::uint64_t reference_frame, double minimum_current) {
    std::memset(&point, 0, sizeof(point));
    point.loop = loop;
    point.reference_frame = reference_frame;
    point.minimum_current = minimum_current;
    if (reference_frame < record.frames.size()) {
        point.raw_timestamp = record.frames[static_cast<std::size_t>(reference_frame)].raw_timestamp;
        point.time_seconds = frame_time_seconds(record, static_cast<std::size_t>(reference_frame));
    }
}

void fill_distance_result(ardirec_distance_point& point,
                          int32_t loop,
                          const NativeRecord& record,
                          std::uint64_t reference_frame,
                          const ardirec::distance::ThreePhasePhasors& phasors,
                          std::complex<double> grounding_factor,
                          double minimum_current) {
    clear_distance_point(point, loop, record, reference_frame, minimum_current);
    if (!distance_loop_available(record, loop)) return;

    const auto result = ardirec::distance::distance_impedance(
        native_fault_loop(loop), phasors, grounding_factor, minimum_current);
    if (!result.valid) return;
    const auto impedance = result.impedance;
    if (!std::isfinite(impedance.real()) || !std::isfinite(impedance.imag())) return;

    point.valid = 1;
    point.r = impedance.real();
    point.x = impedance.imag();
    point.magnitude = std::abs(impedance);
    point.angle_degrees = std::atan2(impedance.imag(), impedance.real()) * 180.0 / kPi;
    point.measuring_current = std::abs(result.measuring_current);
}

std::complex<double> grounding_factor(double magnitude, double angle_degrees) {
    const double radians = angle_degrees * kPi / 180.0;
    return std::polar(std::max(0.0, magnitude), radians);
}

uint32_t normalized_locus_point_count(uint64_t frame_count, uint32_t maximum_points) {
    if (frame_count == 0) return 0;
    const uint32_t bounded = std::clamp<uint32_t>(maximum_points, 16u, 4000u);
    if (frame_count <= bounded) return static_cast<uint32_t>(frame_count);
    const uint64_t stride = (frame_count - 1u + (bounded - 2u)) / (bounded - 1u);
    const uint64_t regular = ((frame_count - 1u) / stride) + 1u;
    const bool final_already = ((regular - 1u) * stride) == (frame_count - 1u);
    const uint64_t count = regular + (final_already ? 0u : 1u);
    return static_cast<uint32_t>(std::min<uint64_t>(count, bounded));
}

} // namespace

extern "C" {

uint32_t ardirec_bridge_abi_version(void) {
    return ARDIREC_BRIDGE_ABI_VERSION;
}

uint64_t ardirec_bridge_capabilities(void) {
    return ARDIREC_BRIDGE_CAP_CURSOR_MEASUREMENT
           | ARDIREC_BRIDGE_CAP_CHANNEL_SEMANTICS
           | ARDIREC_BRIDGE_CAP_VALUE_REPRESENTATION
           | ARDIREC_BRIDGE_CAP_STATUS_STATE
           | ARDIREC_BRIDGE_CAP_DIGITAL_EDGE_SNAP
           | ARDIREC_BRIDGE_CAP_PHASOR
           | ARDIREC_BRIDGE_CAP_HARMONICS
           | ARDIREC_BRIDGE_CAP_DISTANCE_LOCUS;
}

int32_t ardirec_record_open_utf8(
    const char* cfg_path_utf8,
    ardirec_record_handle* out_handle,
    char* error_utf8,
    size_t error_capacity) {
    if (out_handle == nullptr || cfg_path_utf8 == nullptr || *cfg_path_utf8 == '\0') {
        copy_text(error_utf8, error_capacity, "CFG path and output handle are required.");
        return kInvalidArgument;
    }

    *out_handle = nullptr;
    try {
        const auto cfg_path = utf8_path(cfg_path_utf8);
        const auto bundle = ardirec::comtrade::locate_bundle(cfg_path);
        if (bundle.dat.empty()) throw std::runtime_error("Matching DAT file was not found.");

        auto record = std::make_unique<NativeRecord>();
        record->config = ardirec::comtrade::ConfigParser{}.parse_file(bundle.cfg);
        record->frames = ardirec::comtrade::DatReader{}.read(record->config, bundle.dat);
        if (record->frames.empty()) throw std::runtime_error("COMTRADE DAT contains no readable frames.");
        resolve_distance_channels(*record);

        *out_handle = record.release();
        if (error_utf8 != nullptr && error_capacity > 0) error_utf8[0] = '\0';
        return 0;
    } catch (const std::exception& ex) {
        copy_text(error_utf8, error_capacity, ex.what());
        return kOpenFailed;
    } catch (...) {
        copy_text(error_utf8, error_capacity, "Unknown native COMTRADE error.");
        return kOpenFailed;
    }
}

void ardirec_record_close(ardirec_record_handle handle) {
    delete as_record(handle);
}

int32_t ardirec_record_get_info(ardirec_record_handle handle, ardirec_record_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->abi_version = ARDIREC_BRIDGE_ABI_VERSION;
    out_info->revision_year = record->config.revision_year;
    out_info->data_format = static_cast<int32_t>(record->config.data_format);
    out_info->analog_count = static_cast<uint32_t>(record->config.analog_channels.size());
    out_info->status_count = static_cast<uint32_t>(record->config.status_channels.size());
    out_info->frame_count = static_cast<uint64_t>(record->frames.size());
    out_info->nominal_frequency = record->config.nominal_frequency;
    out_info->time_multiplier = record->config.time_multiplier;
    copy_text(out_info->station_name, sizeof(out_info->station_name), record->config.station_name);
    copy_text(out_info->recorder_id, sizeof(out_info->recorder_id), record->config.recorder_id);
    copy_text(out_info->start_time, sizeof(out_info->start_time), record->config.start_time.raw);
    copy_text(out_info->trigger_time, sizeof(out_info->trigger_time), record->config.trigger_time.raw);
    return 0;
}

int32_t ardirec_record_get_analog_channel(ardirec_record_handle handle, uint32_t channel_index,
                                          ardirec_analog_channel_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size()) return kOutOfRange;
    const auto& channel = record->config.analog_channels[channel_index];
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->index = channel.index;
    out_info->a = channel.a;
    out_info->b = channel.b;
    out_info->skew_us = channel.skew_us;
    out_info->min_value = channel.min_value;
    out_info->max_value = channel.max_value;
    if (channel.primary.has_value()) { out_info->primary = *channel.primary; out_info->has_primary = 1; }
    if (channel.secondary.has_value()) { out_info->secondary = *channel.secondary; out_info->has_secondary = 1; }
    copy_text(out_info->id, sizeof(out_info->id), channel.id);
    copy_text(out_info->phase, sizeof(out_info->phase), channel.phase);
    copy_text(out_info->circuit, sizeof(out_info->circuit), channel.circuit);
    copy_text(out_info->units, sizeof(out_info->units), channel.units);
    copy_text(out_info->primary_secondary, sizeof(out_info->primary_secondary), channel.primary_secondary);
    return 0;
}

int32_t ardirec_record_get_status_channel(ardirec_record_handle handle, uint32_t channel_index,
                                          ardirec_status_channel_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.status_channels.size()) return kOutOfRange;
    const auto& channel = record->config.status_channels[channel_index];
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->index = channel.index;
    out_info->normal_state = channel.normal_state;
    copy_text(out_info->id, sizeof(out_info->id), channel.id);
    copy_text(out_info->phase, sizeof(out_info->phase), channel.phase);
    copy_text(out_info->circuit, sizeof(out_info->circuit), channel.circuit);
    return 0;
}

int32_t ardirec_record_get_analog_semantics(ardirec_record_handle handle, uint32_t channel_index,
                                             ardirec_analog_semantics_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size()) return kOutOfRange;
    const auto& channel = record->config.analog_channels[channel_index];
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->role = static_cast<int32_t>(ardirec::comtrade::analog_role(channel));
    out_info->phase_role = static_cast<int32_t>(ardirec::comtrade::phase_role(channel));
    out_info->recorded_representation = recorded_representation_for(channel);
    out_info->has_valid_transformer_ratio = ardirec::comtrade::has_valid_transformer_ratio(channel) ? 1 : 0;
    out_info->scale_to_secondary = ardirec::comtrade::representation_scale(channel, ardirec::comtrade::ValueRepresentation::Secondary);
    out_info->scale_to_primary = ardirec::comtrade::representation_scale(channel, ardirec::comtrade::ValueRepresentation::Primary);
    return 0;
}

int32_t ardirec_record_get_representation_scale(ardirec_record_handle handle, uint32_t channel_index,
                                                 int32_t representation, double* out_scale) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_scale == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size()) return kOutOfRange;
    return representation_scale_for(record->config.analog_channels[channel_index], representation, out_scale)
               ? 0 : kInvalidArgument;
}

int32_t ardirec_record_get_cursor_measurement(ardirec_record_handle handle, uint32_t channel_index,
                                               uint64_t reference_frame, int32_t representation,
                                               ardirec_cursor_measurement_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size() || reference_frame >= record->frames.size()) return kOutOfRange;
    double scale = 1.0;
    if (!representation_scale_for(record->config.analog_channels[channel_index], representation, &scale)) return kInvalidArgument;

    std::memset(out_info, 0, sizeof(*out_info));
    const auto [first, end] = one_cycle_window(*record, reference_frame);
    const std::size_t reference = static_cast<std::size_t>(reference_frame);
    out_info->reference_frame = reference_frame;
    out_info->raw_timestamp = record->frames[reference].raw_timestamp;
    out_info->time_seconds = frame_time_seconds(*record, reference);
    out_info->window_start_frame = static_cast<uint64_t>(first);
    out_info->window_end_exclusive = static_cast<uint64_t>(end);
    out_info->instantaneous = record->frames[reference].analog[channel_index] * scale;

    long double sum_squares = 0.0L;
    uint32_t count = 0;
    for (std::size_t i = first; i < end; ++i) {
        const double value = record->frames[i].analog[channel_index];
        if (!std::isfinite(value)) continue;
        const long double scaled = static_cast<long double>(value) * static_cast<long double>(scale);
        sum_squares += scaled * scaled;
        ++count;
    }
    out_info->window_sample_count = count;
    if (count == 0 || !std::isfinite(out_info->instantaneous)) return 0;
    out_info->rms = std::sqrt(static_cast<double>(sum_squares / static_cast<long double>(count)));
    out_info->valid = std::isfinite(out_info->rms) ? 1 : 0;
    return 0;
}

int32_t ardirec_record_get_status_state(ardirec_record_handle handle, uint32_t channel_index,
                                         uint64_t reference_frame, ardirec_status_state_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.status_channels.size() || reference_frame >= record->frames.size()) return kOutOfRange;
    const auto& channel = record->config.status_channels[channel_index];
    const int32_t raw = record->frames[static_cast<std::size_t>(reference_frame)].status[channel_index] ? 1 : 0;
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->raw_state = raw;
    out_info->normal_state = channel.normal_state != 0 ? 1 : 0;
    out_info->is_active = raw != out_info->normal_state ? 1 : 0;
    return 0;
}

int32_t ardirec_record_find_nearest_status_edge(ardirec_record_handle handle, uint64_t reference_frame,
                                                 double max_distance_seconds, ardirec_status_edge_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr || !std::isfinite(max_distance_seconds) || max_distance_seconds < 0.0)
        return kInvalidArgument;
    if (reference_frame >= record->frames.size()) return kOutOfRange;
    std::memset(out_info, 0, sizeof(*out_info));
    if (record->frames.size() < 2 || record->config.status_channels.empty()) return 0;

    const std::size_t reference = static_cast<std::size_t>(reference_frame);
    const double target_time = frame_time_seconds(*record, reference);
    std::size_t first = reference;
    while (first > 1 && target_time - frame_time_seconds(*record, first - 1) <= max_distance_seconds) --first;
    first = std::max<std::size_t>(1, first);
    std::size_t end = reference + 1;
    while (end < record->frames.size() && frame_time_seconds(*record, end) - target_time <= max_distance_seconds) ++end;

    double best_distance = std::numeric_limits<double>::infinity();
    std::size_t best_frame = 0;
    uint32_t best_channel = 0;
    int32_t best_before = 0;
    int32_t best_after = 0;
    bool found = false;
    for (std::size_t frame = first; frame < end; ++frame) {
        const double distance = std::abs(frame_time_seconds(*record, frame) - target_time);
        if (distance > max_distance_seconds) continue;
        for (uint32_t channel = 0; channel < record->config.status_channels.size(); ++channel) {
            const int32_t before = record->frames[frame - 1].status[channel] ? 1 : 0;
            const int32_t after = record->frames[frame].status[channel] ? 1 : 0;
            if (before == after) continue;
            const bool better = !found || distance < best_distance - 1.0e-12
                                || (std::abs(distance - best_distance) <= 1.0e-12
                                    && (frame < best_frame || (frame == best_frame && channel < best_channel)));
            if (!better) continue;
            found = true; best_distance = distance; best_frame = frame; best_channel = channel;
            best_before = before; best_after = after;
        }
    }
    if (!found) return 0;
    const int32_t normal = record->config.status_channels[best_channel].normal_state != 0 ? 1 : 0;
    out_info->valid = 1;
    out_info->channel_index = best_channel;
    out_info->frame_index = static_cast<uint64_t>(best_frame);
    out_info->raw_timestamp = record->frames[best_frame].raw_timestamp;
    out_info->before_state = best_before;
    out_info->after_state = best_after;
    out_info->normal_state = normal;
    out_info->became_active = best_after != normal ? 1 : 0;
    out_info->distance_seconds = best_distance;
    return 0;
}

int32_t ardirec_record_copy_analog(ardirec_record_handle handle, uint32_t channel_index,
                                    uint64_t start_frame, uint64_t frame_count, double* destination) {
    const auto* record = as_record(handle);
    if (record == nullptr || (frame_count > 0 && destination == nullptr)) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size() || !valid_range(*record, start_frame, frame_count)) return kOutOfRange;
    for (uint64_t i = 0; i < frame_count; ++i) destination[i] = record->frames[static_cast<std::size_t>(start_frame + i)].analog[channel_index];
    return 0;
}

int32_t ardirec_record_copy_status(ardirec_record_handle handle, uint32_t channel_index,
                                    uint64_t start_frame, uint64_t frame_count, uint8_t* destination) {
    const auto* record = as_record(handle);
    if (record == nullptr || (frame_count > 0 && destination == nullptr)) return kInvalidArgument;
    if (channel_index >= record->config.status_channels.size() || !valid_range(*record, start_frame, frame_count)) return kOutOfRange;
    for (uint64_t i = 0; i < frame_count; ++i)
        destination[i] = record->frames[static_cast<std::size_t>(start_frame + i)].status[channel_index] ? 1u : 0u;
    return 0;
}

int32_t ardirec_record_copy_raw_timestamps(ardirec_record_handle handle, uint64_t start_frame,
                                            uint64_t frame_count, uint32_t* destination) {
    const auto* record = as_record(handle);
    if (record == nullptr || (frame_count > 0 && destination == nullptr)) return kInvalidArgument;
    if (!valid_range(*record, start_frame, frame_count)) return kOutOfRange;
    for (uint64_t i = 0; i < frame_count; ++i)
        destination[i] = record->frames[static_cast<std::size_t>(start_frame + i)].raw_timestamp;
    return 0;
}

int32_t ardirec_record_get_phasor(ardirec_record_handle handle, uint32_t channel_index,
                                   uint64_t reference_frame, ardirec_phasor_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size() || reference_frame >= record->frames.size()) return kOutOfRange;
    std::memset(out_info, 0, sizeof(*out_info));
    std::size_t first = 0, end = 0;
    const auto spectrum = analyze_spectrum(*record, channel_index, reference_frame, 1, &first, &end);
    out_info->window_start_frame = static_cast<uint64_t>(first);
    out_info->window_end_exclusive = static_cast<uint64_t>(end);
    if (!spectrum.valid || spectrum.bins.empty()) return 0;
    const auto& fundamental = spectrum.bins.front();
    const double angle = wrap_degrees(fundamental.angle_degrees - 90.0);
    const double radians = angle * kPi / 180.0;
    out_info->valid = 1;
    out_info->magnitude_rms = fundamental.magnitude_rms;
    out_info->angle_degrees = angle;
    out_info->real = fundamental.magnitude_rms * std::cos(radians);
    out_info->imag = fundamental.magnitude_rms * std::sin(radians);
    return 0;
}

int32_t ardirec_record_get_harmonic_spectrum(ardirec_record_handle handle, uint32_t channel_index,
                                              uint64_t reference_frame, int32_t maximum_order,
                                              ardirec_harmonic_spectrum_info* out_info,
                                              ardirec_harmonic_bin* bins, uint32_t bin_capacity) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr || maximum_order < 1) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size() || reference_frame >= record->frames.size()) return kOutOfRange;
    std::memset(out_info, 0, sizeof(*out_info));
    std::size_t first = 0, end = 0;
    const auto spectrum = analyze_spectrum(*record, channel_index, reference_frame, maximum_order, &first, &end);
    out_info->window_start_frame = static_cast<uint64_t>(first);
    out_info->window_end_exclusive = static_cast<uint64_t>(end);
    if (!spectrum.valid) return 0;
    out_info->valid = 1;
    out_info->dc_component = spectrum.dc_component;
    out_info->fundamental_rms = spectrum.fundamental_rms;
    out_info->thd_percent = spectrum.thd_percent;
    out_info->dominant_order = spectrum.dominant_order;
    out_info->dominant_rms = spectrum.dominant_rms;
    out_info->dominant_percent = spectrum.dominant_percent;
    out_info->estimated_sample_rate_hz = spectrum.estimated_sample_rate_hz;
    out_info->maximum_resolvable_order = spectrum.maximum_resolvable_order;
    out_info->bin_count = static_cast<uint32_t>(spectrum.bins.size());
    if (bins == nullptr || bin_capacity == 0) return 0;
    if (bin_capacity < out_info->bin_count) return kInsufficientBuffer;
    const double fundamental = spectrum.fundamental_rms;
    for (std::size_t i = 0; i < spectrum.bins.size(); ++i) {
        const auto& source = spectrum.bins[i];
        bins[i].order = source.order;
        bins[i].magnitude_rms = source.magnitude_rms;
        bins[i].percent_of_fundamental = fundamental > 1.0e-12 ? source.magnitude_rms / fundamental * 100.0 : 0.0;
        bins[i].angle_degrees = source.angle_degrees;
    }
    return 0;
}

int32_t ardirec_record_get_distance_current_floor(ardirec_record_handle handle,
                                                   int32_t representation,
                                                   double* out_minimum_current) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_minimum_current == nullptr) return kInvalidArgument;
    if (representation < ARDIREC_VALUE_RECORDED || representation > ARDIREC_VALUE_PRIMARY) return kInvalidArgument;

    double current_peak = 0.0;
    for (const int channel_index : record->current_channels) {
        if (channel_index < 0) continue;
        const auto& channel = record->config.analog_channels[static_cast<std::size_t>(channel_index)];
        double representation_scale = 1.0;
        if (!representation_scale_for(channel, representation, &representation_scale)) return kInvalidArgument;
        const double si_scale = unit_scale_to_si(channel);
        for (const auto& frame : record->frames) {
            const double value = frame.analog[static_cast<std::size_t>(channel_index)];
            if (!std::isfinite(value)) continue;
            current_peak = std::max(current_peak, std::abs(value * representation_scale * si_scale));
        }
    }
    *out_minimum_current = (!std::isfinite(current_peak) || current_peak <= 0.0)
                               ? 1.0e-6 : std::max(1.0e-6, current_peak * 1.0e-3);
    return 0;
}

int32_t ardirec_record_get_distance_loops(ardirec_record_handle handle,
                                           uint64_t reference_frame,
                                           int32_t representation,
                                           double grounding_factor_magnitude,
                                           double grounding_factor_angle_degrees,
                                           double minimum_current,
                                           ardirec_distance_point* points,
                                           uint32_t point_capacity) {
    const auto* record = as_record(handle);
    if (record == nullptr || points == nullptr || point_capacity < kDistanceLoopCount
        || reference_frame >= record->frames.size()
        || representation < ARDIREC_VALUE_RECORDED || representation > ARDIREC_VALUE_PRIMARY
        || !std::isfinite(grounding_factor_magnitude) || grounding_factor_magnitude < 0.0
        || !std::isfinite(grounding_factor_angle_degrees)
        || !std::isfinite(minimum_current) || minimum_current <= 0.0) {
        return kInvalidArgument;
    }

    ardirec::distance::ThreePhasePhasors phasors;
    if (!fill_distance_phasors(*record, reference_frame, representation, phasors)) return kOutOfRange;
    const auto kl = grounding_factor(grounding_factor_magnitude, grounding_factor_angle_degrees);
    for (int32_t loop = ARDIREC_DISTANCE_L1_E; loop <= ARDIREC_DISTANCE_L3_L1; ++loop)
        fill_distance_result(points[static_cast<std::size_t>(loop)], loop, *record, reference_frame, phasors, kl, minimum_current);
    return 0;
}

int32_t ardirec_record_get_distance_locus(ardirec_record_handle handle,
                                           int32_t loop,
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
    const auto* record = as_record(handle);
    if (record == nullptr || out_point_count == nullptr
        || loop < ARDIREC_DISTANCE_L1_E || loop > ARDIREC_DISTANCE_L3_L1
        || maximum_points == 0 || !valid_range(*record, start_frame, frame_count)
        || representation < ARDIREC_VALUE_RECORDED || representation > ARDIREC_VALUE_PRIMARY
        || !std::isfinite(grounding_factor_magnitude) || grounding_factor_magnitude < 0.0
        || !std::isfinite(grounding_factor_angle_degrees)
        || !std::isfinite(minimum_current) || minimum_current <= 0.0) {
        return kInvalidArgument;
    }

    const uint32_t required = normalized_locus_point_count(frame_count, maximum_points);
    *out_point_count = required;
    if (points == nullptr || point_capacity == 0 || required == 0) return 0;
    if (point_capacity < required) return kInsufficientBuffer;

    const uint32_t bounded = std::clamp<uint32_t>(maximum_points, 16u, 4000u);
    const uint64_t stride = frame_count <= bounded || frame_count <= 1
                                ? 1u
                                : (frame_count - 1u + (bounded - 2u)) / (bounded - 1u);
    const auto kl = grounding_factor(grounding_factor_magnitude, grounding_factor_angle_degrees);
    uint32_t write = 0;
    uint64_t last_relative = std::numeric_limits<uint64_t>::max();
    for (uint64_t relative = 0; relative < frame_count && write < required; relative += stride) {
        const uint64_t frame = start_frame + relative;
        ardirec::distance::ThreePhasePhasors phasors;
        fill_distance_phasors(*record, frame, representation, phasors);
        fill_distance_result(points[write++], loop, *record, frame, phasors, kl, minimum_current);
        last_relative = relative;
        if (frame_count - relative <= stride) break;
    }
    const uint64_t final_relative = frame_count > 0 ? frame_count - 1u : 0u;
    if (frame_count > 0 && last_relative != final_relative && write < required) {
        const uint64_t frame = start_frame + final_relative;
        ardirec::distance::ThreePhasePhasors phasors;
        fill_distance_phasors(*record, frame, representation, phasors);
        fill_distance_result(points[write++], loop, *record, frame, phasors, kl, minimum_current);
    }
    *out_point_count = write;
    return 0;
}

} // extern "C"
