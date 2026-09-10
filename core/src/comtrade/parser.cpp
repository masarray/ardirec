// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace ardirec::comtrade {
namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    if (value.size() >= 3 && static_cast<unsigned char>(value[0]) == 0xEF
        && static_cast<unsigned char>(value[1]) == 0xBB
        && static_cast<unsigned char>(value[2]) == 0xBF) {
        value.erase(0, 3);
    }
    return value;
}

std::string_view trim_view(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
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

bool read_line(std::ifstream& in, std::string& line) {
    if (!std::getline(in, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

bool parse_int_prefix(std::string_view text, int& value) {
    text = trim_view(text);
    if (text.empty()) return false;
    const char* begin = text.data();
    const char* end = begin + text.size();
    int parsed = 0;
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr == begin) return false;
    value = parsed;
    return true;
}

bool parse_u64_prefix(std::string_view text, std::uint64_t& value) {
    text = trim_view(text);
    if (text.empty()) return false;
    const char* begin = text.data();
    const char* end = begin + text.size();
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr == begin) return false;
    value = parsed;
    return true;
}

bool parse_double_strict(std::string_view text, double& value) {
    text = trim_view(text);
    if (text.empty()) return false;
    double parsed = 0.0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed,
                                        std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

std::optional<double> optional_double(const std::vector<std::string>& fields,
                                      std::size_t index,
                                      std::vector<std::string>& diagnostics,
                                      const std::string& label) {
    if (index >= fields.size() || fields[index].empty()) return std::nullopt;
    double value = 0.0;
    if (parse_double_strict(fields[index], value)) return value;
    diagnostics.emplace_back(label + " is invalid; ratio metadata ignored.");
    return std::nullopt;
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

void diagnostic_fallback(std::vector<std::string>& diagnostics,
                         const std::string& field,
                         const std::string& fallback) {
    diagnostics.emplace_back(field + " is invalid/missing; using " + fallback + ".");
}

bool require_line(std::ifstream& in,
                  std::string& line,
                  const char* section,
                  ConfigParseResult& result) {
    if (read_line(in, line)) return true;
    result.error = std::string("Unexpected end of CFG while reading ") + section;
    return false;
}

} // namespace

ConfigParseResult ConfigParser::try_parse_file(const std::filesystem::path& path) const noexcept {
    ConfigParseResult result;
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            result.error = "Cannot open CFG: " + path.string();
            return result;
        }

        RecordConfig cfg;
        cfg.cfg_path = path;
        std::string line;

        if (!require_line(in, line, "header", result)) return result;
        const auto header = csv(line);
        if (header.empty()) {
            result.error = "CFG header is empty";
            return result;
        }
        cfg.station_name = header[0];
        if (cfg.station_name.empty()) {
            cfg.station_name = path.stem().string();
            diagnostic_fallback(cfg.diagnostics, "Station name", "CFG filename");
        }
        if (header.size() > 1 && !header[1].empty()) {
            cfg.recorder_id = header[1];
        } else {
            cfg.recorder_id = "unknown";
            diagnostic_fallback(cfg.diagnostics, "Recorder id", "unknown");
        }
        if (header.size() > 2 && !header[2].empty()) {
            int revision = 0;
            if (parse_int_prefix(header[2], revision) && revision >= 1900 && revision <= 9999) {
                cfg.revision_year = revision;
            } else {
                diagnostic_fallback(cfg.diagnostics, "Revision year", "1991 semantics");
            }
        }

        if (!require_line(in, line, "channel counts", result)) return result;
        const auto counts = csv(line);
        if (counts.size() < 3) {
            result.error = "CFG channel count line is incomplete";
            return result;
        }

        int analogCount = 0;
        int statusCount = 0;
        if (!parse_int_prefix(counts[1], analogCount) || analogCount < 0) {
            result.error = "Invalid analog channel count: " + counts[1];
            return result;
        }
        if (!parse_int_prefix(counts[2], statusCount) || statusCount < 0) {
            result.error = "Invalid status channel count: " + counts[2];
            return result;
        }
        constexpr int kMaximumReasonableChannels = 1'000'000;
        if (analogCount > kMaximumReasonableChannels || statusCount > kMaximumReasonableChannels
            || analogCount > kMaximumReasonableChannels - statusCount) {
            result.error = "CFG channel count is unreasonably large";
            return result;
        }

        const int derivedTotal = analogCount + statusCount;
        int declaredTotal = 0;
        if (!parse_int_prefix(counts[0], declaredTotal) || declaredTotal < 0) {
            cfg.total_channels = derivedTotal;
            diagnostic_fallback(cfg.diagnostics, "Total channel count", std::to_string(derivedTotal));
        } else {
            cfg.total_channels = declaredTotal;
        }

        cfg.analog_channels.reserve(static_cast<std::size_t>(analogCount));
        for (int i = 0; i < analogCount; ++i) {
            if (!require_line(in, line, "analog channel", result)) return result;
            const auto fields = csv(line);
            AnalogChannel channel;
            const std::string prefix = "Analog channel " + std::to_string(i + 1) + ": ";

            if (fields.empty() || !parse_int_prefix(fields[0], channel.index) || channel.index <= 0) {
                channel.index = i + 1;
                diagnostic_fallback(cfg.diagnostics, prefix + "index", std::to_string(channel.index));
            }
            if (fields.size() > 1 && !fields[1].empty()) channel.id = fields[1];
            else {
                channel.id = "A" + std::to_string(i + 1);
                diagnostic_fallback(cfg.diagnostics, prefix + "id", channel.id);
            }
            if (fields.size() > 2) channel.phase = fields[2];
            if (fields.size() > 3) channel.circuit = fields[3];
            if (fields.size() > 4) channel.units = fields[4];

            auto recoverDouble = [&](std::size_t fieldIndex,
                                     double& target,
                                     double fallback,
                                     const char* name) {
                if (fieldIndex < fields.size() && parse_double_strict(fields[fieldIndex], target)) return;
                target = fallback;
                diagnostic_fallback(cfg.diagnostics, prefix + name, std::to_string(fallback));
            };
            recoverDouble(5, channel.a, 1.0, "scale a");
            recoverDouble(6, channel.b, 0.0, "scale b");
            recoverDouble(7, channel.skew_us, 0.0, "skew");
            recoverDouble(8, channel.min_value, 0.0, "minimum");
            recoverDouble(9, channel.max_value, 0.0, "maximum");
            channel.primary = optional_double(fields, 10, cfg.diagnostics, prefix + "primary ratio");
            channel.secondary = optional_double(fields, 11, cfg.diagnostics, prefix + "secondary ratio");
            if (fields.size() > 12) channel.primary_secondary = fields[12];
            cfg.analog_channels.push_back(std::move(channel));
        }

        cfg.status_channels.reserve(static_cast<std::size_t>(statusCount));
        for (int i = 0; i < statusCount; ++i) {
            if (!require_line(in, line, "status channel", result)) return result;
            const auto fields = csv(line);
            StatusChannel channel;
            const std::string prefix = "Status channel " + std::to_string(i + 1) + ": ";

            if (fields.empty() || !parse_int_prefix(fields[0], channel.index) || channel.index <= 0) {
                channel.index = i + 1;
                diagnostic_fallback(cfg.diagnostics, prefix + "index", std::to_string(channel.index));
            }
            if (fields.size() > 1 && !fields[1].empty()) channel.id = fields[1];
            else {
                channel.id = "D" + std::to_string(i + 1);
                diagnostic_fallback(cfg.diagnostics, prefix + "id", channel.id);
            }
            if (fields.size() > 2) channel.phase = fields[2];
            if (fields.size() > 3) channel.circuit = fields[3];
            if (fields.size() > 4 && !fields[4].empty()) {
                int normal = 0;
                if (parse_int_prefix(fields[4], normal) && (normal == 0 || normal == 1)) {
                    channel.normal_state = normal;
                } else {
                    diagnostic_fallback(cfg.diagnostics, prefix + "normal state", "0");
                }
            }
            cfg.status_channels.push_back(std::move(channel));
        }

        if (!require_line(in, line, "nominal frequency", result)) return result;
        double nominalFrequency = 0.0;
        if (!parse_double_strict(line, nominalFrequency) || nominalFrequency <= 0.0) {
            cfg.nominal_frequency = 50.0;
            diagnostic_fallback(cfg.diagnostics, "Nominal frequency", "50 Hz");
        } else {
            cfg.nominal_frequency = nominalFrequency;
        }

        if (!require_line(in, line, "sample-rate count", result)) return result;
        int rateCount = 0;
        if (!parse_int_prefix(line, rateCount) || rateCount < 0 || rateCount > 100'000) {
            result.error = "Invalid sample-rate segment count: " + trim(line);
            return result;
        }
        if (rateCount == 0) cfg.diagnostics.emplace_back("CFG declares no sample-rate segments.");
        cfg.sample_rates.reserve(static_cast<std::size_t>(rateCount));
        for (int i = 0; i < rateCount; ++i) {
            if (!require_line(in, line, "sample-rate segment", result)) return result;
            const auto fields = csv(line);
            if (fields.size() < 2) {
                cfg.diagnostics.emplace_back("Sample-rate segment " + std::to_string(i + 1)
                                             + " is incomplete; segment ignored.");
                continue;
            }
            double sampleRate = 0.0;
            std::uint64_t endSample = 0;
            if (!parse_double_strict(fields[0], sampleRate) || sampleRate <= 0.0
                || !parse_u64_prefix(fields[1], endSample)) {
                cfg.diagnostics.emplace_back("Sample-rate segment " + std::to_string(i + 1)
                                             + " is invalid; segment ignored.");
                continue;
            }
            cfg.sample_rates.push_back({sampleRate, endSample});
        }

        if (!require_line(in, line, "start time", result)) return result;
        cfg.start_time.raw = trim(line);
        if (cfg.start_time.raw.empty()) cfg.diagnostics.emplace_back("CFG start timestamp is empty.");

        if (!require_line(in, line, "trigger time", result)) return result;
        cfg.trigger_time.raw = trim(line);
        if (cfg.trigger_time.raw.empty()) cfg.diagnostics.emplace_back("CFG trigger timestamp is empty.");

        if (!require_line(in, line, "data format", result)) return result;
        cfg.data_format = parse_format(line);
        if (cfg.data_format == DataFormat::Unknown) cfg.diagnostics.emplace_back("Unknown DAT format.");

        if (read_line(in, line)) {
            line = trim(line);
            if (!line.empty()) {
                double multiplier = 0.0;
                if (parse_double_strict(line, multiplier) && multiplier > 0.0) {
                    cfg.time_multiplier = multiplier;
                } else {
                    diagnostic_fallback(cfg.diagnostics, "Time multiplier", "1.0");
                }
            }
        }

        if (cfg.total_channels != derivedTotal) {
            cfg.diagnostics.emplace_back("Total channel count does not match analog + status counts; channel lists were retained.");
        }

        result.config = std::move(cfg);
        return result;
    } catch (const std::exception& ex) {
        result.config.reset();
        result.error = std::string("Unexpected CFG parser failure: ") + ex.what();
        return result;
    } catch (...) {
        result.config.reset();
        result.error = "Unknown CFG parser failure";
        return result;
    }
}

RecordConfig ConfigParser::parse_file(const std::filesystem::path& path) const {
    auto result = try_parse_file(path);
    if (!result) throw std::runtime_error(result.error.empty() ? "CFG parsing failed" : result.error);
    return std::move(*result.config);
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
