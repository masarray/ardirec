// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace ardirec::comtrade {
namespace {

std::string trim(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    if (value.size() >= 3 && static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB && static_cast<unsigned char>(value[2]) == 0xBF) {
        value.erase(0, 3);
    }
    return value;
}

std::vector<std::string> csv(std::string_view line) {
    std::vector<std::string> out;
    std::string field;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                field.push_back('"');
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (c == ',' && !quoted) {
            out.push_back(trim(field));
            field.clear();
        } else {
            field.push_back(c);
        }
    }
    out.push_back(trim(field));
    return out;
}

int as_int(const std::string& s, const char* what) {
    try { return std::stoi(s); }
    catch (...) { throw std::runtime_error(std::string("Invalid ") + what + ": " + s); }
}

double as_double(const std::string& s, const char* what) {
    try { return std::stod(s); }
    catch (...) { throw std::runtime_error(std::string("Invalid ") + what + ": " + s); }
}

std::optional<double> optional_double(const std::vector<std::string>& v, std::size_t i) {
    if (i >= v.size() || v[i].empty()) return std::nullopt;
    try { return std::stod(v[i]); } catch (...) { return std::nullopt; }
}

std::string next_line(std::ifstream& in, const char* section) {
    std::string line;
    if (!std::getline(in, line)) {
        throw std::runtime_error(std::string("Unexpected end of CFG while reading ") + section);
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

DataFormat parse_format(std::string value) {
    value = trim(value);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    if (value == "ASCII") return DataFormat::Ascii;
    if (value == "BINARY") return DataFormat::Binary16;
    if (value == "BINARY32") return DataFormat::Binary32;
    if (value == "FLOAT32") return DataFormat::Float32;
    return DataFormat::Unknown;
}

} // namespace

RecordConfig ConfigParser::parse_file(const std::filesystem::path& path) const {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open CFG: " + path.string());

    RecordConfig cfg;
    cfg.cfg_path = path;

    const auto header = csv(next_line(in, "header"));
    if (header.size() < 2) throw std::runtime_error("CFG header must contain station and recorder id");
    cfg.station_name = header[0];
    cfg.recorder_id = header[1];
    if (header.size() >= 3 && !header[2].empty()) cfg.revision_year = as_int(header[2], "revision year");

    const auto counts = csv(next_line(in, "channel counts"));
    if (counts.size() < 3) throw std::runtime_error("CFG channel count line is incomplete");
    cfg.total_channels = as_int(counts[0], "total channel count");
    const int analog_count = as_int(counts[1], "analog channel count");
    const int status_count = as_int(counts[2], "status channel count");

    cfg.analog_channels.reserve(static_cast<std::size_t>(std::max(analog_count, 0)));
    for (int i = 0; i < analog_count; ++i) {
        const auto f = csv(next_line(in, "analog channel"));
        if (f.size() < 10) throw std::runtime_error("Analog channel line has too few fields");
        AnalogChannel ch;
        ch.index = as_int(f[0], "analog index");
        ch.id = f[1];
        if (f.size() > 2) ch.phase = f[2];
        if (f.size() > 3) ch.circuit = f[3];
        if (f.size() > 4) ch.units = f[4];
        ch.a = as_double(f[5], "analog scale a");
        ch.b = as_double(f[6], "analog scale b");
        ch.skew_us = as_double(f[7], "analog skew");
        ch.min_value = as_double(f[8], "analog min");
        ch.max_value = as_double(f[9], "analog max");
        ch.primary = optional_double(f, 10);
        ch.secondary = optional_double(f, 11);
        if (f.size() > 12) ch.primary_secondary = f[12];
        cfg.analog_channels.push_back(std::move(ch));
    }

    cfg.status_channels.reserve(static_cast<std::size_t>(std::max(status_count, 0)));
    for (int i = 0; i < status_count; ++i) {
        const auto f = csv(next_line(in, "status channel"));
        if (f.size() < 2) throw std::runtime_error("Status channel line has too few fields");
        StatusChannel ch;
        ch.index = as_int(f[0], "status index");
        ch.id = f[1];
        if (f.size() > 2) ch.phase = f[2];
        if (f.size() > 3) ch.circuit = f[3];
        if (f.size() > 4 && !f[4].empty()) ch.normal_state = as_int(f[4], "status normal state");
        cfg.status_channels.push_back(std::move(ch));
    }

    cfg.nominal_frequency = as_double(trim(next_line(in, "nominal frequency")), "nominal frequency");
    const int rate_count = as_int(trim(next_line(in, "sample-rate count")), "sample-rate count");
    if (rate_count <= 0) cfg.diagnostics.emplace_back("CFG declares no sample-rate segments.");
    for (int i = 0; i < rate_count; ++i) {
        const auto f = csv(next_line(in, "sample-rate segment"));
        if (f.size() < 2) throw std::runtime_error("Sample-rate line is incomplete");
        SampleRateSegment segment;
        segment.samples_per_second = as_double(f[0], "sample rate");
        segment.end_sample = static_cast<std::uint64_t>(std::stoull(f[1]));
        cfg.sample_rates.push_back(segment);
    }

    cfg.start_time.raw = trim(next_line(in, "start time"));
    cfg.trigger_time.raw = trim(next_line(in, "trigger time"));
    cfg.data_format = parse_format(next_line(in, "data format"));
    if (cfg.data_format == DataFormat::Unknown) cfg.diagnostics.emplace_back("Unknown DAT format.");

    std::string multiplier;
    if (std::getline(in, multiplier)) {
        if (!multiplier.empty() && multiplier.back() == '\r') multiplier.pop_back();
        multiplier = trim(multiplier);
        if (!multiplier.empty()) {
            try { cfg.time_multiplier = std::stod(multiplier); }
            catch (...) { cfg.diagnostics.emplace_back("Invalid time multiplier; defaulting to 1.0."); }
        }
    }

    if (cfg.total_channels != analog_count + status_count) {
        cfg.diagnostics.emplace_back("Total channel count does not match analog + status counts.");
    }
    return cfg;
}

const char* to_string(DataFormat format) noexcept {
    switch (format) {
    case DataFormat::Ascii: return "ASCII";
    case DataFormat::Binary16: return "BINARY";
    case DataFormat::Binary32: return "BINARY32";
    case DataFormat::Float32: return "FLOAT32";
    default: return "UNKNOWN";
    }
}

} // namespace ardirec::comtrade
