// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_loader.hpp"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/channel_semantics.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr std::uintmax_t kMaximumCfgBytes = 32u * 1024u * 1024u;
constexpr std::uintmax_t kMaximumHeaderPreviewBytes = 4u * 1024u * 1024u;
constexpr std::string_view kNormalMmapDiagnostic = "DAT access: read-only memory map.";
constexpr std::size_t kMaximumFrequencySamples = 8192u;
constexpr double kPi = 3.141592653589793238462643383279502884;

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

std::optional<double> time_of_day_seconds(const std::string& raw) {
    const auto comma = raw.find(',');
    if (comma == std::string::npos || comma + 1u >= raw.size()) return std::nullopt;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
    if (std::sscanf(raw.c_str() + static_cast<std::ptrdiff_t>(comma + 1u), "%d:%d:%lf",
                    &hour, &minute, &second) != 3) {
        return std::nullopt;
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59
        || !std::isfinite(second) || second < 0.0 || second >= 60.0) {
        return std::nullopt;
    }
    return static_cast<double>(hour * 3600 + minute * 60) + second;
}

std::optional<double> trigger_relative_seconds(const ardirec::comtrade::RecordConfig& config,
                                               double recordDuration) {
    const auto start = time_of_day_seconds(config.start_time.raw);
    const auto trigger = time_of_day_seconds(config.trigger_time.raw);
    if (!start || !trigger || !std::isfinite(recordDuration) || recordDuration <= 0.0) return std::nullopt;
    double delta = *trigger - *start;
    if (delta < -0.5) delta += 24.0 * 3600.0;
    if (delta < -1.0e-6 || delta > recordDuration + 1.0e-3) return std::nullopt;
    return std::clamp(delta, 0.0, recordDuration);
}

struct PhaseTriplet final {
    std::array<int, 3> channel{{-1, -1, -1}};
    const char* provenance{nullptr};

    [[nodiscard]] bool complete() const noexcept {
        return channel[0] >= 0 && channel[1] >= 0 && channel[2] >= 0;
    }
};

PhaseTriplet phase_triplet(const ardirec::comtrade::RecordConfig& config,
                           ardirec::comtrade::AnalogRole wantedRole,
                           const char* provenance) {
    PhaseTriplet result;
    result.provenance = provenance;
    for (std::size_t index = 0; index < config.analog_channels.size(); ++index) {
        const auto& definition = config.analog_channels[index];
        if (ardirec::comtrade::analog_role(definition) != wantedRole) continue;
        int slot = -1;
        switch (ardirec::comtrade::phase_role(definition)) {
        case ardirec::comtrade::PhaseRole::L1: slot = 0; break;
        case ardirec::comtrade::PhaseRole::L2: slot = 1; break;
        case ardirec::comtrade::PhaseRole::L3: slot = 2; break;
        default: break;
        }
        if (slot >= 0 && result.channel[static_cast<std::size_t>(slot)] < 0) {
            result.channel[static_cast<std::size_t>(slot)] = static_cast<int>(index);
        }
    }
    return result;
}

struct FrequencyEstimate final {
    bool valid{false};
    double hz{0.0};
    std::string provenance;
};

FrequencyEstimate estimate_triplet_frequency(
    const ardirec::comtrade::IndexedDatFile& data,
    const std::vector<double>& times,
    const PhaseTriplet& triplet,
    double windowStart,
    double windowEnd,
    double nominalFrequency,
    const std::shared_ptr<std::atomic_bool>& cancel) {
    FrequencyEstimate result;
    if (!triplet.complete() || times.size() < 8 || !(windowEnd > windowStart)
        || !(nominalFrequency > 1.0)) {
        return result;
    }

    const auto beginIt = std::lower_bound(times.begin(), times.end(), windowStart);
    const auto endIt = std::upper_bound(times.begin(), times.end(), windowEnd);
    const std::size_t begin = static_cast<std::size_t>(std::distance(times.begin(), beginIt));
    const std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    if (end <= begin + 7u) return result;

    const std::size_t available = end - begin;
    const std::size_t stride = std::max<std::size_t>(1u,
        (available + kMaximumFrequencySamples - 1u) / kMaximumFrequencySamples);
    const std::complex<double> a{-0.5, std::sqrt(3.0) * 0.5};
    const std::complex<double> a2 = a * a;

    struct Point final { double time; double angle; double magnitude; };
    std::vector<Point> points;
    points.reserve(std::min<std::size_t>(available, kMaximumFrequencySamples));
    double maximumMagnitude = 0.0;
    for (std::size_t index = begin; index < end; index += stride) {
        if (cancel && (points.size() & 255u) == 0u && cancel->load(std::memory_order_relaxed)) return {};
        const double l1 = data.analogValue(index, static_cast<std::size_t>(triplet.channel[0]));
        const double l2 = data.analogValue(index, static_cast<std::size_t>(triplet.channel[1]));
        const double l3 = data.analogValue(index, static_cast<std::size_t>(triplet.channel[2]));
        if (!std::isfinite(l1) || !std::isfinite(l2) || !std::isfinite(l3)) continue;
        const std::complex<double> space = (2.0 / 3.0) * (std::complex<double>{l1, 0.0}
                                                   + a * l2 + a2 * l3);
        const double magnitude = std::abs(space);
        if (!std::isfinite(magnitude)) continue;
        maximumMagnitude = std::max(maximumMagnitude, magnitude);
        points.push_back({times[index], std::atan2(space.imag(), space.real()), magnitude});
    }
    if (points.size() < 8 || !(maximumMagnitude > 1.0e-12)) return result;

    const double magnitudeFloor = maximumMagnitude * 0.05;
    std::vector<std::pair<double, double>> unwrapped;
    unwrapped.reserve(points.size());
    bool haveAngle = false;
    double previousRaw = 0.0;
    double running = 0.0;
    for (const auto& point : points) {
        if (point.magnitude < magnitudeFloor) continue;
        if (!haveAngle) {
            previousRaw = point.angle;
            running = point.angle;
            haveAngle = true;
        } else {
            running += std::remainder(point.angle - previousRaw, 2.0 * kPi);
            previousRaw = point.angle;
        }
        unwrapped.emplace_back(point.time, running);
    }
    if (unwrapped.size() < 8) return result;

    const double span = unwrapped.back().first - unwrapped.front().first;
    if (!(span >= 2.0 / nominalFrequency)) return result;

    long double sumT = 0.0L;
    long double sumA = 0.0L;
    long double sumTT = 0.0L;
    long double sumTA = 0.0L;
    const double origin = unwrapped.front().first;
    for (const auto& [time, angle] : unwrapped) {
        const long double t = static_cast<long double>(time - origin);
        const long double y = static_cast<long double>(angle);
        sumT += t;
        sumA += y;
        sumTT += t * t;
        sumTA += t * y;
    }
    const long double n = static_cast<long double>(unwrapped.size());
    const long double denominator = n * sumTT - sumT * sumT;
    if (std::abs(denominator) <= 1.0e-18L) return result;
    const long double slope = (n * sumTA - sumT * sumA) / denominator;
    const long double intercept = (sumA - slope * sumT) / n;
    const double frequency = std::abs(static_cast<double>(slope)) / (2.0 * kPi);
    if (!std::isfinite(frequency)
        || frequency < nominalFrequency * 0.80
        || frequency > nominalFrequency * 1.20) {
        return result;
    }

    long double squaredResidual = 0.0L;
    for (const auto& [time, angle] : unwrapped) {
        const long double t = static_cast<long double>(time - origin);
        const long double residual = static_cast<long double>(angle) - (intercept + slope * t);
        squaredResidual += residual * residual;
    }
    const double rmsResidual = std::sqrt(static_cast<double>(squaredResidual / n));
    if (!std::isfinite(rmsResidual) || rmsResidual > 0.20) return result;

    result.valid = true;
    result.hz = frequency;
    result.provenance = triplet.provenance ? triplet.provenance : "PREFault estimated";
    return result;
}

FrequencyEstimate estimate_prefault_frequency(
    const ardirec::comtrade::RecordConfig& config,
    const ardirec::comtrade::IndexedDatFile& data,
    const std::vector<double>& times,
    const std::shared_ptr<std::atomic_bool>& cancel) {
    FrequencyEstimate result;
    const double nominal = config.nominal_frequency > 1.0 ? config.nominal_frequency : 50.0;
    if (times.size() < 8) return result;
    const double duration = times.back() - times.front();
    const auto triggerRelative = trigger_relative_seconds(config, duration);
    if (!triggerRelative) return result;

    const double period = 1.0 / nominal;
    const double triggerTime = times.front() + *triggerRelative;
    const double windowEnd = triggerTime - 0.25 * period;
    const double windowStart = std::max(times.front(), windowEnd - 6.0 * period);
    if (!(windowEnd - windowStart >= 2.0 * period)) return result;

    const PhaseTriplet voltage = phase_triplet(config, ardirec::comtrade::AnalogRole::Voltage,
                                                "PREFault estimated · voltage V1");
    result = estimate_triplet_frequency(data, times, voltage, windowStart, windowEnd, nominal, cancel);
    if (result.valid) return result;

    const PhaseTriplet current = phase_triplet(config, ardirec::comtrade::AnalogRole::Current,
                                                "PREFault estimated · current I1");
    return estimate_triplet_frequency(data, times, current, windowStart, windowEnd, nominal, cancel);
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

        const double nominal = result->config.nominal_frequency > 1.0
                                   ? result->config.nominal_frequency : 50.0;
        result->calculation_frequency_hz = nominal;
        result->calculation_frequency_provenance = "COMTRADE nominal";
        const auto estimated = estimate_prefault_frequency(result->config, *result->dat,
                                                            index.time_seconds, cancel);
        if (estimated.valid) {
            result->calculation_frequency_hz = estimated.hz;
            result->calculation_frequency_provenance = estimated.provenance;
        }
        {
            std::ostringstream diagnostic;
            diagnostic.setf(std::ios::fixed);
            diagnostic.precision(4);
            diagnostic << "Calculation frequency: " << result->calculation_frequency_hz
                       << " Hz (" << result->calculation_frequency_provenance << ").";
            result->diagnostics.push_back(diagnostic.str());
        }

        if (cancel && cancel->load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }

        // Build all record-sized visual indexing in this worker. Level 0 remains
        // the single-pass extrema cache; coarser levels are derived without raw
        // DAT traversal and let zoomed-out geometry select the nearest resolution.
        result->analog_lod = index.analog_lod;
        result->analog_lod_pyramid = ardirec::desktop::buildAnalogLodPyramid(
            result->analog_lod, cancel.get());
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            result->cancelled = true;
            return result;
        }

        // Publish the immutable base visual cache before this DAT source can reach
        // the UI/render thread. Engineering calculations still read raw data.
        result->dat->publishAnalogLod(result->analog_lod);
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
