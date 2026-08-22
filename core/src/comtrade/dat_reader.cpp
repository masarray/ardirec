// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/dat_reader.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace ardirec::comtrade {
namespace {

template <typename T>
T read_le(std::istream& in) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) throw std::runtime_error("Unexpected end of binary DAT");
    if constexpr (std::endian::native == std::endian::big) {
        auto* p = reinterpret_cast<unsigned char*>(&value);
        std::reverse(p, p + sizeof(T));
    }
    return value;
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(item);
    return out;
}

double scaled(const AnalogChannel& ch, double raw) {
    return ch.a * raw + ch.b;
}

bool missing_i16(std::int16_t v, int revision) {
    if (revision <= 1991) return static_cast<std::uint16_t>(v) == 0xFFFFu;
    return v == std::numeric_limits<std::int16_t>::min();
}

} // namespace

std::vector<SampleFrame> DatReader::read(const RecordConfig& config,
                                         const std::filesystem::path& dat_path,
                                         std::size_t max_frames) const {
    std::vector<SampleFrame> result;
    std::ifstream in(dat_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open DAT: " + dat_path.string());

    const std::size_t analog_count = config.analog_channels.size();
    const std::size_t status_count = config.status_channels.size();
    const std::size_t status_words = (status_count + 15u) / 16u;

    if (config.data_format == DataFormat::Ascii) {
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            const auto f = split(line);
            if (f.size() < 2 + analog_count + status_count) {
                throw std::runtime_error("ASCII DAT row has fewer fields than declared channels");
            }
            SampleFrame frame;
            frame.sample_number = static_cast<std::uint32_t>(std::stoul(f[0]));
            frame.raw_timestamp = static_cast<std::uint32_t>(std::stoul(f[1]));
            frame.analog.reserve(analog_count);
            for (std::size_t i = 0; i < analog_count; ++i) {
                const double raw = std::stod(f[2 + i]);
                frame.analog.push_back(scaled(config.analog_channels[i], raw));
            }
            frame.status.reserve(status_count);
            for (std::size_t i = 0; i < status_count; ++i) {
                frame.status.push_back(std::stoi(f[2 + analog_count + i]) != 0);
            }
            result.push_back(std::move(frame));
            if (max_frames != 0 && result.size() >= max_frames) break;
        }
        return result;
    }

    if (config.data_format == DataFormat::Unknown) {
        throw std::runtime_error("Unsupported/unknown DAT format");
    }

    while (in.peek() != std::char_traits<char>::eof()) {
        SampleFrame frame;
        try {
            frame.sample_number = read_le<std::uint32_t>(in);
            frame.raw_timestamp = read_le<std::uint32_t>(in);
        } catch (const std::runtime_error&) {
            if (result.empty()) throw;
            break;
        }

        frame.analog.reserve(analog_count);
        for (std::size_t i = 0; i < analog_count; ++i) {
            double raw{};
            bool missing = false;
            if (config.data_format == DataFormat::Binary16) {
                const auto v = read_le<std::int16_t>(in);
                missing = missing_i16(v, config.revision_year);
                raw = static_cast<double>(v);
            } else if (config.data_format == DataFormat::Binary32) {
                const auto v = read_le<std::int32_t>(in);
                missing = v == std::numeric_limits<std::int32_t>::min();
                raw = static_cast<double>(v);
            } else {
                const float v = read_le<float>(in);
                raw = static_cast<double>(v);
                missing = !std::isfinite(raw) || v == std::numeric_limits<float>::lowest();
            }
            frame.analog.push_back(missing ? std::numeric_limits<double>::quiet_NaN()
                                           : scaled(config.analog_channels[i], raw));
        }

        frame.status.assign(status_count, false);
        for (std::size_t word = 0; word < status_words; ++word) {
            const auto bits = read_le<std::uint16_t>(in);
            for (std::size_t bit = 0; bit < 16; ++bit) {
                const std::size_t index = word * 16 + bit;
                if (index < status_count) frame.status[index] = (bits & (1u << bit)) != 0;
            }
        }
        result.push_back(std::move(frame));
        if (max_frames != 0 && result.size() >= max_frames) break;
    }
    return result;
}

} // namespace ardirec::comtrade
