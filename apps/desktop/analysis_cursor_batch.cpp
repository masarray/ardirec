// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

QVariantMap invalid_cursor_distance(const QString& loop, double minimumCurrent) {
    return {{QStringLiteral("valid"), false},
            {QStringLiteral("r"), 0.0},
            {QStringLiteral("x"), 0.0},
            {QStringLiteral("magnitude"), 0.0},
            {QStringLiteral("angle"), 0.0},
            {QStringLiteral("measuringCurrent"), 0.0},
            {QStringLiteral("minimumCurrent"), minimumCurrent},
            {QStringLiteral("loop"), loop}};
}

constexpr std::array<const char*, 6> kLoopIds{{
    "L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"
}};
} // namespace

QVariantMap AnalysisController::distanceLoopsAt(double absoluteTimeSeconds,
                                                double groundingFactorMagnitude,
                                                double groundingFactorAngleDegrees) const {
    QVariantMap values;
    if (!m_document || !std::isfinite(absoluteTimeSeconds)
        || !std::isfinite(groundingFactorMagnitude)
        || !std::isfinite(groundingFactorAngleDegrees)) {
        return values;
    }

    ardirec::distance::ThreePhasePhasors phasors;
    for (int phase = 0; phase < 3; ++phase) {
        const QString phaseName = QStringLiteral("L%1").arg(phase + 1);
        const int voltageChannel = phaseChannel(QStringLiteral("Voltage"), phaseName);
        const int currentChannel = phaseChannel(QStringLiteral("Current"), phaseName);
        if (voltageChannel >= 0) {
            phasors.voltage[static_cast<std::size_t>(phase)] =
                phasorComplex(voltageChannel, absoluteTimeSeconds, 1) * unitScaleToSi(voltageChannel);
        }
        if (currentChannel >= 0) {
            phasors.current[static_cast<std::size_t>(phase)] =
                phasorComplex(currentChannel, absoluteTimeSeconds, 1) * unitScaleToSi(currentChannel);
        }
    }

    const double angleRadians = groundingFactorAngleDegrees * kPi / 180.0;
    const std::complex<double> groundingFactor =
        std::polar(std::max(0.0, groundingFactorMagnitude), angleRadians);
    const double minimumCurrent = distanceCurrentFloor();

    struct LoopDef {
        const char* id;
        ardirec::distance::FaultLoop loop;
    };
    constexpr std::array<LoopDef, 6> loops{{
        {"L1-E", ardirec::distance::FaultLoop::L1E},
        {"L2-E", ardirec::distance::FaultLoop::L2E},
        {"L3-E", ardirec::distance::FaultLoop::L3E},
        {"L1-L2", ardirec::distance::FaultLoop::L1L2},
        {"L2-L3", ardirec::distance::FaultLoop::L2L3},
        {"L3-L1", ardirec::distance::FaultLoop::L3L1},
    }};

    for (const auto& definition : loops) {
        const QString loopId = QString::fromLatin1(definition.id);
        if (!distanceLoopAvailable(loopId)) {
            values.insert(loopId, invalid_cursor_distance(loopId, minimumCurrent));
            continue;
        }

        const auto result = ardirec::distance::distance_impedance(
            definition.loop, phasors, groundingFactor, minimumCurrent);
        if (!result.valid) {
            values.insert(loopId, invalid_cursor_distance(loopId, minimumCurrent));
            continue;
        }

        const auto impedance = result.impedance;
        values.insert(loopId,
                      QVariantMap{{QStringLiteral("valid"), true},
                                  {QStringLiteral("r"), impedance.real()},
                                  {QStringLiteral("x"), impedance.imag()},
                                  {QStringLiteral("magnitude"), std::abs(impedance)},
                                  {QStringLiteral("angle"), std::atan2(impedance.imag(), impedance.real()) * 180.0 / kPi},
                                  {QStringLiteral("measuringCurrent"), std::abs(result.measuring_current)},
                                  {QStringLiteral("minimumCurrent"), minimumCurrent},
                                  {QStringLiteral("loop"), loopId}});
    }

    return values;
}

QVariantMap AnalysisController::distanceLoci(double viewStartSeconds,
                                             double visibleDurationSeconds,
                                             int maximumPoints,
                                             double groundingFactorMagnitude,
                                             double groundingFactorAngleDegrees) const {
    QVariantMap result;
    std::array<QVariantList, 6> loci;
    for (std::size_t i = 0; i < kLoopIds.size(); ++i)
        result.insert(QString::fromLatin1(kLoopIds[i]), QVariantList{});

    if (!m_document || visibleDurationSeconds <= 0.0 || !std::isfinite(viewStartSeconds)
        || !std::isfinite(visibleDurationSeconds) || !std::isfinite(groundingFactorMagnitude)
        || !std::isfinite(groundingFactorAngleDegrees)) {
        return result;
    }

    const auto& times = m_document->timeSeconds();
    if (times.empty()) return result;

    const double requestedEnd = viewStartSeconds + visibleDurationSeconds;
    const double startTime = std::max(viewStartSeconds, m_document->dataStartSeconds());
    const double endTime = std::min(requestedEnd, m_document->dataEndSeconds());
    if (endTime < startTime) return result;

    const auto firstIt = std::lower_bound(times.begin(), times.end(), startTime);
    const auto endIt = std::upper_bound(times.begin(), times.end(), endTime);
    const std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), firstIt));
    const std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    if (first >= end || first >= times.size()) return result;

    maximumPoints = std::clamp(maximumPoints, 16, 4000);
    const std::size_t sampleCount = end - first;
    const std::size_t maxCount = static_cast<std::size_t>(maximumPoints);
    const std::size_t stride = sampleCount <= maxCount
                                   ? 1u
                                   : static_cast<std::size_t>(std::ceil(static_cast<double>(sampleCount - 1)
                                                                        / static_cast<double>(maxCount - 1)));
    const qsizetype reserveCount = static_cast<qsizetype>(std::min(sampleCount, maxCount) + 1);
    for (auto& list : loci) list.reserve(reserveCount);

    auto appendSample = [&](std::size_t index) {
        const double time = times[index];
        const QVariantMap values = distanceLoopsAt(time,
                                                   groundingFactorMagnitude,
                                                   groundingFactorAngleDegrees);
        for (std::size_t loopIndex = 0; loopIndex < kLoopIds.size(); ++loopIndex) {
            const QString loopId = QString::fromLatin1(kLoopIds[loopIndex]);
            QVariantMap point = values.value(loopId).toMap();
            if (point.isEmpty()) point = invalid_cursor_distance(loopId, distanceCurrentFloor());
            point.insert(QStringLiteral("time"), time);
            loci[loopIndex].push_back(point);
        }
    };

    std::size_t lastAppended = first;
    for (std::size_t index = first; index < end; index += stride) {
        appendSample(index);
        lastAppended = index;
    }
    const std::size_t finalIndex = end - 1;
    if (lastAppended != finalIndex) appendSample(finalIndex);

    for (std::size_t i = 0; i < kLoopIds.size(); ++i)
        result.insert(QString::fromLatin1(kLoopIds[i]), loci[i]);
    return result;
}
