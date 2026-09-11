// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
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


bool continuation(unsigned char value) {
    return value >= 0x80 && value <= 0xBF;
}

bool valid_utf8(std::string_view value) {
    std::size_t i = 0;
    while (i < value.size()) {
        const auto first = static_cast<unsigned char>(value[i]);
        if (first <= 0x7F) {
            ++i;
            continue;
        }
        if (first >= 0xC2 && first <= 0xDF) {
            if (i + 1 >= value.size()
                || !continuation(static_cast<unsigned char>(value[i + 1]))) return false;
            i += 2;
            continue;
        }
        if (first == 0xE0) {
            if (i + 2 >= value.size()) return false;
            const auto second = static_cast<unsigned char>(value[i + 1]);
            if (second < 0xA0 || second > 0xBF
                || !continuation(static_cast<unsigned char>(value[i + 2]))) return false;
            i += 3;
            continue;
        }
        if ((first >= 0xE1 && first <= 0xEC) || (first >= 0xEE && first <= 0xEF)) {
            if (i + 2 >= value.size()
                || !continuation(static_cast<unsigned char>(value[i + 1]))
                || !continuation(static_cast<unsigned char>(value[i + 2]))) return false;
            i += 3;
            continue;
        }
        if (first == 0xED) {
            if (i + 2 >= value.size()) return false;
            const auto second = static_cast<unsigned char>(value[i + 1]);
            if (second < 0x80 || second > 0x9F
                || !continuation(static_cast<unsigned char>(value[i + 2]))) return false;
            i += 3;
            continue;
        }
        if (first == 0xF0) {
            if (i + 3 >= value.size()) return false;
            const auto second = static_cast<unsigned char>(value[i + 1]);
            if (second < 0x90 || second > 0xBF
                || !continuation(static_cast<unsigned char>(value[i + 2]))
                || !continuation(static_cast<unsigned char>(value[i + 3]))) return false;
            i += 4;
            continue;
        }
        if (first >= 0xF1 && first <= 0xF3) {
            if (i + 3 >= value.size()
                || !continuation(static_cast<unsigned char>(value[i + 1]))
                || !continuation(static_cast<unsigned char>(value[i + 2]))
                || !continuation(static_cast<unsigned char>(value[i + 3]))) return false;
            i += 4;
            continue;
        }
        if (first == 0xF4) {
            if (i + 3 >= value.size()) return false;
            const auto second = static_cast<unsigned char>(value[i + 1]);
            if (second < 0x80 || second > 0x8F
                || !continuation(static_cast<unsigned char>(value[i + 2]))
                || !continuation(static_cast<unsigned char>(value[i + 3]))) return false;
            i += 4;
            continue;
        }
        return false;
    }
    return true;
}

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::string windows_1252_to_utf8(std::string_view value) {
    // Undefined Windows-1252 positions intentionally map to their C1 code points so no source
    // byte is silently discarded. Printable CP1252 punctuation uses its Unicode code point.
    static constexpr std::array<std::uint16_t, 32> extension = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
    };

    std::string output;
    output.reserve(value.size() + value.size() / 4);
    for (const unsigned char byte : value) {
        std::uint32_t codepoint = byte;
        if (byte >= 0x80 && byte <= 0x9F) codepoint = extension[byte - 0x80];
        append_utf8(output, codepoint);
    }
    return output;
}

std::string normalize_cfg_text(std::string value) {
    if (value.empty() || valid_utf8(value)) return value;
    // Legacy COMTRADE exports commonly predate an explicit Unicode encoding. For invalid UTF-8
    // only, interpret Western-European relay metadata deterministically as Windows-1252.
    return windows_1252_to_utf8(value);
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
        cfg.station_name = normalize_cfg_text(header[0]);
        if (cfg.station_name.empty()) {
            cfg.station_name = path.stem().string();
            diagnostic_fallback(cfg.diagnostics, "Station name", "CFG filename");
        }
        if (header.size() > 1 && !header[1].empty()) {
            cfg.recorder_id = normalize_cfg_text(header[1]);
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
            if (fields.size() > 1 && !fields[1].empty()) channel.id = normalize_cfg_text(fields[1]);
            else {
                channel.id = "A" + std::to_string(i + 1);
                diagnostic_fallback(cfg.diagnostics, prefix + "id", channel.id);
            }
            if (fields.size() > 2) channel.phase = normalize_cfg_text(fields[2]);
            if (fields.size() > 3) channel.circuit = normalize_cfg_text(fields[3]);
            if (fields.size() > 4) channel.units = normalize_cfg_text(fields[4]);

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
            if (fields.size() > 12) channel.primary_secondary = normalize_cfg_text(fields[12]);
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
            if (fields.size() > 1 && !fields[1].empty()) channel.id = normalize_cfg_text(fields[1]);
            else {
                channel.id = "D" + std::to_string(i + 1);
                diagnostic_fallback(cfg.diagnostics, prefix + "id", channel.id);
            }
            if (fields.size() > 2) channel.phase = normalize_cfg_text(fields[2]);
            if (fields.size() > 3) channel.circuit = normalize_cfg_text(fields[3]);
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
