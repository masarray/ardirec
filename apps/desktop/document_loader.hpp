// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/indexed_dat.hpp"
#include "ardirec/comtrade/record.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct LoadedDocumentData {
    ardirec::comtrade::FileBundle bundle;
    ardirec::comtrade::RecordConfig config;
    std::shared_ptr<ardirec::comtrade::IndexedDatFile> dat;
    std::shared_ptr<const std::vector<double>> time_seconds;
    std::vector<double> channel_peaks;
    std::vector<std::uint8_t> status_active;
    std::vector<double> digital_edge_times;
    std::vector<std::string> diagnostics;
    std::string header_text;
    std::string error;
    bool cancelled{false};
};

[[nodiscard]] std::shared_ptr<LoadedDocumentData>
loadDocumentData(const std::filesystem::path& cfgPath,
                 const std::shared_ptr<std::atomic_bool>& cancel);
