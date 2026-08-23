// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

QString compact_name(QString value) {
    value = value.trimmed().toUpper();
    value.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    return value;
}

QString normalized_unit(QString value) {
    value = value.trimmed().toUpper();
    value.remove(' ');
    return value;
}

QString phase_from_name(const QString& rawName) {
    const QString name = compact_name(rawName);
    if (name.isEmpty()) return QStringLiteral("Other");

    if (name.contains(QStringLiteral("L1")) || name.endsWith(QStringLiteral("AN"))
        || name.endsWith(QStringLiteral("IA")) || name.endsWith(QStringLiteral("VA"))
        || name.endsWith(QStringLiteral("UA")) || name == QStringLiteral("A")) {
        return QStringLiteral("L1");
    }
    if (name.contains(QStringLiteral("L2")) || name.endsWith(QStringLiteral("BN"))
        || name.endsWith(QStringLiteral("IB")) || name.endsWith(QStringLiteral("VB"))
        || name.endsWith(QStringLiteral("UB")) || name == QStringLiteral("B")) {
        return QStringLiteral("L2");
    }
    if (name.contains(QStringLiteral("L3")) || name.endsWith(QStringLiteral("CN"))
        || name.endsWith(QStringLiteral("IC")) || name.endsWith(QStringLiteral("VC"))
        || name.endsWith(QStringLiteral("UC")) || name == QStringLiteral("C")) {
        return QStringLiteral("L3");
    }

    if (name.contains(QStringLiteral("3I0")) || name.contains(QStringLiteral("3V0"))
        || name.contains(QStringLiteral("3U0")) || name.contains(QStringLiteral("RES"))
        || name.contains(QStringLiteral("NEUTRAL")) || name.contains(QStringLiteral("GROUND"))
        || name.contains(QStringLiteral("EARTH")) || name.endsWith(QStringLiteral("IN"))
        || name.endsWith(QStringLiteral("VN")) || name.endsWith(QStringLiteral("UN"))
        || name.endsWith(QStringLiteral("IE")) || name.endsWith(QStringLiteral("VE"))
        || name.endsWith(QStringLiteral("UE")) || name == QStringLiteral("N")
        || name == QStringLiteral("E")) {
        return QStringLiteral("E");
    }

    return QStringLiteral("Other");
}

QVariantMap invalid_phasor() {
    return {{QStringLiteral("valid"), false},
            {QStringLiteral("magnitude"), 0.0},
            {QStringLiteral("angle"), 0.0},
            {QStringLiteral("real"), 0.0},
            {QStringLiteral("imag"), 0.0}};
}
} // namespace

AnalysisController::AnalysisController(DocumentController* document, QObject* parent)
    : QObject(parent), m_document(document) {}

QString AnalysisController::channelPhase(int channelIndex) const {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) {
        return QStringLiteral("Other");
    }
    return phase_from_name(m_document->channelName(channelIndex));
}

QString AnalysisController::phaseColorForName(const QString& phase) const {
    const QString normalized = phase.trimmed().toUpper();
    if (normalized == QStringLiteral("L1")) return QStringLiteral("#d32f2f");
    if (normalized == QStringLiteral("L2")) return QStringLiteral("#d6a700");
    if (normalized == QStringLiteral("L3")) return QStringLiteral("#1976d2");
    if (normalized == QStringLiteral("E") || normalized == QStringLiteral("N")) {
        return QStringLiteral("#2e8b57");
    }
    return QStringLiteral("#6f7780");
}

QString AnalysisController::phaseColor(int channelIndex) const {
    return phaseColorForName(channelPhase(channelIndex));
}

int AnalysisController::phaseChannel(const QString& role, const QString& phase) const {
    if (!m_document) return -1;
    for (int i = 0; i < m_document->analogCount(); ++i) {
        if (m_document->analogRole(i).compare(role, Qt::CaseInsensitive) == 0
            && channelPhase(i).compare(phase, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

std::pair<std::size_t, std::size_t> AnalysisController::oneCycleWindow(double absoluteTimeSeconds) const {
    if (!m_document) return {0, 0};
    const auto& times = m_document->timeSeconds();
    if (times.size() < 2) return {0, times.size()};

    const double frequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    const double period = 1.0 / frequency;
    const double endTime = std::clamp(absoluteTimeSeconds,
                                      m_document->dataStartSeconds(),
                                      m_document->dataEndSeconds());
    const double startTime = std::max(m_document->dataStartSeconds(), endTime - period);

    auto firstIt = std::lower_bound(times.begin(), times.end(), startTime);
    auto endIt = std::upper_bound(times.begin(), times.end(), endTime);
    std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), firstIt));
    std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    end = std::min(end, times.size());

    // A perfect one-cycle window must not count the same electrical phase twice at both boundaries.
    if (end > first + 2 && times[end - 1] - times[first] >= period * (1.0 - 1.0e-8)) ++first;
    if (end <= first) return {0, 0};
    return {first, end};
}

double AnalysisController::rmsValue(int channelIndex, double absoluteTimeSeconds) const {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto& samples = m_document->analogSamples(channelIndex);
    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    if (first >= end || first >= samples.size()) return std::numeric_limits<double>::quiet_NaN();
    const std::size_t cappedEnd = std::min(end, samples.size());

    long double sumSquares = 0.0L;
    std::size_t count = 0;
    for (std::size_t i = first; i < cappedEnd; ++i) {
        const double value = samples[i];
        if (!std::isfinite(value)) continue;
        sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
        ++count;
    }
    if (count == 0) return std::numeric_limits<double>::quiet_NaN();
    return std::sqrt(static_cast<double>(sumSquares / static_cast<long double>(count)));
}

QString AnalysisController::formatEngineeringValue(double value, int channelIndex) const {
    if (!m_document || !std::isfinite(value)) return QStringLiteral("—");
    const double magnitude = std::abs(value);
    int decimals = 4;
    if (magnitude >= 1000.0) decimals = 1;
    else if (magnitude >= 100.0) decimals = 2;
    else if (magnitude >= 10.0) decimals = 3;
    const QString unit = m_document->channelUnit(channelIndex);
    return unit.isEmpty() ? QString::number(value, 'f', decimals)
                          : QStringLiteral("%1 %2").arg(QString::number(value, 'f', decimals), unit);
}

QString AnalysisController::rmsValueText(int channelIndex, double absoluteTimeSeconds) const {
    return formatEngineeringValue(rmsValue(channelIndex, absoluteTimeSeconds), channelIndex);
}

std::complex<double> AnalysisController::phasorComplex(int channelIndex,
                                                       double absoluteTimeSeconds,
                                                       int harmonicOrder) const {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()
        || harmonicOrder < 1) {
        return {};
    }
    const auto& samples = m_document->analogSamples(channelIndex);
    const auto& times = m_document->timeSeconds();
    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    const std::size_t cappedEnd = std::min({end, samples.size(), times.size()});
    if (first >= cappedEnd || cappedEnd - first < 4) return {};

    const double frequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    const double omega = 2.0 * kPi * frequency * static_cast<double>(harmonicOrder);
    const double referenceTime = m_document->dataStartSeconds();
    std::complex<long double> accumulator{0.0L, 0.0L};
    std::size_t count = 0;

    for (std::size_t i = first; i < cappedEnd; ++i) {
        const double value = samples[i];
        if (!std::isfinite(value)) continue;
        const long double angle = -static_cast<long double>(omega * (times[i] - referenceTime));
        const std::complex<long double> basis{std::cos(angle), std::sin(angle)};
        accumulator += static_cast<long double>(value) * basis;
        ++count;
    }
    if (count < 4) return {};

    const long double scale = std::sqrt(2.0L) / static_cast<long double>(count);
    accumulator *= scale;
    return {static_cast<double>(accumulator.real()), static_cast<double>(accumulator.imag())};
}

QVariantMap AnalysisController::phasorAt(int channelIndex, double absoluteTimeSeconds) const {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) return invalid_phasor();
    const auto value = phasorComplex(channelIndex, absoluteTimeSeconds, 1);
    const double magnitude = std::abs(value);
    if (!std::isfinite(magnitude)) return invalid_phasor();
    const double angle = std::atan2(value.imag(), value.real()) * 180.0 / kPi;
    return {{QStringLiteral("valid"), true},
            {QStringLiteral("magnitude"), magnitude},
            {QStringLiteral("angle"), angle},
            {QStringLiteral("real"), value.real()},
            {QStringLiteral("imag"), value.imag()},
            {QStringLiteral("unit"), m_document->channelUnit(channelIndex)},
            {QStringLiteral("name"), m_document->channelName(channelIndex)},
            {QStringLiteral("phase"), channelPhase(channelIndex)}};
}

QVariantList AnalysisController::harmonicsAt(int channelIndex,
                                             double absoluteTimeSeconds,
                                             int maximumOrder) const {
    QVariantList result;
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) return result;
    maximumOrder = std::clamp(maximumOrder, 1, 50);

    const double fundamental = std::abs(phasorComplex(channelIndex, absoluteTimeSeconds, 1));
    for (int order = 1; order <= maximumOrder; ++order) {
        const double magnitude = std::abs(phasorComplex(channelIndex, absoluteTimeSeconds, order));
        const double percent = fundamental > 1.0e-12 ? magnitude / fundamental * 100.0 : 0.0;
        result.push_back(QVariantMap{{QStringLiteral("order"), order},
                                     {QStringLiteral("magnitude"), magnitude},
                                     {QStringLiteral("percent"), percent}});
    }
    return result;
}

double AnalysisController::unitScaleToSi(int channelIndex) const {
    if (!m_document) return 1.0;
    const QString unit = normalized_unit(m_document->channelUnit(channelIndex));
    if (unit == QStringLiteral("KV") || unit == QStringLiteral("KA")) return 1.0e3;
    if (unit == QStringLiteral("MV")) return 1.0e6;
    return 1.0;
}

QVariantMap AnalysisController::impedanceAt(int voltageChannelIndex,
                                            int currentChannelIndex,
                                            double absoluteTimeSeconds) const {
    if (!m_document || voltageChannelIndex < 0 || currentChannelIndex < 0) {
        return {{QStringLiteral("valid"), false}};
    }

    std::complex<double> voltage = phasorComplex(voltageChannelIndex, absoluteTimeSeconds, 1)
                                   * unitScaleToSi(voltageChannelIndex);
    std::complex<double> current = phasorComplex(currentChannelIndex, absoluteTimeSeconds, 1)
                                   * unitScaleToSi(currentChannelIndex);
    if (std::abs(current) <= 1.0e-9) return {{QStringLiteral("valid"), false}};

    const std::complex<double> impedance = voltage / current;
    if (!std::isfinite(impedance.real()) || !std::isfinite(impedance.imag())) {
        return {{QStringLiteral("valid"), false}};
    }
    return {{QStringLiteral("valid"), true},
            {QStringLiteral("r"), impedance.real()},
            {QStringLiteral("x"), impedance.imag()},
            {QStringLiteral("magnitude"), std::abs(impedance)},
            {QStringLiteral("angle"), std::atan2(impedance.imag(), impedance.real()) * 180.0 / kPi}};
}
