// SPDX-License-Identifier: GPL-3.0-or-later
#include "locus_snapshot_controller.hpp"

#include "ardirec/distance/distance.hpp"
#include "ardirec/power/timestamped_dft.hpp"
#include "distance_compensation.hpp"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::size_t kMaximumAnalysisPoints = 65536u;
constexpr std::array<const char*, 6> kLoopIds{{"L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"}};
constexpr std::array<ardirec::distance::FaultLoop, 6> kLoops{{
    ardirec::distance::FaultLoop::L1E, ardirec::distance::FaultLoop::L2E,
    ardirec::distance::FaultLoop::L3E, ardirec::distance::FaultLoop::L1L2,
    ardirec::distance::FaultLoop::L2L3, ardirec::distance::FaultLoop::L3L1,
}};

QString compact_name(QString value) {
    value = value.trimmed().toUpper();
    QString result;
    result.reserve(value.size());
    for (const QChar ch : value) if (ch.isLetterOrNumber()) result.append(ch);
    return result;
}

QString phase_from_name(const QString& rawName) {
    const QString name = compact_name(rawName);
    if (name.contains(QStringLiteral("L1")) || name.endsWith(QStringLiteral("AN"))
        || name.endsWith(QStringLiteral("IA")) || name.endsWith(QStringLiteral("VA"))
        || name.endsWith(QStringLiteral("UA")) || name == QStringLiteral("A")) return QStringLiteral("L1");
    if (name.contains(QStringLiteral("L2")) || name.endsWith(QStringLiteral("BN"))
        || name.endsWith(QStringLiteral("IB")) || name.endsWith(QStringLiteral("VB"))
        || name.endsWith(QStringLiteral("UB")) || name == QStringLiteral("B")) return QStringLiteral("L2");
    if (name.contains(QStringLiteral("L3")) || name.endsWith(QStringLiteral("CN"))
        || name.endsWith(QStringLiteral("IC")) || name.endsWith(QStringLiteral("VC"))
        || name.endsWith(QStringLiteral("UC")) || name == QStringLiteral("C")) return QStringLiteral("L3");
    return QStringLiteral("Other");
}

// Convert a dedicated residual/earth-current channel to the internal phase-current
// sum convention Ires = IL1 + IL2 + IL3 = 3I0. SIGRA's IE channel uses the opposite
// reference direction, so IE is negated here and converted back only in the classical
// RE/RL-XE/XL solver.
std::optional<double> residual_to_sum_multiplier(const QString& rawName) {
    const QString name = compact_name(rawName);
    if (name.contains(QStringLiteral("3I0")) || name.contains(QStringLiteral("RESIDUAL"))
        || name == QStringLiteral("IRES") || name == QStringLiteral("RES")) return 1.0;
    if (name == QStringLiteral("I0") || name.endsWith(QStringLiteral("I0"))) return 3.0;
    if (name == QStringLiteral("IE") || name.endsWith(QStringLiteral("IE"))
        || name.contains(QStringLiteral("EARTH")) || name.contains(QStringLiteral("GROUND"))) return -1.0;
    return std::nullopt;
}

double unit_scale_to_si(QString unit) {
    unit = unit.trimmed().toUpper();
    unit.remove(' ');
    if (unit == QStringLiteral("KV") || unit == QStringLiteral("KA")) return 1.0e3;
    if (unit == QStringLiteral("MV")) return 1.0e6;
    return 1.0;
}

bool window_has_status_change(const std::vector<double>& edges,
                              double startSeconds,
                              double endSeconds) {
    if (edges.empty() || !std::isfinite(startSeconds) || !std::isfinite(endSeconds)
        || endSeconds < startSeconds) return false;
    constexpr double epsilon = 1.0e-10;
    const auto it = std::lower_bound(edges.begin(), edges.end(), startSeconds - epsilon);
    return it != edges.end() && *it <= endSeconds + epsilon;
}

int loop_index(const QString& loopId) {
    for (std::size_t index = 0; index < kLoopIds.size(); ++index) {
        if (loopId.compare(QString::fromLatin1(kLoopIds[index]), Qt::CaseInsensitive) == 0)
            return static_cast<int>(index);
    }
    return -1;
}

double percentile99(std::vector<double> values) {
    if (values.empty()) return 0.0;
    const std::size_t index = static_cast<std::size_t>(
        std::floor(0.99 * static_cast<double>(values.size() - 1u)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

double distance_squared_to_segment(const LocusNativePoint& p,
                                   const LocusNativePoint& a,
                                   const LocusNativePoint& b) {
    const double ax = a.r;
    const double ay = a.x;
    const double bx = b.r;
    const double by = b.x;
    const double px = p.r;
    const double py = p.x;
    const double dx = bx - ax;
    const double dy = by - ay;
    const double denom = dx * dx + dy * dy;
    if (denom <= 1.0e-24) {
        const double ex = px - ax;
        const double ey = py - ay;
        return ex * ex + ey * ey;
    }
    const double t = std::clamp(((px - ax) * dx + (py - ay) * dy) / denom, 0.0, 1.0);
    const double ex = px - (ax + t * dx);
    const double ey = py - (ay + t * dy);
    return ex * ex + ey * ey;
}

std::vector<std::size_t> rdp_indices(const std::vector<LocusNativePoint>& points,
                                     std::size_t begin,
                                     std::size_t end,
                                     double tolerance) {
    std::vector<std::size_t> result;
    if (begin >= end) return result;
    if (end - begin <= 2u || tolerance <= 0.0) {
        result.reserve(end - begin);
        for (std::size_t index = begin; index < end; ++index) result.push_back(index);
        return result;
    }

    std::vector<std::uint8_t> keep(end - begin, 0u);
    keep.front() = 1u;
    keep.back() = 1u;
    std::vector<std::pair<std::size_t, std::size_t>> stack;
    stack.emplace_back(begin, end - 1u);
    const double toleranceSquared = tolerance * tolerance;
    while (!stack.empty()) {
        const auto [left, right] = stack.back();
        stack.pop_back();
        if (right <= left + 1u) continue;
        double maximumDistance = -1.0;
        std::size_t split = left;
        for (std::size_t index = left + 1u; index < right; ++index) {
            const double distance = distance_squared_to_segment(points[index], points[left], points[right]);
            if (distance > maximumDistance) {
                maximumDistance = distance;
                split = index;
            }
        }
        if (maximumDistance > toleranceSquared && split > left && split < right) {
            keep[split - begin] = 1u;
            stack.emplace_back(left, split);
            stack.emplace_back(split, right);
        }
    }
    for (std::size_t offset = 0; offset < keep.size(); ++offset)
        if (keep[offset]) result.push_back(begin + offset);
    return result;
}

std::vector<LocusNativePoint> simplify_bounded(const std::vector<LocusNativePoint>& input,
                                                std::size_t maximumPoints) {
    if (input.size() <= maximumPoints || maximumPoints < 4u) return input;

    std::vector<std::pair<std::size_t, std::size_t>> segments;
    std::size_t index = 0;
    while (index < input.size()) {
        while (index < input.size() && !input[index].valid) ++index;
        if (index >= input.size()) break;
        const std::size_t begin = index;
        while (index < input.size() && input[index].valid) ++index;
        segments.emplace_back(begin, index);
    }
    if (segments.empty()) return {LocusNativePoint{}};

    const std::size_t separators = segments.size() > 1u ? segments.size() - 1u : 0u;
    if (segments.size() + separators >= maximumPoints) {
        std::vector<LocusNativePoint> sparse;
        sparse.reserve(maximumPoints);
        for (std::size_t segment = 0; segment < segments.size() && sparse.size() < maximumPoints; ++segment) {
            if (!sparse.empty() && sparse.size() < maximumPoints) sparse.push_back({});
            if (sparse.size() < maximumPoints) sparse.push_back(input[segments[segment].first]);
        }
        return sparse;
    }

    double minR = std::numeric_limits<double>::infinity();
    double maxR = -std::numeric_limits<double>::infinity();
    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    for (const auto& point : input) {
        if (!point.valid) continue;
        minR = std::min(minR, static_cast<double>(point.r));
        maxR = std::max(maxR, static_cast<double>(point.r));
        minX = std::min(minX, static_cast<double>(point.x));
        maxX = std::max(maxX, static_cast<double>(point.x));
    }
    double low = 0.0;
    double high = std::hypot(maxR - minR, maxX - minX) + 1.0e-12;
    std::vector<std::vector<std::size_t>> best;

    auto calculate = [&](double tolerance) {
        std::vector<std::vector<std::size_t>> selected;
        selected.reserve(segments.size());
        std::size_t count = separators;
        for (const auto& [begin, end] : segments) {
            selected.push_back(rdp_indices(input, begin, end, tolerance));
            count += selected.back().size();
        }
        return std::pair{std::move(selected), count};
    };

    for (int iteration = 0; iteration < 24; ++iteration) {
        const double mid = (low + high) * 0.5;
        auto [selected, count] = calculate(mid);
        if (count > maximumPoints) {
            low = mid;
        } else {
            high = mid;
            best = std::move(selected);
        }
    }
    if (best.empty()) best = calculate(high).first;

    std::vector<LocusNativePoint> output;
    output.reserve(std::min(maximumPoints, input.size()));
    for (std::size_t segment = 0; segment < best.size(); ++segment) {
        if (segment > 0 && output.size() < maximumPoints) output.push_back({});
        for (const std::size_t pointIndex : best[segment]) {
            if (output.size() >= maximumPoints) break;
            output.push_back(input[pointIndex]);
        }
        if (output.size() >= maximumPoints) break;
    }
    return output;
}
} // namespace

struct LocusSnapshotSource {
    std::shared_ptr<const ardirec::comtrade::IndexedDatFile> data;
    std::shared_ptr<const std::vector<double>> times;
    std::shared_ptr<const std::vector<double>> statusEdges;
    std::array<int, 3> voltage{{-1, -1, -1}};
    std::array<int, 3> current{{-1, -1, -1}};
    std::array<double, 3> voltageScale{{1.0, 1.0, 1.0}};
    std::array<double, 3> currentScale{{1.0, 1.0, 1.0}};
    int residualCurrent{-1};
    double residualScale{1.0};
    double residualToSumMultiplier{1.0};
    ardirec::desktop::ClassicalGroundingFactors classicalGrounding;
    double calculationFrequency{50.0};
    double referenceTime{0.0};
    double currentFloor{1.0e-6};
};

namespace {
bool loop_available(const LocusSnapshotSource& source, ardirec::distance::FaultLoop loop) {
    const auto& v = source.voltage;
    const auto& i = source.current;
    const bool residualAvailable = source.residualCurrent >= 0 || (i[0] >= 0 && i[1] >= 0 && i[2] >= 0);
    switch (loop) {
    case ardirec::distance::FaultLoop::L1E: return v[0] >= 0 && i[0] >= 0 && residualAvailable;
    case ardirec::distance::FaultLoop::L2E: return v[1] >= 0 && i[1] >= 0 && residualAvailable;
    case ardirec::distance::FaultLoop::L3E: return v[2] >= 0 && i[2] >= 0 && residualAvailable;
    case ardirec::distance::FaultLoop::L1L2: return v[0] >= 0 && v[1] >= 0 && i[0] >= 0 && i[1] >= 0;
    case ardirec::distance::FaultLoop::L2L3: return v[1] >= 0 && v[2] >= 0 && i[1] >= 0 && i[2] >= 0;
    case ardirec::distance::FaultLoop::L3L1: return v[2] >= 0 && v[0] >= 0 && i[2] >= 0 && i[0] >= 0;
    }
    return false;
}

bool batch_phasors_at(const LocusSnapshotSource& source,
                      std::size_t sampleIndex,
                      std::array<std::complex<double>, 3>& voltage,
                      std::array<std::complex<double>, 3>& current,
                      std::optional<std::complex<double>>& measuredResidual,
                      bool& statusRejected,
                      const std::shared_ptr<std::atomic_bool>& cancel) {
    const auto& times = *source.times;
    if (sampleIndex >= times.size()) return false;
    const double frequency = source.calculationFrequency > 1.0 ? source.calculationFrequency : 50.0;
    const auto window = ardirec::power::trailing_cycle_window(times, times[sampleIndex], frequency);
    if (!window.valid() || window.end - window.first < 4u) return false;
    if (source.statusEdges && window_has_status_change(*source.statusEdges,
                                                       window.start_seconds,
                                                       window.end_seconds)) {
        statusRejected = true;
        return false;
    }

    std::array<std::complex<long double>, 7> accum{};
    std::array<long double, 7> weightSum{};
    std::array<std::size_t, 7> count{};
    const double omega = 2.0 * kPi * frequency;
    for (std::size_t sample = window.first; sample < window.end; ++sample) {
        if (cancel && ((sample - window.first) & 15u) == 0u && cancel->load(std::memory_order_relaxed)) return false;
        const double weight = ardirec::power::timestamp_cell_weight(times, window, sample);
        if (!(weight > 0.0) || !std::isfinite(weight)) continue;
        const long double angle = -static_cast<long double>(omega * (times[sample] - source.referenceTime));
        const std::complex<long double> basis{std::cos(angle), std::sin(angle)};
        const long double weighted = static_cast<long double>(weight);
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const int vc = source.voltage[phase];
            if (vc >= 0) {
                const double value = source.data->analogValue(sample, static_cast<std::size_t>(vc));
                if (std::isfinite(value)) {
                    accum[phase] += static_cast<long double>(value) * basis * weighted;
                    weightSum[phase] += weighted;
                    ++count[phase];
                }
            }
            const int ic = source.current[phase];
            if (ic >= 0) {
                const double value = source.data->analogValue(sample, static_cast<std::size_t>(ic));
                if (std::isfinite(value)) {
                    accum[phase + 3] += static_cast<long double>(value) * basis * weighted;
                    weightSum[phase + 3] += weighted;
                    ++count[phase + 3];
                }
            }
        }
        if (source.residualCurrent >= 0) {
            const double value = source.data->analogValue(sample, static_cast<std::size_t>(source.residualCurrent));
            if (std::isfinite(value)) {
                accum[6] += static_cast<long double>(value) * basis * weighted;
                weightSum[6] += weighted;
                ++count[6];
            }
        }
    }

    const double nan = std::numeric_limits<double>::quiet_NaN();
    voltage.fill({nan, nan});
    current.fill({nan, nan});
    for (std::size_t phase = 0; phase < 3; ++phase) {
        if (source.voltage[phase] >= 0 && count[phase] >= 4u && weightSum[phase] > 0.0L) {
            const long double scale = std::sqrt(2.0L) / weightSum[phase]
                                      * static_cast<long double>(source.voltageScale[phase]);
            const auto value = accum[phase] * scale;
            voltage[phase] = {static_cast<double>(value.real()), static_cast<double>(value.imag())};
        }
        if (source.current[phase] >= 0 && count[phase + 3] >= 4u && weightSum[phase + 3] > 0.0L) {
            const long double scale = std::sqrt(2.0L) / weightSum[phase + 3]
                                      * static_cast<long double>(source.currentScale[phase]);
            const auto value = accum[phase + 3] * scale;
            current[phase] = {static_cast<double>(value.real()), static_cast<double>(value.imag())};
        }
    }
    if (source.residualCurrent >= 0 && count[6] >= 4u && weightSum[6] > 0.0L) {
        const long double scale = std::sqrt(2.0L) / weightSum[6]
                                  * static_cast<long double>(source.residualScale * source.residualToSumMultiplier);
        const auto value = accum[6] * scale;
        measuredResidual = std::complex<double>{static_cast<double>(value.real()), static_cast<double>(value.imag())};
    } else {
        measuredResidual.reset();
    }
    return true;
}

ardirec::distance::DistanceImpedance calculate_loop(
    const LocusSnapshotSource& source,
    ardirec::distance::FaultLoop loop,
    const ardirec::distance::ThreePhasePhasors& phasors,
    const std::complex<double>& groundingFactor,
    const std::optional<std::complex<double>>& measuredResidual) {
    if (ardirec::distance::is_earth_loop(loop) && source.classicalGrounding.valid) {
        const std::complex<double> residual = measuredResidual.value_or(
            phasors.current[0] + phasors.current[1] + phasors.current[2]);
        const std::complex<double> earthCurrentIe = -residual;
        return ardirec::distance::distance_impedance_rerl_xexl(
            loop, phasors, earthCurrentIe,
            source.classicalGrounding.re_over_rl,
            source.classicalGrounding.xe_over_xl,
            source.currentFloor);
    }
    return ardirec::distance::distance_impedance(
        loop, phasors, groundingFactor, source.currentFloor, measuredResidual);
}

std::shared_ptr<LocusNativeSnapshot>
build_locus_snapshot(const std::shared_ptr<const LocusSnapshotSource>& source,
                     double viewStartSeconds,
                     double visibleDurationSeconds,
                     int maximumPoints,
                     double groundingFactorMagnitude,
                     double groundingFactorAngleDegrees,
                     const std::shared_ptr<std::atomic_bool>& cancel) {
    if (!source || !source->data || !source->times || source->times->empty()
        || !(visibleDurationSeconds > 0.0) || !std::isfinite(viewStartSeconds)
        || !std::isfinite(visibleDurationSeconds) || !std::isfinite(groundingFactorMagnitude)
        || !std::isfinite(groundingFactorAngleDegrees)) return {};

    const auto& times = *source->times;
    const double startTime = std::max(viewStartSeconds, times.front());
    const double endTime = std::min(viewStartSeconds + visibleDurationSeconds, times.back());
    if (endTime < startTime) return {};
    const auto firstIt = std::lower_bound(times.begin(), times.end(), startTime);
    const auto endIt = std::upper_bound(times.begin(), times.end(), endTime);
    const std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), firstIt));
    const std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    if (first >= end || first >= times.size()) return {};

    maximumPoints = std::clamp(maximumPoints, 16, 4096);
    const std::size_t sampleCount = end - first;
    const std::size_t analysisCount = std::min(sampleCount, kMaximumAnalysisPoints);
    const std::size_t analysisStride = sampleCount <= analysisCount
                                           ? 1u
                                           : static_cast<std::size_t>(std::ceil(static_cast<double>(sampleCount - 1u)
                                                                                / static_cast<double>(analysisCount - 1u)));

    std::array<std::vector<LocusNativePoint>, 6> candidateLoops;
    std::array<std::vector<LocusNativePoint>, 3> candidateRaw;
    const std::size_t reserveCount = analysisCount + 1u;
    for (auto& loop : candidateLoops) loop.reserve(reserveCount);
    for (auto& phase : candidateRaw) phase.reserve(reserveCount);

    auto snapshot = std::make_shared<LocusNativeSnapshot>();
    snapshot->pointBudget = maximumPoints;
    const double angleRadians = groundingFactorAngleDegrees * kPi / 180.0;
    const auto groundingFactor = std::polar(std::max(0.0, groundingFactorMagnitude), angleRadians);
    std::vector<double> earthR;
    std::vector<double> earthX;
    std::vector<double> phaseR;
    std::vector<double> phaseX;
    earthR.reserve(reserveCount * 3u);
    earthX.reserve(reserveCount * 3u);
    phaseR.reserve(reserveCount * 3u);
    phaseX.reserve(reserveCount * 3u);

    auto appendSample = [&](std::size_t index) -> bool {
        if (cancel && cancel->load(std::memory_order_relaxed)) return false;
        std::array<std::complex<double>, 3> voltage{};
        std::array<std::complex<double>, 3> current{};
        std::optional<std::complex<double>> measuredResidual;
        bool statusRejected = false;
        const bool phasorsValid = batch_phasors_at(*source, index, voltage, current,
                                                    measuredResidual, statusRejected, cancel);
        if (statusRejected) ++snapshot->statusRejectedCount;
        ++snapshot->analyzedPointCount;
        ardirec::distance::ThreePhasePhasors phasors{voltage, current};

        for (std::size_t loopIndex = 0; loopIndex < kLoops.size(); ++loopIndex) {
            LocusNativePoint point;
            if (phasorsValid && loop_available(*source, kLoops[loopIndex])) {
                const auto result = calculate_loop(*source, kLoops[loopIndex], phasors,
                                                   groundingFactor, measuredResidual);
                if (result.valid && std::isfinite(result.impedance.real()) && std::isfinite(result.impedance.imag())) {
                    const double r = result.impedance.real();
                    const double x = result.impedance.imag();
                    point.r = static_cast<float>(r);
                    point.x = static_cast<float>(x);
                    point.valid = 1;
                    snapshot->maxAbsR = std::max(snapshot->maxAbsR, std::abs(r));
                    snapshot->maxAbsX = std::max(snapshot->maxAbsX, std::abs(x));
                    if (loopIndex < 3u) {
                        snapshot->earthMaxAbsR = std::max(snapshot->earthMaxAbsR, std::abs(r));
                        snapshot->earthMaxAbsX = std::max(snapshot->earthMaxAbsX, std::abs(x));
                        earthR.push_back(std::abs(r));
                        earthX.push_back(std::abs(x));
                    } else {
                        snapshot->phaseMaxAbsR = std::max(snapshot->phaseMaxAbsR, std::abs(r));
                        snapshot->phaseMaxAbsX = std::max(snapshot->phaseMaxAbsX, std::abs(x));
                        phaseR.push_back(std::abs(r));
                        phaseX.push_back(std::abs(x));
                    }
                }
            }
            candidateLoops[loopIndex].push_back(point);
        }

        for (std::size_t phase = 0; phase < 3; ++phase) {
            LocusNativePoint point;
            if (phasorsValid && source->voltage[phase] >= 0 && source->current[phase] >= 0
                && std::isfinite(current[phase].real()) && std::isfinite(current[phase].imag())
                && std::abs(current[phase]) > source->currentFloor) {
                const std::complex<double> z = voltage[phase] / current[phase];
                if (std::isfinite(z.real()) && std::isfinite(z.imag())) {
                    point.r = static_cast<float>(z.real());
                    point.x = static_cast<float>(z.imag());
                    point.valid = 1;
                    snapshot->rawMaxAbsR = std::max(snapshot->rawMaxAbsR, std::abs(z.real()));
                    snapshot->rawMaxAbsX = std::max(snapshot->rawMaxAbsX, std::abs(z.imag()));
                }
            }
            candidateRaw[phase].push_back(point);
        }
        return true;
    };

    std::size_t last = first;
    for (std::size_t index = first; index < end; index += analysisStride) {
        if (!appendSample(index)) return {};
        last = index;
    }
    const std::size_t finalIndex = end - 1u;
    if (last != finalIndex && !appendSample(finalIndex)) return {};
    if (cancel && cancel->load(std::memory_order_relaxed)) return {};

    snapshot->earthRelevantMaxAbsR = percentile99(std::move(earthR));
    snapshot->earthRelevantMaxAbsX = percentile99(std::move(earthX));
    snapshot->phaseRelevantMaxAbsR = percentile99(std::move(phaseR));
    snapshot->phaseRelevantMaxAbsX = percentile99(std::move(phaseX));

    const std::size_t renderBudget = static_cast<std::size_t>(maximumPoints);
    for (std::size_t loop = 0; loop < candidateLoops.size(); ++loop) {
        snapshot->loops[loop] = simplify_bounded(candidateLoops[loop], renderBudget);
    }
    for (std::size_t phase = 0; phase < candidateRaw.size(); ++phase) {
        snapshot->rawPhase[phase] = simplify_bounded(candidateRaw[phase], renderBudget);
    }
    return snapshot;
}
} // namespace

LocusSnapshotController::LocusSnapshotController(DocumentController* document, QObject* parent)
    : QObject(parent), m_document(document) {
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, [this]() { rebuildSource(); invalidate(); });
        connect(m_document, &DocumentController::representationChanged, this, [this]() { rebuildSource(); invalidate(); });
    }
    rebuildSource();
}

LocusSnapshotController::~LocusSnapshotController() { cancel(); }

void LocusSnapshotController::rebuildSource() {
    cancel();
    m_classicalGroundingValid = false;
    m_reOverRl = 0.0;
    m_xeOverXl = 0.0;
    if (!m_document || !m_document->dataStoreSnapshot() || !m_document->timeIndexSnapshot()
        || m_document->analogCount() <= 0) {
        m_source.reset();
        return;
    }

    auto source = std::make_shared<LocusSnapshotSource>();
    source->data = m_document->dataStoreSnapshot();
    source->times = m_document->timeIndexSnapshot();
    source->statusEdges = std::make_shared<const std::vector<double>>(m_document->digitalEdgeTimes());
    source->classicalGrounding = ardirec::desktop::read_classical_grounding_factors(m_document->distanceZonePath());
    m_classicalGroundingValid = source->classicalGrounding.valid;
    m_reOverRl = source->classicalGrounding.re_over_rl;
    m_xeOverXl = source->classicalGrounding.xe_over_xl;
    source->calculationFrequency = m_document->calculationFrequency() > 1.0
                                       ? m_document->calculationFrequency()
                                       : (m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0);
    source->referenceTime = m_document->dataStartSeconds();

    double currentPeak = 0.0;
    for (int index = 0; index < m_document->analogCount(); ++index) {
        const QString role = m_document->analogRole(index);
        QString phase = m_document->channelPhase(index);
        if (phase == QStringLiteral("Other")) phase = phase_from_name(m_document->channelName(index));
        const double scale = m_document->channelDisplayScale(index) * unit_scale_to_si(m_document->channelUnit(index));

        int slot = -1;
        if (phase == QStringLiteral("L1")) slot = 0;
        else if (phase == QStringLiteral("L2")) slot = 1;
        else if (phase == QStringLiteral("L3")) slot = 2;
        if (slot >= 0) {
            if (role == QStringLiteral("Voltage") && source->voltage[static_cast<std::size_t>(slot)] < 0) {
                source->voltage[static_cast<std::size_t>(slot)] = index;
                source->voltageScale[static_cast<std::size_t>(slot)] = scale;
            } else if (role == QStringLiteral("Current") && source->current[static_cast<std::size_t>(slot)] < 0) {
                source->current[static_cast<std::size_t>(slot)] = index;
                source->currentScale[static_cast<std::size_t>(slot)] = scale;
                const double peak = std::abs(m_document->channelPeak(index) * unit_scale_to_si(m_document->channelUnit(index)));
                if (std::isfinite(peak)) currentPeak = std::max(currentPeak, peak);
            }
            continue;
        }

        if (role == QStringLiteral("Current") && phase == QStringLiteral("E") && source->residualCurrent < 0) {
            const auto multiplier = residual_to_sum_multiplier(m_document->channelName(index));
            if (multiplier) {
                source->residualCurrent = index;
                source->residualScale = scale;
                source->residualToSumMultiplier = *multiplier;
            }
        }
    }
    source->currentFloor = std::isfinite(currentPeak) && currentPeak > 0.0
                               ? std::max(1.0e-6, currentPeak * 1.0e-3)
                               : 1.0e-6;
    m_source = std::move(source);
}

void LocusSnapshotController::cancel() noexcept {
    if (m_cancel) m_cancel->store(true, std::memory_order_relaxed);
    m_cancel.reset();
}

void LocusSnapshotController::invalidate() {
    cancel();
    ++m_generation;
    m_nativeSnapshot.reset();
    m_haveRequest = false;
    ++m_revision;
    emit snapshotChanged();
    if (m_busy) { m_busy = false; emit busyChanged(); }
}

QVariantMap LocusSnapshotController::snapshot() const {
    return {{QStringLiteral("valid"), static_cast<bool>(m_nativeSnapshot)},
            {QStringLiteral("maxAbsR"), maxAbsR()},
            {QStringLiteral("maxAbsX"), maxAbsX()},
            {QStringLiteral("earthMaxAbsR"), earthMaxAbsR()},
            {QStringLiteral("earthMaxAbsX"), earthMaxAbsX()},
            {QStringLiteral("phaseMaxAbsR"), phaseMaxAbsR()},
            {QStringLiteral("phaseMaxAbsX"), phaseMaxAbsX()},
            {QStringLiteral("earthRelevantMaxAbsR"), earthRelevantMaxAbsR()},
            {QStringLiteral("earthRelevantMaxAbsX"), earthRelevantMaxAbsX()},
            {QStringLiteral("phaseRelevantMaxAbsR"), phaseRelevantMaxAbsR()},
            {QStringLiteral("phaseRelevantMaxAbsX"), phaseRelevantMaxAbsX()},
            {QStringLiteral("rawMaxAbsR"), rawMaxAbsR()},
            {QStringLiteral("rawMaxAbsX"), rawMaxAbsX()},
            {QStringLiteral("analyzedPointCount"), analyzedPointCount()},
            {QStringLiteral("statusRejectedCount"), statusRejectedCount()},
            {QStringLiteral("classicalGroundingValid"), classicalGroundingValid()},
            {QStringLiteral("reOverRl"), reOverRl()},
            {QStringLiteral("xeOverXl"), xeOverXl()},
            {QStringLiteral("calculationFrequency"), m_document ? m_document->calculationFrequency() : 0.0},
            {QStringLiteral("calculationFrequencyProvenance"), m_document ? m_document->calculationFrequencyProvenance() : QString{}},
            {QStringLiteral("pointBudget"), m_nativeSnapshot ? m_nativeSnapshot->pointBudget : 0}};
}

QVariantList LocusSnapshotController::locus(const QString& loopId) const {
    QVariantList values;
    if (!m_nativeSnapshot) return values;
    const int index = loop_index(loopId);
    if (index < 0) return values;
    const auto& points = m_nativeSnapshot->loops[static_cast<std::size_t>(index)];
    values.reserve(static_cast<qsizetype>(points.size()));
    for (const auto& point : points) {
        values.push_back(QVariantMap{{QStringLiteral("valid"), point.valid != 0},
                                     {QStringLiteral("r"), static_cast<double>(point.r)},
                                     {QStringLiteral("x"), static_cast<double>(point.x)}});
    }
    return values;
}

void LocusSnapshotController::request(double viewStartSeconds,
                                      double visibleDurationSeconds,
                                      int maximumPoints,
                                      double groundingFactorMagnitude,
                                      double groundingFactorAngleDegrees) {
    if (!m_source || !(visibleDurationSeconds > 0.0) || !std::isfinite(viewStartSeconds)
        || !std::isfinite(groundingFactorMagnitude) || !std::isfinite(groundingFactorAngleDegrees)) return;
    maximumPoints = std::clamp(maximumPoints, 16, 4096);
    if (m_haveRequest && m_lastStart == viewStartSeconds && m_lastDuration == visibleDurationSeconds
        && m_lastMaximumPoints == maximumPoints && m_lastKMagnitude == groundingFactorMagnitude
        && m_lastKAngle == groundingFactorAngleDegrees && (m_busy || m_nativeSnapshot)) return;

    m_lastStart = viewStartSeconds;
    m_lastDuration = visibleDurationSeconds;
    m_lastMaximumPoints = maximumPoints;
    m_lastKMagnitude = groundingFactorMagnitude;
    m_lastKAngle = groundingFactorAngleDegrees;
    m_haveRequest = true;

    cancel();
    auto cancelToken = std::make_shared<std::atomic_bool>(false);
    m_cancel = cancelToken;
    const quint64 generation = ++m_generation;
    const auto source = m_source;
    if (!m_busy) { m_busy = true; emit busyChanged(); }

    auto* watcher = new QFutureWatcher<std::shared_ptr<LocusNativeSnapshot>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<LocusNativeSnapshot>>::finished, this,
            [this, watcher, generation, cancelToken]() {
                const auto value = watcher->result();
                watcher->deleteLater();
                if (generation != m_generation || cancelToken->load(std::memory_order_relaxed) || !value) return;
                m_cancel.reset();
                m_nativeSnapshot = value;
                if (m_busy) { m_busy = false; emit busyChanged(); }
                ++m_revision;
                emit snapshotChanged();
            });
    watcher->setFuture(QtConcurrent::run([source, viewStartSeconds, visibleDurationSeconds, maximumPoints,
                                          groundingFactorMagnitude, groundingFactorAngleDegrees, cancelToken]() {
        return build_locus_snapshot(source, viewStartSeconds, visibleDurationSeconds, maximumPoints,
                                    groundingFactorMagnitude, groundingFactorAngleDegrees, cancelToken);
    }));
}
