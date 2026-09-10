// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/indexed_dat.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void store_u16_le(std::byte* out, std::uint16_t value) noexcept {
    out[0] = static_cast<std::byte>(value & 0xffu);
    out[1] = static_cast<std::byte>((value >> 8u) & 0xffu);
}

void store_u32_le(std::byte* out, std::uint32_t value) noexcept {
    out[0] = static_cast<std::byte>(value & 0xffu);
    out[1] = static_cast<std::byte>((value >> 8u) & 0xffu);
    out[2] = static_cast<std::byte>((value >> 16u) & 0xffu);
    out[3] = static_cast<std::byte>((value >> 24u) & 0xffu);
}

void write_cfg(const std::filesystem::path& path,
               int revision,
               const std::string& format) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "temporary binary robustness CFG opens");
    out << "BINARY FIELD ROBUSTNESS,RECORDER," << revision << "\n"
        << "2,1A,1D\n"
        << "1,A1,A,LINE,A,1,0,0,-2147483648,2147483647,1,1,S\n"
        << "1,D1,,BREAKER,0\n"
        << "50\n"
        << "1\n"
        << "1000,1\n"
        << "01/01/2020,00:00:00.000000\n"
        << "01/01/2020,00:00:00.001000\n"
        << format << "\n"
        << "1\n";
    require(static_cast<bool>(out), "temporary binary robustness CFG writes");
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::byte>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "temporary binary robustness DAT opens");
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    require(static_cast<bool>(out), "temporary binary robustness DAT writes");
}

std::vector<std::byte> binary16_frame(std::uint16_t rawAnalog,
                                      std::uint16_t statusWord = 1u) {
    std::vector<std::byte> frame(12u);
    store_u32_le(frame.data(), 1u);
    store_u32_le(frame.data() + 4u, 1000u);
    store_u16_le(frame.data() + 8u, rawAnalog);
    store_u16_le(frame.data() + 10u, statusWord);
    return frame;
}

std::vector<std::byte> binary32_frame(std::uint32_t rawAnalog,
                                      std::uint16_t statusWord = 1u) {
    std::vector<std::byte> frame(14u);
    store_u32_le(frame.data(), 1u);
    store_u32_le(frame.data() + 4u, 1000u);
    store_u32_le(frame.data() + 8u, rawAnalog);
    store_u16_le(frame.data() + 12u, statusWord);
    return frame;
}

std::vector<std::byte> float32_frame(float rawAnalog,
                                     std::uint16_t statusWord = 1u) {
    return binary32_frame(std::bit_cast<std::uint32_t>(rawAnalog), statusWord);
}

void verify_missing_sample(const std::filesystem::path& cfgPath,
                           const std::filesystem::path& datPath,
                           int revision,
                           const std::string& format,
                           const std::vector<std::byte>& frame) {
    write_cfg(cfgPath, revision, format);
    write_bytes(datPath, frame);
    const auto parsed = ardirec::comtrade::ConfigParser{}.try_parse_file(cfgPath);
    require(parsed.ok(), "binary robustness CFG parses");
    const auto opened = ardirec::comtrade::IndexedDatFile::open(*parsed.config, datPath);
    require(opened.file != nullptr, "binary robustness DAT opens");
    require(opened.file->frameCount() == 1u, "binary robustness frame retained");
    require(std::isnan(opened.file->analogValue(0u, 0u)), "missing binary analog maps to NaN");
    require(opened.file->statusState(0u, 0u).has_value(), "complete binary digital state is known");
    require(opened.file->statusValue(0u, 0u), "packed digital state survives missing analog");
    require(std::isnan(opened.file->analogValue(9u, 0u)), "out-of-range analog access is safe");
    require(!opened.file->statusState(9u, 0u).has_value(), "out-of-range digital access is unknown");

    std::atomic_bool cancel{true};
    const auto cancelled = opened.file->buildIndex(1.0e-6, &cancel);
    require(cancelled.cancelled, "index scan honours cancellation before first frame");
}

} // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "ardirec_binary_field_robustness";
        std::error_code ignored;
        // Tests can be interrupted on Windows while an mmap-backed fixture is
        // still alive. Recreate the directory when possible, but use distinct
        // DAT names for later cases so the test never relies on truncating an
        // actively mapped file.
        std::filesystem::remove_all(directory, ignored);
        ignored.clear();
        std::filesystem::create_directories(directory, ignored);
        require(!ignored, "temporary binary robustness directory creates");
        const auto cfgPath = directory / "record.cfg";
        const auto datPath = directory / "record.dat";

        verify_missing_sample(cfgPath, datPath, 2013, "BINARY",
                              binary16_frame(static_cast<std::uint16_t>(0x8000u)));
        verify_missing_sample(cfgPath, datPath, 1991, "BINARY",
                              binary16_frame(static_cast<std::uint16_t>(0xffffu)));
        verify_missing_sample(cfgPath, datPath, 2013, "BINARY32",
                              binary32_frame(static_cast<std::uint32_t>(0x80000000u)));
        verify_missing_sample(cfgPath, datPath, 2013, "FLOAT32",
                              float32_frame(std::numeric_limits<float>::quiet_NaN()));

        // A valid complete frame followed by damaged bytes remains readable.
        write_cfg(cfgPath, 2013, "BINARY");
        const auto truncatedDatPath = directory / "truncated.dat";
        auto truncatedTail = binary16_frame(123u);
        truncatedTail.push_back(static_cast<std::byte>(0x5au));
        write_bytes(truncatedDatPath, truncatedTail);
        const auto parsed = ardirec::comtrade::ConfigParser{}.try_parse_file(cfgPath);
        require(parsed.ok(), "truncated-tail CFG parses");
        const auto salvaged = ardirec::comtrade::IndexedDatFile::open(*parsed.config, truncatedDatPath);
        require(salvaged.file != nullptr, "complete prefix survives damaged binary tail");
        require(salvaged.file->frameCount() == 1u, "damaged binary tail is not exposed as a frame");
        require(!salvaged.diagnostics.empty(), "damaged binary tail is diagnosed");
        require(std::abs(salvaged.file->analogValue(0u, 0u) - 123.0) < 1e-12,
                "complete binary prefix remains numerically valid");

        // A file containing no complete frame is a controlled failure, never a
        // partial decode. Use a separate path so Windows never needs to truncate
        // an mmap-backed file from the previous assertion scope.
        const auto unusableDatPath = directory / "unusable.dat";
        write_bytes(unusableDatPath, std::vector<std::byte>(5u, static_cast<std::byte>(0xa5u)));
        const auto unusable = ardirec::comtrade::IndexedDatFile::open(*parsed.config, unusableDatPath);
        require(unusable.file == nullptr, "binary DAT without complete frame is rejected");
        require(!unusable.diagnostics.empty(), "binary DAT without complete frame reports diagnostics");

        // Best-effort cleanup: an mmap may still be held until local objects
        // unwind on Windows, which is not a test failure and has no production
        // lifecycle implication.
        std::filesystem::remove_all(directory, ignored);
        std::cout << "ardirec binary field robustness tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec binary field robustness tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
