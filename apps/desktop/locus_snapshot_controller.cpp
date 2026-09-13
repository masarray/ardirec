// SPDX-License-Identifier: GPL-3.0-or-later
#include "locus_snapshot_controller.hpp"

#include "ardirec/distance/distance.hpp"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::array<const char*, 6> kLoopIds{{"L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"}};

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

double unit_scale_to_si(QString unit) {
    unit = unit.trimmed().toUpper();
    unit.remove(' ');
    if (unit == QStringLiteral("KV") || unit == QStringLiteral("KA")) return 1.0e3;
    if (unit == QStringLiteral("MV")) return 1.0e6;
    return 1.0;
}

std::pair<std::size_t, std::size_t> one_cycle_window(const std::vector<double>& times,
                                                     double absoluteTimeSeconds,
                                                     double frequency) {
    if (times.size() < 2) return {0, times.size()};
    const double period = 1.0 / frequency;
    const double endTime = std::clamp(absoluteTimeSeconds, times.front(), times.back());
    const double startTime = std::max(times.front(), endTime - period);
    auto firstIt = std::lower_bound(times.begin(), times.end(), startTime);
    auto endIt = std::upper_bound(times.begin(), times.end(), endTime);
    std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), firstIt));
    std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    end = std::min(end, times.size());
    if (end > first + 2 && times[end - 1] - times[first] >= period * (1.0 - 1.0e-8)) ++first;
    if (end <= first) return {0, 0};
    return {first, end};
}

QVariantMap invalid_point(double timeSeconds, const QString& loopId, double minimumCurrent) {
    return {{QStringLiteral("valid"), false},
            {QStringLiteral("time"), timeSeconds},
            {QStringLiteral("r"), 0.0},
            {QStringLiteral("x"), 0.0},
            {QStringLiteral("magnitude"), 0.0},
            {QStringLiteral("angle"), 0.0},
            {QStringLiteral("measuringCurrent"), 0.0},
            {QStringLiteral("minimumCurrent"), minimumCurrent},
            {QStringLiteral("loop"), loopId}};
}
} // namespace

struct LocusSnapshotSource {
    std::shared_ptr<const ardirec::comtrade::IndexedDatFile> data;
    std::shared_ptr<const std::vector<double>> times;
    std::array<int, 3> voltage{{-1, -1, -1}};
    std::array<int, 3> current{{-1, -1, -1}};
    std::array<double, 3> voltageScale{{1.0, 1.0, 1.0}};
    std::array<double, 3> currentScale{{1.0, 1.0, 1.0}};
    double nominalFrequency{50.0};
    double referenceTime{0.0};
    double currentFloor{1.0e-6};
};

namespace {
bool loop_available(const LocusSnapshotSource& source, ardirec::distance::FaultLoop loop) {
    const auto& v = source.voltage;
    const auto& i = source.current;
    switch (loop) {
    case ardirec::distance::FaultLoop::L1E: return v[0] >= 0 && i[0] >= 0 && i[1] >= 0 && i[2] >= 0;
    case ardirec::distance::FaultLoop::L2E: return v[1] >= 0 && i[0] >= 0 && i[1] >= 0 && i[2] >= 0;
    case ardirec::distance::FaultLoop::L3E: return v[2] >= 0 && i[0] >= 0 && i[1] >= 0 && i[2] >= 0;
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
                      const std::shared_ptr<std::atomic_bool>& cancel) {
    const auto& times = *source.times;
    if (sampleIndex >= times.size()) return false;
    const auto [first, end] = one_cycle_window(times, times[sampleIndex], source.nominalFrequency);
    if (first >= end || end - first < 4) return false;

    std::array<std::complex<long double>, 6> accum{};
    std::array<std::size_t, 6> count{};
    const double omega = 2.0 * kPi * source.nominalFrequency;
    for (std::size_t sample = first; sample < end; ++sample) {
        if (cancel && ((sample - first) & 15u) == 0u && cancel->load(std::memory_order_relaxed)) return false;
        const long double angle = -static_cast<long double>(omega * (times[sample] - source.referenceTime));
        const std::complex<long double> basis{std::cos(angle), std::sin(angle)};
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const int vc = source.voltage[phase];
            if (vc >= 0) {
                const double value = source.data->analogValue(sample, static_cast<std::size_t>(vc));
                if (std::isfinite(value)) { accum[phase] += static_cast<long double>(value) * basis; ++count[phase]; }
            }
            const int ic = source.current[phase];
            if (ic >= 0) {
                const double value = source.data->analogValue(sample, static_cast<std::size_t>(ic));
                if (std::isfinite(value)) { accum[phase + 3] += static_cast<long double>(value) * basis; ++count[phase + 3]; }
            }
        }
    }

    for (std::size_t phase = 0; phase < 3; ++phase) {
        if (source.voltage[phase] >= 0 && count[phase] >= 4) {
            const long double scale = std::sqrt(2.0L) / static_cast<long double>(count[phase])
                                      * static_cast<long double>(source.voltageScale[phase]);
            const auto value = accum[phase] * scale;
            voltage[phase] = {static_cast<double>(value.real()), static_cast<double>(value.imag())};
        }
        if (source.current[phase] >= 0 && count[phase + 3] >= 4) {
            const long double scale = std::sqrt(2.0L) / static_cast<long double>(count[phase + 3])
                                      * static_cast<long double>(source.currentScale[phase]);
            const auto value = accum[phase + 3] * scale;
            current[phase] = {static_cast<double>(value.real()), static_cast<double>(value.imag())};
        }
    }
    return true;
}

QVariantMap build_locus_snapshot(const std::shared_ptr<const LocusSnapshotSource>& source,
                                 double viewStartSeconds,
                                 double visibleDurationSeconds,
                                 int maximumPoints,
                                 double groundingFactorMagnitude,
                                 double groundingFactorAngleDegrees,
                                 const std::shared_ptr<std::atomic_bool>& cancel) {
    QVariantMap result;
    std::array<QVariantList, 6> loci;
    for (std::size_t i = 0; i < kLoopIds.size(); ++i) result.insert(QString::fromLatin1(kLoopIds[i]), QVariantList{});
    if (!source || !source->data || !source->times || source->times->empty()
        || !(visibleDurationSeconds > 0.0) || !std::isfinite(viewStartSeconds)
        || !std::isfinite(visibleDurationSeconds) || !std::isfinite(groundingFactorMagnitude)
        || !std::isfinite(groundingFactorAngleDegrees)) return result;

    const auto& times = *source->times;
    const double startTime = std::max(viewStartSeconds, times.front());
    const double endTime = std::min(viewStartSeconds + visibleDurationSeconds, times.back());
    if (endTime < startTime) return result;
    const auto firstIt = std::lower_bound(times.begin(), times.end(), startTime);
    const auto endIt = std::upper_bound(times.begin(), times.end(), endTime);
    const std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), firstIt));
    const std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    if (first >= end || first >= times.size()) return result;

    maximumPoints = std::clamp(maximumPoints, 16, 4096);
    const std::size_t sampleCount = end - first;
    const std::size_t maxCount = static_cast<std::size_t>(maximumPoints);
    const std::size_t stride = sampleCount <= maxCount
                                   ? 1u
                                   : static_cast<std::size_t>(std::ceil(static_cast<double>(sampleCount - 1u)
                                                                        / static_cast<double>(maxCount - 1u)));
    const qsizetype reserveCount = static_cast<qsizetype>(std::min(sampleCount, maxCount) + 1u);
    for (auto& list : loci) list.reserve(reserveCount);

    const double angleRadians = groundingFactorAngleDegrees * kPi / 180.0;
    const auto groundingFactor = std::polar(std::max(0.0, groundingFactorMagnitude), angleRadians);
    constexpr std::array<ardirec::distance::FaultLoop, 6> loops{{
        ardirec::distance::FaultLoop::L1E, ardirec::distance::FaultLoop::L2E,
        ardirec::distance::FaultLoop::L3E, ardirec::distance::FaultLoop::L1L2,
        ardirec::distance::FaultLoop::L2L3, ardirec::distance::FaultLoop::L3L1,
    }};

    auto appendSample = [&](std::size_t index) -> bool {
        if (cancel && cancel->load(std::memory_order_relaxed)) return false;
        std::array<std::complex<double>, 3> voltage{};
        std::array<std::complex<double>, 3> current{};
        const double time = times[index];
        const bool phasorsValid = batch_phasors_at(*source, index, voltage, current, cancel);
        ardirec::distance::ThreePhasePhasors phasors{voltage, current};
        for (std::size_t loopIndex = 0; loopIndex < loops.size(); ++loopIndex) {
            const QString loopId = QString::fromLatin1(kLoopIds[loopIndex]);
            if (!phasorsValid || !loop_available(*source, loops[loopIndex])) {
                loci[loopIndex].push_back(invalid_point(time, loopId, source->currentFloor));
                continue;
            }
            const auto value = ardirec::distance::distance_impedance(loops[loopIndex], phasors,
                                                                      groundingFactor, source->currentFloor);
            if (!value.valid) {
                loci[loopIndex].push_back(invalid_point(time, loopId, source->currentFloor));
                continue;
            }
            const auto z = value.impedance;
            loci[loopIndex].push_back(QVariantMap{{QStringLiteral("valid"), true},
                                                  {QStringLiteral("time"), time},
                                                  {QStringLiteral("r"), z.real()},
                                                  {QStringLiteral("x"), z.imag()},
                                                  {QStringLiteral("magnitude"), std::abs(z)},
                                                  {QStringLiteral("angle"), std::atan2(z.imag(), z.real()) * 180.0 / kPi},
                                                  {QStringLiteral("measuringCurrent"), std::abs(value.measuring_current)},
                                                  {QStringLiteral("minimumCurrent"), source->currentFloor},
                                                  {QStringLiteral("loop"), loopId}});
        }
        return true;
    };

    std::size_t last = first;
    for (std::size_t index = first; index < end; index += stride) {
        if (!appendSample(index)) return {{QStringLiteral("cancelled"), true}};
        last = index;
    }
    const std::size_t finalIndex = end - 1u;
    if (last != finalIndex && !appendSample(finalIndex)) return {{QStringLiteral("cancelled"), true}};
    if (cancel && cancel->load(std::memory_order_relaxed)) return {{QStringLiteral("cancelled"), true}};

    double maxAbsR = 0.0;
    double maxAbsX = 0.0;
    for (std::size_t loopIndex = 0; loopIndex < kLoopIds.size(); ++loopIndex) {
        for (const QVariant& item : loci[loopIndex]) {
            const QVariantMap point = item.toMap();
            if (!point.value(QStringLiteral("valid")).toBool()) continue;
            maxAbsR = std::max(maxAbsR, std::abs(point.value(QStringLiteral("r")).toDouble()));
            maxAbsX = std::max(maxAbsX, std::abs(point.value(QStringLiteral("x")).toDouble()));
        }
        result.insert(QString::fromLatin1(kLoopIds[loopIndex]), loci[loopIndex]);
    }
    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("maxAbsR"), maxAbsR);
    result.insert(QStringLiteral("maxAbsX"), maxAbsX);
    result.insert(QStringLiteral("pointBudget"), maximumPoints);
    return result;
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

LocusSnapshotController::~LocusSnapshotController() {
    cancel();
}

void LocusSnapshotController::rebuildSource() {
    cancel();
    if (!m_document || !m_document->dataStoreSnapshot() || !m_document->timeIndexSnapshot()
        || m_document->analogCount() <= 0) {
        m_source.reset();
        return;
    }
    auto source = std::make_shared<LocusSnapshotSource>();
    source->data = m_document->dataStoreSnapshot();
    source->times = m_document->timeIndexSnapshot();
    source->nominalFrequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    source->referenceTime = m_document->dataStartSeconds();

    double currentPeak = 0.0;
    for (int index = 0; index < m_document->analogCount(); ++index) {
        const QString role = m_document->analogRole(index);
        const QString phase = phase_from_name(m_document->channelName(index));
        int slot = -1;
        if (phase == QStringLiteral("L1")) slot = 0;
        else if (phase == QStringLiteral("L2")) slot = 1;
        else if (phase == QStringLiteral("L3")) slot = 2;
        if (slot < 0) continue;
        const double scale = m_document->channelDisplayScale(index) * unit_scale_to_si(m_document->channelUnit(index));
        if (role == QStringLiteral("Voltage") && source->voltage[static_cast<std::size_t>(slot)] < 0) {
            source->voltage[static_cast<std::size_t>(slot)] = index;
            source->voltageScale[static_cast<std::size_t>(slot)] = scale;
        } else if (role == QStringLiteral("Current") && source->current[static_cast<std::size_t>(slot)] < 0) {
            source->current[static_cast<std::size_t>(slot)] = index;
            source->currentScale[static_cast<std::size_t>(slot)] = scale;
            const double peak = std::abs(m_document->channelPeak(index) * unit_scale_to_si(m_document->channelUnit(index)));
            if (std::isfinite(peak)) currentPeak = std::max(currentPeak, peak);
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
    m_snapshot.clear();
    m_haveRequest = false;
    ++m_revision;
    emit snapshotChanged();
    if (m_busy) { m_busy = false; emit busyChanged(); }
}

QVariantList LocusSnapshotController::locus(const QString& loopId) const {
    return m_snapshot.value(loopId).toList();
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
        && m_lastKAngle == groundingFactorAngleDegrees && (m_busy || m_snapshot.value(QStringLiteral("valid")).toBool())) {
        return;
    }

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

    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, generation, cancelToken]() {
                const QVariantMap value = watcher->result();
                watcher->deleteLater();
                if (generation != m_generation || cancelToken->load(std::memory_order_relaxed)
                    || value.value(QStringLiteral("cancelled")).toBool()) return;
                m_cancel.reset();
                m_snapshot = value;
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
