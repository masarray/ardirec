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
