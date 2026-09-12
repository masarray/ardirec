// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

[[nodiscard]] bool finite_complex(const std::complex<double>& value) noexcept {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

[[nodiscard]] bool has_minimum_finite_samples(const DocumentController& document,
                                              int channelIndex,
                                              std::size_t first,
                                              std::size_t end) noexcept {
    std::size_t finiteCount = 0;
    for (std::size_t i = first; i < end; ++i) {
        if (!std::isfinite(document.recordedAnalogSampleAt(channelIndex, i))) continue;
        if (++finiteCount >= 4) return true;
    }
    return false;
}

[[nodiscard]] QVariantMap invalid_sequence_snapshot(const QString& role) {
    return {{QStringLiteral("valid"), false},
            {QStringLiteral("role"), role},
            {QStringLiteral("provenance"), QStringLiteral("DERIVED")}};
}

[[nodiscard]] QVariantMap sequence_value(const QString& name,
                                         const QString& sequence,
                                         const QString& unit,
                                         const std::complex<double>& value) {
    const double magnitude = std::abs(value);
    const double angle = std::atan2(value.imag(), value.real()) * 180.0 / kPi;
    return {{QStringLiteral("valid"), finite_complex(value) && std::isfinite(magnitude)},
            {QStringLiteral("name"), name},
            {QStringLiteral("sequence"), sequence},
            {QStringLiteral("provenance"), QStringLiteral("DERIVED")},
            {QStringLiteral("unit"), unit},
            {QStringLiteral("magnitude"), magnitude},
            {QStringLiteral("angle"), angle},
            {QStringLiteral("real"), value.real()},
            {QStringLiteral("imag"), value.imag()}};
}
} // namespace

QVariantMap AnalysisController::sequenceComponentsAt(const QString& role,
                                                     double absoluteTimeSeconds) const {
    if (!m_document) return invalid_sequence_snapshot(role);

    QString normalizedRole;
    QString prefix;
    if (role.compare(QStringLiteral("Voltage"), Qt::CaseInsensitive) == 0) {
        normalizedRole = QStringLiteral("Voltage");
        prefix = QStringLiteral("V");
    } else if (role.compare(QStringLiteral("Current"), Qt::CaseInsensitive) == 0) {
        normalizedRole = QStringLiteral("Current");
        prefix = QStringLiteral("I");
    } else {
        return invalid_sequence_snapshot(role);
    }

    const int l1 = phaseChannel(normalizedRole, QStringLiteral("L1"));
    const int l2 = phaseChannel(normalizedRole, QStringLiteral("L2"));
    const int l3 = phaseChannel(normalizedRole, QStringLiteral("L3"));
    if (l1 < 0 || l2 < 0 || l3 < 0) return invalid_sequence_snapshot(normalizedRole);

    const QString unit = m_document->channelUnit(l1).trimmed();
    if (m_document->channelUnit(l2).trimmed().compare(unit, Qt::CaseInsensitive) != 0
        || m_document->channelUnit(l3).trimmed().compare(unit, Qt::CaseInsensitive) != 0) {
        return invalid_sequence_snapshot(normalizedRole);
    }

    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    if (end <= first || end - first < 4) return invalid_sequence_snapshot(normalizedRole);
    if (!has_minimum_finite_samples(*m_document, l1, first, end)
        || !has_minimum_finite_samples(*m_document, l2, first, end)
        || !has_minimum_finite_samples(*m_document, l3, first, end)) {
        return invalid_sequence_snapshot(normalizedRole);
    }

    const ardirec::power::ThreePhasePhasors phases{
        phasorComplex(l1, absoluteTimeSeconds, 1),
        phasorComplex(l2, absoluteTimeSeconds, 1),
        phasorComplex(l3, absoluteTimeSeconds, 1),
    };
    if (!finite_complex(phases.l1) || !finite_complex(phases.l2) || !finite_complex(phases.l3)) {
        return invalid_sequence_snapshot(normalizedRole);
    }

    const auto components = ardirec::power::symmetrical_components(phases);
    if (!components.has_value()) return invalid_sequence_snapshot(normalizedRole);

    const double positiveMagnitude = std::abs(components->positive);
    const double negativeMagnitude = std::abs(components->negative);
    const double zeroMagnitude = std::abs(components->zero);
    const double ratioFloor = std::max(1.0e-12,
                                       std::max({std::abs(phases.l1), std::abs(phases.l2), std::abs(phases.l3)})
                                           * 1.0e-12);
    const double invalidRatio = std::numeric_limits<double>::quiet_NaN();
    const double negativePercent = positiveMagnitude > ratioFloor
                                       ? negativeMagnitude / positiveMagnitude * 100.0
                                       : invalidRatio;
    const double zeroPercent = positiveMagnitude > ratioFloor
                                   ? zeroMagnitude / positiveMagnitude * 100.0
                                   : invalidRatio;

    return {{QStringLiteral("valid"), true},
            {QStringLiteral("role"), normalizedRole},
            {QStringLiteral("provenance"), QStringLiteral("DERIVED")},
            {QStringLiteral("unit"), unit},
            {QStringLiteral("sourceL1"), l1},
            {QStringLiteral("sourceL2"), l2},
            {QStringLiteral("sourceL3"), l3},
            {QStringLiteral("positive"),
             sequence_value(prefix + QStringLiteral("1"), QStringLiteral("POSITIVE"), unit, components->positive)},
            {QStringLiteral("negative"),
             sequence_value(prefix + QStringLiteral("2"), QStringLiteral("NEGATIVE"), unit, components->negative)},
            {QStringLiteral("zero"),
             sequence_value(prefix + QStringLiteral("0"), QStringLiteral("ZERO"), unit, components->zero)},
            {QStringLiteral("negativePercent"), negativePercent},
            {QStringLiteral("zeroPercent"), zeroPercent}};
}
