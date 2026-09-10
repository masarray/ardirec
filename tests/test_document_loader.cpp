// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_loader.hpp"

#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "temporary loader fixture opens");
    out << text;
    require(static_cast<bool>(out), "temporary loader fixture writes");
}
} // namespace

int main() {
    try {
        const auto dir = std::filesystem::temp_directory_path() / "ardirec_loader_robustness";
        std::error_code ignored;
        std::filesystem::create_directories(dir, ignored);
        const auto cfgPath = dir / "nonmonotonic.cfg";
        const auto datPath = dir / "nonmonotonic.dat";

        write_text(cfgPath,
                   "FIELD LOADER,RECORDER,1999\n"
                   "1,1A,0D\n"
                   "1,VA,A,LINE,V,1,0,0,-1000,1000,1,1,S\n"
                   "50\n"
                   "1\n"
                   "1000,4\n"
                   "01/01/2020,00:00:00.000000\n"
                   "01/01/2020,00:00:00.001000\n"
                   "ASCII\n"
                   "1\n");
        write_text(datPath,
                   "1,0,1\n"
                   "2,1000,2\n"
                   "3,500,3\n"
                   "4,2000,4\n");

        auto cancel = std::make_shared<std::atomic_bool>(false);
        const auto loaded = loadDocumentData(cfgPath, cancel);
        require(loaded != nullptr, "loader returns a result object");
        require(loaded->error.empty(), "non-monotonic tail is recoverable");
        require(loaded->time_seconds != nullptr, "safe time index is produced");
        require(loaded->time_seconds->size() == 2,
                "time index is truncated before the first backwards timestamp");
        require(std::abs((*loaded->time_seconds)[0] - 0.0) < 1e-12,
                "first safe timestamp retained");
        require(std::abs((*loaded->time_seconds)[1] - 0.001) < 1e-12,
                "last monotonic timestamp retained");
        bool foundDiagnostic = false;
        for (const auto& diagnostic : loaded->diagnostics) {
            if (diagnostic.find("non-monotonic") != std::string::npos) {
                foundDiagnostic = true;
                break;
            }
        }
        require(foundDiagnostic, "time-prefix salvage is diagnosed");

        std::filesystem::remove(cfgPath, ignored);
        std::filesystem::remove(datPath, ignored);
        std::filesystem::remove(dir, ignored);

        std::cout << "ardirec document loader tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec document loader tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
