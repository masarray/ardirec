// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_loader.hpp"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uintmax_t kCfgLimit = 32u * 1024u * 1024u;
constexpr std::size_t kHeaderLimit = 4u * 1024u * 1024u;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "temporary loader safety text file opens");
    out << text;
    require(static_cast<bool>(out), "temporary loader safety text file writes");
}

bool contains_diagnostic(const std::vector<std::string>& diagnostics,
                         const std::string& needle) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.find(needle) != std::string::npos) return true;
    }
    return false;
}
} // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "ardirec_loader_file_safety";
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        std::filesystem::create_directories(directory, ignored);
        require(!ignored, "temporary loader safety directory creates");

        const auto cfgPath = directory / "safety.cfg";
        const auto datPath = directory / "safety.dat";
        const auto hdrPath = directory / "safety.hdr";
        auto cancel = std::make_shared<std::atomic_bool>(false);

        // A pathological CFG must be rejected before line-oriented parsing can
        // allocate proportionally to attacker/corruption-controlled metadata.
        {
            std::ofstream cfg(cfgPath, std::ios::binary | std::ios::trunc);
            require(static_cast<bool>(cfg), "oversized CFG fixture opens");
            cfg.seekp(static_cast<std::streamoff>(kCfgLimit));
            cfg.put('X');
            require(static_cast<bool>(cfg), "oversized CFG fixture writes");
        }
        write_text(datPath, "1,0,1\n");
        const auto oversized = loadDocumentData(cfgPath, cancel);
        require(oversized != nullptr, "oversized CFG returns controlled result");
        require(oversized->error.find("32 MiB safety limit") != std::string::npos,
                "oversized CFG is rejected by bounded metadata guard");

        write_text(cfgPath,
                   "LOADER SAFETY,RECORDER,1999\n"
                   "1,1A,0D\n"
                   "1,A1,A,LINE,V,1,0,0,-1000,1000,1,1,S\n"
                   "50\n"
                   "1\n"
                   "1000,2\n"
                   "01/01/2020,00:00:00.000000\n"
                   "01/01/2020,00:00:00.001000\n"
                   "ASCII\n"
                   "1\n");
        write_text(datPath, "1,0,1\n2,1000,2\n");

        // HDR is optional presentation metadata. Oversized text is retained as
        // a bounded preview and diagnosed instead of allocating the whole file.
        {
            std::ofstream hdr(hdrPath, std::ios::binary | std::ios::trunc);
            require(static_cast<bool>(hdr), "oversized HDR fixture opens");
            const std::string block(64u * 1024u, 'H');
            std::size_t written = 0;
            const std::size_t target = kHeaderLimit + 128u * 1024u;
            while (written < target) {
                const std::size_t count = std::min(block.size(), target - written);
                hdr.write(block.data(), static_cast<std::streamsize>(count));
                written += count;
            }
            require(static_cast<bool>(hdr), "oversized HDR fixture writes");
        }

        cancel = std::make_shared<std::atomic_bool>(false);
        const auto boundedHeader = loadDocumentData(cfgPath, cancel);
        require(boundedHeader != nullptr, "bounded HDR record returns result");
        require(boundedHeader->error.empty(), "oversized HDR does not invalidate COMTRADE data");
        require(boundedHeader->header_text.size() == kHeaderLimit,
                "HDR preview is capped at exactly 4 MiB");
        require(contains_diagnostic(boundedHeader->diagnostics, "HDR preview truncated to 4 MiB"),
                "HDR preview truncation is surfaced diagnostically");

        std::filesystem::remove_all(directory, ignored);
        std::cout << "ardirec loader file safety tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec loader file safety tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
