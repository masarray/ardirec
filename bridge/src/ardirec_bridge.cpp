// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec_bridge.h"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"
#include "ardirec/power/harmonics.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
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
};

constexpr int32_t kInvalidArgument = -1;
constexpr int32_t kOutOfRange = -2;
constexpr int32_t kOpenFailed = -3;
constexpr int32_t kInsufficientBuffer = -4;
constexpr double kPi = 3.141592653589793238462643383279502884;

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

} // namespace

extern "C" {

uint32_t ardirec_bridge_abi_version(void) {
    return ARDIREC_BRIDGE_ABI_VERSION;
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
        if (bundle.dat.empty()) {
            throw std::runtime_error("Matching DAT file was not found.");
        }

        auto record = std::make_unique<NativeRecord>();
        record->config = ardirec::comtrade::ConfigParser{}.parse_file(bundle.cfg);
        record->frames = ardirec::comtrade::DatReader{}.read(record->config, bundle.dat);
        if (record->frames.empty()) {
            throw std::runtime_error("COMTRADE DAT contains no readable frames.");
        }

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

int32_t ardirec_record_get_analog_channel(
    ardirec_record_handle handle,
    uint32_t channel_index,
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
    if (channel.primary.has_value()) {
        out_info->primary = *channel.primary;
        out_info->has_primary = 1;
    }
    if (channel.secondary.has_value()) {
        out_info->secondary = *channel.secondary;
        out_info->has_secondary = 1;
    }
    copy_text(out_info->id, sizeof(out_info->id), channel.id);
    copy_text(out_info->phase, sizeof(out_info->phase), channel.phase);
    copy_text(out_info->circuit, sizeof(out_info->circuit), channel.circuit);
    copy_text(out_info->units, sizeof(out_info->units), channel.units);
    copy_text(out_info->primary_secondary, sizeof(out_info->primary_secondary), channel.primary_secondary);
    return 0;
}

int32_t ardirec_record_get_status_channel(
    ardirec_record_handle handle,
    uint32_t channel_index,
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

int32_t ardirec_record_copy_analog(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t start_frame,
    uint64_t frame_count,
    double* destination) {
    const auto* record = as_record(handle);
    if (record == nullptr || (frame_count > 0 && destination == nullptr)) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size() || !valid_range(*record, start_frame, frame_count)) {
        return kOutOfRange;
    }

    for (uint64_t i = 0; i < frame_count; ++i) {
        destination[i] = record->frames[static_cast<size_t>(start_frame + i)].analog[channel_index];
    }
    return 0;
}

int32_t ardirec_record_copy_status(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t start_frame,
    uint64_t frame_count,
    uint8_t* destination) {
    const auto* record = as_record(handle);
    if (record == nullptr || (frame_count > 0 && destination == nullptr)) return kInvalidArgument;
    if (channel_index >= record->config.status_channels.size() || !valid_range(*record, start_frame, frame_count)) {
        return kOutOfRange;
    }

    for (uint64_t i = 0; i < frame_count; ++i) {
        destination[i] = record->frames[static_cast<size_t>(start_frame + i)].status[channel_index] ? 1u : 0u;
    }
    return 0;
}

int32_t ardirec_record_copy_raw_timestamps(
    ardirec_record_handle handle,
    uint64_t start_frame,
    uint64_t frame_count,
    uint32_t* destination) {
    const auto* record = as_record(handle);
    if (record == nullptr || (frame_count > 0 && destination == nullptr)) return kInvalidArgument;
    if (!valid_range(*record, start_frame, frame_count)) return kOutOfRange;

    for (uint64_t i = 0; i < frame_count; ++i) {
        destination[i] = record->frames[static_cast<size_t>(start_frame + i)].raw_timestamp;
    }
    return 0;
}

int32_t ardirec_record_get_phasor(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t reference_frame,
    ardirec_phasor_info* out_info) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size()
        || reference_frame >= record->frames.size()) {
        return kOutOfRange;
    }

    std::memset(out_info, 0, sizeof(*out_info));
    std::size_t first = 0;
    std::size_t end = 0;
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

int32_t ardirec_record_get_harmonic_spectrum(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t reference_frame,
    int32_t maximum_order,
    ardirec_harmonic_spectrum_info* out_info,
    ardirec_harmonic_bin* bins,
    uint32_t bin_capacity) {
    const auto* record = as_record(handle);
    if (record == nullptr || out_info == nullptr || maximum_order < 1) return kInvalidArgument;
    if (channel_index >= record->config.analog_channels.size()
        || reference_frame >= record->frames.size()) {
        return kOutOfRange;
    }

    std::memset(out_info, 0, sizeof(*out_info));
    std::size_t first = 0;
    std::size_t end = 0;
    const auto spectrum = analyze_spectrum(
        *record, channel_index, reference_frame, maximum_order, &first, &end);
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
        bins[i].percent_of_fundamental = fundamental > 1.0e-12
                                            ? source.magnitude_rms / fundamental * 100.0
                                            : 0.0;
        bins[i].angle_degrees = source.angle_degrees;
    }
    return 0;
}

} // extern "C"
