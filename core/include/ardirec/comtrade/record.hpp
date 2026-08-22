// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ardirec::comtrade {

enum class DataFormat { Ascii, Binary16, Binary32, Float32, Unknown };

struct AnalogChannel {
    int index{};
    std::string id;
    std::string phase;
    std::string circuit;
    std::string units;
    double a{1.0};
    double b{0.0};
    double skew_us{};
    double min_value{};
    double max_value{};
    std::optional<double> primary;
    std::optional<double> secondary;
    std::string primary_secondary;
};

struct StatusChannel {
    int index{};
    std::string id;
    std::string phase;
    std::string circuit;
    int normal_state{};
};

struct SampleRateSegment {
    double samples_per_second{};
    std::uint64_t end_sample{};
};

struct TimestampText {
    std::string raw;
};

struct RecordConfig {
    std::filesystem::path cfg_path;
    std::string station_name;
    std::string recorder_id;
    int revision_year{1991};
    int total_channels{};
    std::vector<AnalogChannel> analog_channels;
    std::vector<StatusChannel> status_channels;
    double nominal_frequency{};
    std::vector<SampleRateSegment> sample_rates;
    TimestampText start_time;
    TimestampText trigger_time;
    DataFormat data_format{DataFormat::Unknown};
    double time_multiplier{1.0};
    std::vector<std::string> diagnostics;
};

struct SampleFrame {
    std::uint32_t sample_number{};
    std::uint32_t raw_timestamp{};
    std::vector<double> analog;
    std::vector<bool> status;
};

struct FileBundle {
    std::filesystem::path cfg;
    std::filesystem::path dat;
    std::filesystem::path hdr;
    std::filesystem::path inf;
    std::filesystem::path dmf;
};

} // namespace ardirec::comtrade
