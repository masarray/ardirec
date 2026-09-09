// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec_bridge.h"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct NativeRecord {
    ardirec::comtrade::RecordConfig config;
    std::vector<ardirec::comtrade::SampleFrame> frames;
};

constexpr int32_t kInvalidArgument = -1;
constexpr int32_t kOutOfRange = -2;
constexpr int32_t kOpenFailed = -3;

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

} // extern "C"
