// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_loader.hpp"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>

namespace {

constexpr std::uintmax_t kMaximumCfgBytes = 32u * 1024u * 1024u;
constexpr std::uintmax_t kMaximumHeaderPreviewBytes = 4u * 1024u * 1024u;
constexpr std::string_view kNormalMmapDiagnostic = "DAT access: read-only memory map.";

std::string join_diagnostics(const std::vector<std::string>& diagnostics) {
    std::ostringstream out;
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
        if (i != 0) out << "; ";
        out << diagnostics[i];
    }
    return out.str();
}

void append_diagnostics(std::vector<std::string>& destination,
                        const std::vector<std::string>& source) {
    destination.insert(destination.end(), source.begin(), source.end());
}

void append_operator_dat_diagnostics(std::vector<std::string>& destination,
                                     const std::vector<std::string>& source) {
    for (const auto& diagnostic : source) {
        if (diagnostic == kNormalMmapDiagnostic) continue;
        destination.push_back(diagnostic);
    }
}

bool validate_regular_file(const std::filesystem::path& path,
                           const char* label,
                           std::string& error) {
    std::error_code ec;
    const bool regular = std::filesystem::is_regular_file(path, ec);
    if (ec) {
        error = std::string("Cannot inspect ") + label + " file: " + ec.message();
        return false;
    }
    if (!regular) {
        error = std::string(label) + " path is not a regular file";
        return false;
    }
    return true;
}

bool validate_cfg_size(const std::filesystem::path& path, std::string& error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        error = "Cannot stat CFG file: " + ec.message();
        return false;
    }
    if (size > kMaximumCfgBytes) {
        error = "CFG exceeds 32 MiB safety limit; refusing unbounded metadata parsing";
        return false;
    }
    return true;
}

std::string read_text_file_bounded(const std::filesystem::path& path,
                                   std::size_t maximumBytes = static_cast<std::size_t>(kMaximumHeaderPreviewBytes)) {
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

void retain_safe_time_prefix(ardirec::comtrade::DatIndexSummary& index,
                             std::vector<std::string>& diagnostics) {
    if (index.time_seconds.empty()) return;

    auto invalid = std::find_if(index.time_seconds.begin(), index.time_seconds.end(),
                                [](double value) { return !std::isfinite(value); });
    std::size_t validCount = invalid == index.time_seconds.end()
                                 ? index.time_seconds.size()
                                 : static_cast<std::size_t>(std::distance(index.time_seconds.begin(), invalid));
    if (invalid != index.time_seconds.end()) {
        diagnostics.emplace_back("DAT timestamp became non-finite; later frames were excluded from time-indexed analysis.");
    }

    for (std::size_t i = 1; i < validCount; ++i) {
        if (index.time_seconds[i] < index.time_seconds[i - 1]) {
            validCount = i;
            diagnostics.emplace_back(
                "DAT timestamps became non-monotonic; the valid monotonic prefix was retained for safe cursor/zoom lookup.");
            break;
        }
    }

    if (validCount < index.time_seconds.size()) index.time_seconds.resize(validCount);
    if (index.time_seconds.empty()) {
        index.digital_edge_times.clear();
        return;
    }

    const double lastTime = index.time_seconds.back();
    index.digital_edge_times.erase(
        std::upper_bound(index.digital_edge_times.begin(), index.digital_edge_times.end(), lastTime),
        index.digital_edge_times.end());
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
        if (!validate_regular_file(result->bundle.cfg, "CFG", result->error)
            || !validate_cfg_size(result->bundle.cfg, result->error)
            || !validate_regular_file(result->bundle.dat, "DAT", result->error)) {
            return result;
        }

        auto parsed = ardirec::comtrade::ConfigParser{}.try_parse_file(result->bundle.cfg);
        if (!parsed) {
            result->error = parsed.error.empty() ? "CFG could not be parsed" : std::move(parsed.error);
            return result;
        }
        result->config = std::move(*parsed.config);
        append_diagnostics(result->diagnostics, result->config.diagnostics);

        if (cancel && cancel->load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }

        auto opened = ardirec::comtrade::IndexedDatFile::open(result->config, result->bundle.dat);
        append_operator_dat_diagnostics(result->diagnostics, opened.diagnostics);
        result->dat = std::move(opened.file);
        if (!result->dat) {
            result->error = result->diagnostics.empty()
                                ? "DAT could not be indexed"
                                : join_diagnostics(result->diagnostics);
            return result;
        }

        const double timeScaleSeconds = result->config.time_multiplier * 1.0e-6;
        auto index = result->dat->buildIndex(timeScaleSeconds, cancel.get());
        append_diagnostics(result->diagnostics, index.diagnostics);
        if (index.cancelled || (cancel && cancel->load(std::memory_order_relaxed))) {
            result->cancelled = true;
            return result;
        }
        if (index.time_seconds.empty()) {
            result->error = "DAT contains no readable sample frames";
            return result;
        }

        retain_safe_time_prefix(index, result->diagnostics);
        if (index.time_seconds.empty()) {
            result->error = "DAT contains no safe monotonic timestamp prefix";
            return result;
        }

        if (index.time_seconds.size() != result->dat->frameCount()) {
            result->diagnostics.emplace_back(
                "DAT time index covers a valid prefix rather than the full physical frame count.");
        }

        result->time_seconds = std::make_shared<const std::vector<double>>(std::move(index.time_seconds));
        result->channel_peaks = std::move(index.analog_abs_peaks);
        result->status_active = std::move(index.status_active);
        result->digital_edge_times = std::move(index.digital_edge_times);

        if (!result->bundle.hdr.empty()) {
            result->header_text = read_text_file_bounded(result->bundle.hdr);
            std::error_code headerSizeError;
            const auto headerSize = std::filesystem::file_size(result->bundle.hdr, headerSizeError);
            if (!headerSizeError && headerSize > kMaximumHeaderPreviewBytes) {
                result->diagnostics.emplace_back("HDR preview truncated to 4 MiB to keep UI memory bounded.");
            }
        }
    } catch (const std::exception& ex) {
        result->error = std::string("Unexpected background load failure: ") + ex.what();
    } catch (...) {
        result->error = "Unknown error while loading COMTRADE record";
    }
    return result;
}
