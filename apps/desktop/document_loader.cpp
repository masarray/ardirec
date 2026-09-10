// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_loader.hpp"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>

namespace {

std::string join_diagnostics(const std::vector<std::string>& diagnostics) {
    std::ostringstream out;
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i != 0) out << "; ";
        out << diagnostics[i];
    }
    return out.str();
}

std::string read_text_file_bounded(const std::filesystem::path& path,
                                   std::size_t maximumBytes = 4u * 1024u * 1024u) {
    if (path.empty()) return {};
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    std::string text;
    text.reserve(std::min<std::size_t>(maximumBytes, 64u * 1024u));
    std::array<char, 16u * 1024u> buffer{};
    while (stream && text.size() < maximumBytes) {
        const std::size_t remaining = maximumBytes - text.size();
        const std::size_t requested = std::min<std::size_t>(remaining, buffer.size());
        stream.read(buffer.data(), static_cast<std::streamsize>(requested));
        const auto count = stream.gcount();
        if (count <= 0) break;
        text.append(buffer.data(), static_cast<std::size_t>(count));
    }
    return text;
}

} // namespace

std::shared_ptr<LoadedDocumentData>
loadDocumentData(const std::filesystem::path& cfgPath,
                 const std::shared_ptr<std::atomic_bool>& cancel) {
    auto result = std::make_shared<LoadedDocumentData>();
    try {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }

        result->bundle = ardirec::comtrade::locate_bundle(cfgPath);
        if (result->bundle.cfg.empty()) {
            result->error = "Cannot locate CFG file";
            return result;
        }
        if (result->bundle.dat.empty()) {
            result->error = "Matching DAT file was not found next to CFG";
            return result;
        }

        result->config = ardirec::comtrade::ConfigParser{}.parse_file(result->bundle.cfg);
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }

        auto opened = ardirec::comtrade::IndexedDatFile::open(result->config, result->bundle.dat);
        result->diagnostics = std::move(opened.diagnostics);
        result->dat = std::move(opened.file);
        if (!result->dat) {
            result->error = result->diagnostics.empty()
                                ? "DAT could not be indexed"
                                : join_diagnostics(result->diagnostics);
            return result;
        }

        const double timeScaleSeconds = result->config.time_multiplier * 1.0e-6;
        auto index = result->dat->buildIndex(timeScaleSeconds, cancel.get());
        if (index.cancelled || (cancel && cancel->load(std::memory_order_relaxed))) {
            result->cancelled = true;
            return result;
        }
        if (index.time_seconds.empty()) {
            result->error = "DAT contains no readable sample frames";
            return result;
        }

        if (index.time_seconds.size() != result->dat->frameCount()) {
            result->diagnostics.emplace_back(
                "DAT index stopped before the expected frame count; valid prefix was retained.");
        }
        if (!std::is_sorted(index.time_seconds.begin(), index.time_seconds.end())) {
            result->diagnostics.emplace_back(
                "DAT timestamps are not monotonic; cursor/time lookup may be limited for this record.");
        }

        result->time_seconds = std::make_shared<const std::vector<double>>(std::move(index.time_seconds));
        result->channel_peaks = std::move(index.analog_abs_peaks);
        result->status_active = std::move(index.status_active);
        result->digital_edge_times = std::move(index.digital_edge_times);

        if (!result->bundle.hdr.empty()) result->header_text = read_text_file_bounded(result->bundle.hdr);
    } catch (const std::exception& ex) {
        result->error = ex.what();
    } catch (...) {
        result->error = "Unknown error while loading COMTRADE record";
    }
    return result;
}
