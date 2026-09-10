// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/indexed_dat.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
    std::uint64_t samples{10'000'000};
    std::size_t analog{6};
    std::size_t digital{8};
    std::uint32_t sampleRate{10'000};
    bool keepFixture{false};
};

bool parse_u64(std::string_view text, std::uint64_t& value) {
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_size(std::string_view text, std::size_t& value) {
    std::uint64_t wide = 0;
    if (!parse_u64(text, wide) || wide > std::numeric_limits<std::size_t>::max()) return false;
    value = static_cast<std::size_t>(wide);
    return true;
}

bool parse_u32(std::string_view text, std::uint32_t& value) {
    std::uint64_t wide = 0;
    if (!parse_u64(text, wide) || wide > std::numeric_limits<std::uint32_t>::max()) return false;
    value = static_cast<std::uint32_t>(wide);
    return true;
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--keep") {
            options.keepFixture = true;
            continue;
        }
        if (i + 1 >= argc) return false;
        const std::string_view value(argv[++i]);
        if (arg == "--samples") {
            if (!parse_u64(value, options.samples)) return false;
        } else if (arg == "--analog") {
            if (!parse_size(value, options.analog)) return false;
        } else if (arg == "--digital") {
            if (!parse_size(value, options.digital)) return false;
        } else if (arg == "--sample-rate") {
            if (!parse_u32(value, options.sampleRate)) return false;
        } else {
            return false;
        }
    }
    return options.samples > 1 && options.analog > 0 && options.sampleRate > 0
           && options.analog <= 4096 && options.digital <= 4096;
}

void store_u16_le(std::byte* out, std::uint16_t value) noexcept {
    out[0] = static_cast<std::byte>(value & 0xffu);
    out[1] = static_cast<std::byte>((value >> 8u) & 0xffu);
}

void store_i16_le(std::byte* out, std::int16_t value) noexcept {
    store_u16_le(out, static_cast<std::uint16_t>(value));
}

void store_u32_le(std::byte* out, std::uint32_t value) noexcept {
    out[0] = static_cast<std::byte>(value & 0xffu);
    out[1] = static_cast<std::byte>((value >> 8u) & 0xffu);
    out[2] = static_cast<std::byte>((value >> 16u) & 0xffu);
    out[3] = static_cast<std::byte>((value >> 24u) & 0xffu);
}

bool write_cfg(const std::filesystem::path& path, const Options& options) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    const std::size_t total = options.analog + options.digital;
    out << "ARDIREC LARGE RECORD BENCH,BENCH,1999\n";
    out << total << ',' << options.analog << "A," << options.digital << "D\n";
    for (std::size_t channel = 0; channel < options.analog; ++channel) {
        out << (channel + 1) << ",A" << (channel + 1)
            << ",,BENCH,V,1,0,0,-32767,32767,1,1,S\n";
    }
    for (std::size_t channel = 0; channel < options.digital; ++channel) {
        out << (channel + 1) << ",D" << (channel + 1) << ",,BENCH,0\n";
    }
    out << "50\n1\n";
    out << options.sampleRate << ',' << options.samples << "\n";
    out << "01/01/2020,00:00:00.000000\n";
    out << "01/01/2020,00:00:00.001000\n";
    out << "BINARY\n1\n";
    return static_cast<bool>(out);
}

bool write_dat(const std::filesystem::path& path, const Options& options) {
    const std::size_t statusWords = (options.digital + 15u) / 16u;
    const std::size_t frameSize = 8u + options.analog * sizeof(std::int16_t)
                                  + statusWords * sizeof(std::uint16_t);
    constexpr std::size_t chunkFrames = 8192;
    std::vector<std::byte> buffer(frameSize * chunkFrames);

    const long double finalTimestamp = (static_cast<long double>(options.samples - 1u) * 1'000'000.0L)
                                       / static_cast<long double>(options.sampleRate);
    if (finalTimestamp > static_cast<long double>(std::numeric_limits<std::uint32_t>::max())) {
        std::cerr << "benchmark fixture exceeds COMTRADE 32-bit timestamp range\n";
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    const std::uint64_t digitalPeriod = std::max<std::uint64_t>(1u, options.samples / 4u);
    std::uint64_t frameBase = 0;
    while (frameBase < options.samples) {
        const std::size_t framesThisChunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(chunkFrames, options.samples - frameBase));
        for (std::size_t local = 0; local < framesThisChunk; ++local) {
            const std::uint64_t frameIndex = frameBase + local;
            std::byte* frame = buffer.data() + local * frameSize;
            store_u32_le(frame, static_cast<std::uint32_t>((frameIndex + 1u) & 0xffffffffu));
            const auto timestamp = static_cast<std::uint32_t>(
                (static_cast<long double>(frameIndex) * 1'000'000.0L)
                / static_cast<long double>(options.sampleRate));
            store_u32_le(frame + 4u, timestamp);

            std::byte* analogBase = frame + 8u;
            for (std::size_t channel = 0; channel < options.analog; ++channel) {
                const std::uint32_t phase = static_cast<std::uint32_t>((frameIndex + channel * 97u) % 2000u);
                const std::int16_t sample = static_cast<std::int16_t>(static_cast<int>(phase) - 1000);
                store_i16_le(analogBase + channel * sizeof(std::int16_t), sample);
            }

            std::byte* statusBase = analogBase + options.analog * sizeof(std::int16_t);
            for (std::size_t word = 0; word < statusWords; ++word) {
                std::uint16_t packed = 0;
                if (((frameIndex / digitalPeriod) & 1u) != 0u) packed = 0x0001u;
                store_u16_le(statusBase + word * sizeof(std::uint16_t), packed);
            }
        }
        out.write(reinterpret_cast<const char*>(buffer.data()),
                  static_cast<std::streamsize>(framesThisChunk * frameSize));
        if (!out) return false;
        frameBase += framesThisChunk;
    }
    return true;
}

using Clock = std::chrono::steady_clock;

double seconds_between(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        std::cerr << "usage: ardirec-large-record-benchmark [--samples N] [--analog N] [--digital N] "
                     "[--sample-rate N] [--keep]\n";
        return 2;
    }

    const auto nonce = static_cast<unsigned long long>(Clock::now().time_since_epoch().count());
    const auto directory = std::filesystem::temp_directory_path()
                           / ("ardirec_large_record_" + std::to_string(nonce));
    const auto cfgPath = directory / "large.cfg";
    const auto datPath = directory / "large.dat";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        std::cerr << "cannot create benchmark directory: " << ec.message() << '\n';
        return 1;
    }

    const auto generationStart = Clock::now();
    if (!write_cfg(cfgPath, options) || !write_dat(datPath, options)) {
        std::cerr << "failed to generate benchmark fixture\n";
        std::filesystem::remove_all(directory, ec);
        return 1;
    }
    const auto generationEnd = Clock::now();

    const auto parseStart = Clock::now();
    const auto parsed = ardirec::comtrade::ConfigParser{}.try_parse_file(cfgPath);
    if (!parsed) {
        std::cerr << "CFG parse failed: " << parsed.error << '\n';
        if (!options.keepFixture) std::filesystem::remove_all(directory, ec);
        return 1;
    }
    auto opened = ardirec::comtrade::IndexedDatFile::open(*parsed.config, datPath);
    if (!opened.file || opened.file->frameCount() != options.samples) {
        std::cerr << "DAT open/frame count validation failed\n";
        if (!options.keepFixture) std::filesystem::remove_all(directory, ec);
        return 1;
    }
    const auto parseEnd = Clock::now();

    const auto indexStart = Clock::now();
    auto summary = opened.file->buildIndex(parsed.config->time_multiplier * 1.0e-6);
    const auto indexEnd = Clock::now();
    if (summary.cancelled || summary.time_seconds.size() != options.samples
        || summary.analog_abs_peaks.size() != options.analog
        || summary.status_active.size() != options.digital) {
        std::cerr << "index validation failed\n";
        if (!options.keepFixture) std::filesystem::remove_all(directory, ec);
        return 1;
    }

    const auto randomStart = Clock::now();
    long double checksum = 0.0L;
    const std::uint64_t stride = std::max<std::uint64_t>(1u, options.samples / 4096u);
    for (std::uint64_t frame = 0; frame < options.samples; frame += stride) {
        const double value = opened.file->analogValue(static_cast<std::size_t>(frame), 0u);
        if (std::isfinite(value)) checksum += value;
    }
    const auto randomEnd = Clock::now();

    const auto fileBytes = std::filesystem::file_size(datPath, ec);
    if (ec) {
        std::cerr << "cannot stat benchmark DAT: " << ec.message() << '\n';
        if (!options.keepFixture) std::filesystem::remove_all(directory, ec);
        return 1;
    }

    const std::uint64_t compactIndexBytes =
        static_cast<std::uint64_t>(summary.time_seconds.capacity()) * sizeof(double)
        + static_cast<std::uint64_t>(summary.analog_abs_peaks.capacity()) * sizeof(double)
        + static_cast<std::uint64_t>(summary.status_active.capacity()) * sizeof(std::uint8_t)
        + static_cast<std::uint64_t>(summary.digital_edge_times.capacity()) * sizeof(double);

    const double generationSeconds = seconds_between(generationStart, generationEnd);
    const double openSeconds = seconds_between(parseStart, parseEnd);
    const double indexSeconds = seconds_between(indexStart, indexEnd);
    const double randomSeconds = seconds_between(randomStart, randomEnd);
    const double mib = static_cast<double>(fileBytes) / (1024.0 * 1024.0);
    const double compactMib = static_cast<double>(compactIndexBytes) / (1024.0 * 1024.0);
    const double throughput = indexSeconds > 0.0
                                  ? static_cast<double>(options.samples) / indexSeconds / 1'000'000.0
                                  : 0.0;

    std::cout << std::fixed << std::setprecision(3)
              << "ardirec large-record benchmark\n"
              << "samples=" << options.samples
              << " analog=" << options.analog
              << " digital=" << options.digital
              << " sample_rate_hz=" << options.sampleRate << '\n'
              << "dat_mib=" << mib
              << " access=" << (opened.file->memoryMapped() ? "mmap" : "stream-fallback") << '\n'
              << "generate_s=" << generationSeconds
              << " open_s=" << openSeconds
              << " index_s=" << indexSeconds
              << " index_mframes_s=" << throughput
              << " random_4k_s=" << randomSeconds << '\n'
              << "compact_index_mib=" << compactMib
              << " digital_edges=" << summary.digital_edge_times.size()
              << " diagnostics=" << (opened.diagnostics.size() + summary.diagnostics.size())
              << " checksum=" << static_cast<double>(checksum) << '\n';

    if (options.keepFixture) {
        std::cout << "fixture=" << directory.string() << '\n';
    } else {
        std::filesystem::remove_all(directory, ec);
    }
    return 0;
}
