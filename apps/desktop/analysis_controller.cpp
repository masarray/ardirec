// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

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

QVariantMap invalid_distance() {
    return {{QStringLiteral("valid"), false},
            {QStringLiteral("r"), 0.0},
            {QStringLiteral("x"), 0.0},
            {QStringLiteral("magnitude"), 0.0},
            {QStringLiteral("angle"), 0.0},
            {QStringLiteral("measuringCurrent"), 0.0}};
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
    const double recordedRms = std::sqrt(static_cast<double>(sumSquares / static_cast<long double>(count)));
    return recordedRms * std::abs(m_document->channelDisplayScale(channelIndex));
}

QString AnalysisController::formatEngineeringValue(double value, int channelIndex) const {
    return m_document ? m_document->formatChannelValue(channelIndex, value) : QStringLiteral("—");
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

    const long double dftScale = std::sqrt(2.0L) / static_cast<long double>(count);
    accumulator *= dftScale * static_cast<long double>(m_document->channelDisplayScale(channelIndex));
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

ardirec::power::HarmonicSpectrum AnalysisController::harmonicSpectrum(int channelIndex,
                                                                      double absoluteTimeSeconds,
                                                                      int maximumOrder) const {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) return {};

    const auto& samples = m_document->analogSamples(channelIndex);
    const auto& times = m_document->timeSeconds();
    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    const std::size_t cappedEnd = std::min({end, samples.size(), times.size()});
    if (first >= cappedEnd || cappedEnd - first < 4) return {};

    const double frequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    const std::size_t count = cappedEnd - first;
    return ardirec::power::harmonic_spectrum(std::span<const double>(samples.data() + first, count),
                                             std::span<const double>(times.data() + first, count),
                                             frequency,
                                             maximumOrder,
                                             m_document->dataStartSeconds());
}

QVariantMap AnalysisController::harmonicSpectrumAt(int channelIndex,
                                                   double absoluteTimeSeconds,
                                                   int maximumOrder) const {
    QVariantList bins;
    if (!m_document) return {{QStringLiteral("valid"), false}, {QStringLiteral("bins"), bins}};

    const auto spectrum = harmonicSpectrum(channelIndex, absoluteTimeSeconds, maximumOrder);
    if (!spectrum.valid) return {{QStringLiteral("valid"), false}, {QStringLiteral("bins"), bins}};

    const double representationScale = std::abs(m_document->channelDisplayScale(channelIndex));
    const double recordedFundamental = spectrum.fundamental_rms;
    for (const auto& bin : spectrum.bins) {
        const double percent = recordedFundamental > 1.0e-12
                                   ? bin.magnitude_rms / recordedFundamental * 100.0
                                   : 0.0;
        bins.push_back(QVariantMap{{QStringLiteral("order"), bin.order},
                                   {QStringLiteral("magnitude"), bin.magnitude_rms * representationScale},
                                   {QStringLiteral("percent"), percent},
                                   {QStringLiteral("angle"), bin.angle_degrees}});
    }

    return {{QStringLiteral("valid"), true},
            {QStringLiteral("fundamental"), spectrum.fundamental_rms * representationScale},
            {QStringLiteral("thdPercent"), spectrum.thd_percent},
            {QStringLiteral("dominantOrder"), spectrum.dominant_order},
            {QStringLiteral("dominantMagnitude"), spectrum.dominant_rms * representationScale},
            {QStringLiteral("dominantPercent"), spectrum.dominant_percent},
            {QStringLiteral("bins"), bins},
            {QStringLiteral("unit"), m_document->channelUnit(channelIndex)},
            {QStringLiteral("name"), m_document->channelName(channelIndex)},
            {QStringLiteral("phase"), channelPhase(channelIndex)}};
}

QVariantList AnalysisController::harmonicsAt(int channelIndex,
                                             double absoluteTimeSeconds,
                                             int maximumOrder) const {
    return harmonicSpectrumAt(channelIndex, absoluteTimeSeconds, maximumOrder)
        .value(QStringLiteral("bins"))
        .toList();
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

bool AnalysisController::distanceLoopAvailable(const QString& loopId) const {
    if (!m_document) return false;
    const auto loop = ardirec::distance::fault_loop_from_id(loopId.toStdString());
    const int v1 = phaseChannel(QStringLiteral("Voltage"), QStringLiteral("L1"));
    const int v2 = phaseChannel(QStringLiteral("Voltage"), QStringLiteral("L2"));
    const int v3 = phaseChannel(QStringLiteral("Voltage"), QStringLiteral("L3"));
    const int i1 = phaseChannel(QStringLiteral("Current"), QStringLiteral("L1"));
    const int i2 = phaseChannel(QStringLiteral("Current"), QStringLiteral("L2"));
    const int i3 = phaseChannel(QStringLiteral("Current"), QStringLiteral("L3"));

    switch (loop) {
    case ardirec::distance::FaultLoop::L1E: return v1 >= 0 && i1 >= 0 && i2 >= 0 && i3 >= 0;
    case ardirec::distance::FaultLoop::L2E: return v2 >= 0 && i1 >= 0 && i2 >= 0 && i3 >= 0;
    case ardirec::distance::FaultLoop::L3E: return v3 >= 0 && i1 >= 0 && i2 >= 0 && i3 >= 0;
    case ardirec::distance::FaultLoop::L1L2: return v1 >= 0 && v2 >= 0 && i1 >= 0 && i2 >= 0;
    case ardirec::distance::FaultLoop::L2L3: return v2 >= 0 && v3 >= 0 && i2 >= 0 && i3 >= 0;
    case ardirec::distance::FaultLoop::L3L1: return v3 >= 0 && v1 >= 0 && i3 >= 0 && i1 >= 0;
    }
    return false;
}

bool AnalysisController::distancePhasors(ardirec::distance::FaultLoop loop,
                                         double absoluteTimeSeconds,
                                         ardirec::distance::ThreePhasePhasors& phasors) const {
    if (!m_document || !distanceLoopAvailable(QString::fromUtf8(ardirec::distance::fault_loop_id(loop).data(),
                                                                static_cast<qsizetype>(ardirec::distance::fault_loop_id(loop).size())))) {
        return false;
    }

    for (int phase = 0; phase < 3; ++phase) {
        const QString phaseName = QStringLiteral("L%1").arg(phase + 1);
        const int voltageChannel = phaseChannel(QStringLiteral("Voltage"), phaseName);
        const int currentChannel = phaseChannel(QStringLiteral("Current"), phaseName);
        if (voltageChannel >= 0) {
            phasors.voltage[static_cast<std::size_t>(phase)] = phasorComplex(voltageChannel, absoluteTimeSeconds, 1)
                                                                  * unitScaleToSi(voltageChannel);
        }
        if (currentChannel >= 0) {
            phasors.current[static_cast<std::size_t>(phase)] = phasorComplex(currentChannel, absoluteTimeSeconds, 1)
                                                                  * unitScaleToSi(currentChannel);
        }
    }
    return true;
}

QVariantMap AnalysisController::distanceLoopAt(const QString& loopId,
                                               double absoluteTimeSeconds,
                                               double groundingFactorMagnitude,
                                               double groundingFactorAngleDegrees) const {
    if (!m_document || !std::isfinite(groundingFactorMagnitude) || !std::isfinite(groundingFactorAngleDegrees)) {
        return invalid_distance();
    }
    const auto loop = ardirec::distance::fault_loop_from_id(loopId.toStdString());
    ardirec::distance::ThreePhasePhasors phasors;
    if (!distancePhasors(loop, absoluteTimeSeconds, phasors)) return invalid_distance();

    const double angleRadians = groundingFactorAngleDegrees * kPi / 180.0;
    const std::complex<double> groundingFactor = std::polar(std::max(0.0, groundingFactorMagnitude), angleRadians);
    const auto result = ardirec::distance::distance_impedance(loop, phasors, groundingFactor);
    if (!result.valid) return invalid_distance();

    const auto impedance = result.impedance;
    return {{QStringLiteral("valid"), true},
            {QStringLiteral("r"), impedance.real()},
            {QStringLiteral("x"), impedance.imag()},
            {QStringLiteral("magnitude"), std::abs(impedance)},
            {QStringLiteral("angle"), std::atan2(impedance.imag(), impedance.real()) * 180.0 / kPi},
            {QStringLiteral("measuringCurrent"), std::abs(result.measuring_current)},
            {QStringLiteral("loop"), loopId}};
}

QVariantList AnalysisController::distanceLocus(const QString& loopId,
                                               double viewStartSeconds,
                                               double visibleDurationSeconds,
                                               int steps,
                                               double groundingFactorMagnitude,
                                               double groundingFactorAngleDegrees) const {
    QVariantList points;
    if (!m_document || visibleDurationSeconds <= 0.0 || !std::isfinite(viewStartSeconds)
        || !std::isfinite(visibleDurationSeconds)) {
        return points;
    }
    steps = std::clamp(steps, 16, 240);
    points.reserve(steps);
    for (int index = 0; index < steps; ++index) {
        const double fraction = static_cast<double>(index) / static_cast<double>(std::max(1, steps - 1));
        const double time = viewStartSeconds + visibleDurationSeconds * fraction;
        const QVariantMap value = distanceLoopAt(loopId,
                                                 time,
                                                 groundingFactorMagnitude,
                                                 groundingFactorAngleDegrees);
        if (!value.value(QStringLiteral("valid")).toBool()) continue;
        QVariantMap point = value;
        point.insert(QStringLiteral("time"), time);
        points.push_back(point);
    }
    return points;
}
