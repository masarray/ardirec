// SPDX-License-Identifier: GPL-3.0-or-later
#include "cursor_snapshot_controller.hpp"

#include "ardirec/distance/distance.hpp"
#include "ardirec/power/symmetrical_components.hpp"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QVariantList>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

struct CursorChannelInfo {
    QString name;
    QString unit;
    QString phase;
    double displayScale{1.0};
    double siScale{1.0};
};

QString compact_name(QString value) {
    value = value.trimmed().toUpper();
    QString result;
    result.reserve(value.size());
    for (const QChar ch : value) {
        if (ch.isLetterOrNumber()) result.append(ch);
    }
    return result;
}

QString phase_from_name(const QString& rawName) {
    const QString name = compact_name(rawName);
    if (name.isEmpty()) return QStringLiteral("Other");
    if (name.contains(QStringLiteral("L1")) || name.endsWith(QStringLiteral("AN"))
        || name.endsWith(QStringLiteral("IA")) || name.endsWith(QStringLiteral("VA"))
        || name.endsWith(QStringLiteral("UA")) || name == QStringLiteral("A")) return QStringLiteral("L1");
    if (name.contains(QStringLiteral("L2")) || name.endsWith(QStringLiteral("BN"))
        || name.endsWith(QStringLiteral("IB")) || name.endsWith(QStringLiteral("VB"))
        || name.endsWith(QStringLiteral("UB")) || name == QStringLiteral("B")) return QStringLiteral("L2");
    if (name.contains(QStringLiteral("L3")) || name.endsWith(QStringLiteral("CN"))
        || name.endsWith(QStringLiteral("IC")) || name.endsWith(QStringLiteral("VC"))
        || name.endsWith(QStringLiteral("UC")) || name == QStringLiteral("C")) return QStringLiteral("L3");
    if (name.contains(QStringLiteral("3I0")) || name.contains(QStringLiteral("3V0"))
        || name.contains(QStringLiteral("3U0")) || name.contains(QStringLiteral("RES"))
        || name.contains(QStringLiteral("NEUTRAL")) || name.contains(QStringLiteral("GROUND"))
        || name.contains(QStringLiteral("EARTH")) || name.endsWith(QStringLiteral("IN"))
        || name.endsWith(QStringLiteral("VN")) || name.endsWith(QStringLiteral("UN"))
        || name.endsWith(QStringLiteral("IE")) || name.endsWith(QStringLiteral("VE"))
        || name.endsWith(QStringLiteral("UE")) || name == QStringLiteral("N")
        || name == QStringLiteral("E")) return QStringLiteral("E");
    return QStringLiteral("Other");
}

double unit_scale_to_si(QString unit) {
    unit = unit.trimmed().toUpper();
    unit.remove(' ');
    if (unit == QStringLiteral("KV") || unit == QStringLiteral("KA")) return 1.0e3;
    if (unit == QStringLiteral("MV")) return 1.0e6;
    return 1.0;
}

QVariantMap sequence_value(const QString& name,
                           const QString& sequence,
                           const QString& unit,
                           const std::complex<double>& value) {
    const double magnitude = std::abs(value);
    return {{QStringLiteral("valid"), std::isfinite(value.real()) && std::isfinite(value.imag()) && std::isfinite(magnitude)},
            {QStringLiteral("name"), name},
            {QStringLiteral("sequence"), sequence},
            {QStringLiteral("provenance"), QStringLiteral("DERIVED")},
            {QStringLiteral("unit"), unit},
            {QStringLiteral("magnitude"), magnitude},
            {QStringLiteral("angle"), std::atan2(value.imag(), value.real()) * 180.0 / kPi},
            {QStringLiteral("real"), value.real()},
            {QStringLiteral("imag"), value.imag()}};
}

QVariantMap invalid_sequence(const QString& role) {
    return {{QStringLiteral("valid"), false},
            {QStringLiteral("role"), role},
            {QStringLiteral("provenance"), QStringLiteral("DERIVED")}};
}

QVariantMap make_sequence(const QString& role,
                          const QString& prefix,
                          const std::array<int, 3>& indices,
                          const std::vector<std::complex<double>>& phasors,
                          const std::vector<CursorChannelInfo>& channels) {
    for (const int index : indices) {
        if (index < 0 || static_cast<std::size_t>(index) >= phasors.size()) return invalid_sequence(role);
        const auto& value = phasors[static_cast<std::size_t>(index)];
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag())) return invalid_sequence(role);
    }
    const QString unit = channels[static_cast<std::size_t>(indices[0])].unit;
    if (channels[static_cast<std::size_t>(indices[1])].unit.compare(unit, Qt::CaseInsensitive) != 0
        || channels[static_cast<std::size_t>(indices[2])].unit.compare(unit, Qt::CaseInsensitive) != 0) {
        return invalid_sequence(role);
    }

    const ardirec::power::ThreePhasePhasors phases{
        phasors[static_cast<std::size_t>(indices[0])],
        phasors[static_cast<std::size_t>(indices[1])],
        phasors[static_cast<std::size_t>(indices[2])],
    };
    const auto components = ardirec::power::symmetrical_components(phases);
    if (!components) return invalid_sequence(role);

    const double positive = std::abs(components->positive);
    const double floor = std::max(1.0e-12,
                                  std::max({std::abs(phases.l1), std::abs(phases.l2), std::abs(phases.l3)}) * 1.0e-12);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return {{QStringLiteral("valid"), true},
            {QStringLiteral("role"), role},
            {QStringLiteral("provenance"), QStringLiteral("DERIVED")},
            {QStringLiteral("unit"), unit},
            {QStringLiteral("sourceL1"), indices[0]},
            {QStringLiteral("sourceL2"), indices[1]},
            {QStringLiteral("sourceL3"), indices[2]},
            {QStringLiteral("positive"), sequence_value(prefix + QStringLiteral("1"), QStringLiteral("POSITIVE"), unit, components->positive)},
            {QStringLiteral("negative"), sequence_value(prefix + QStringLiteral("2"), QStringLiteral("NEGATIVE"), unit, components->negative)},
            {QStringLiteral("zero"), sequence_value(prefix + QStringLiteral("0"), QStringLiteral("ZERO"), unit, components->zero)},
            {QStringLiteral("negativePercent"), positive > floor ? std::abs(components->negative) / positive * 100.0 : nan},
            {QStringLiteral("zeroPercent"), positive > floor ? std::abs(components->zero) / positive * 100.0 : nan}};
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

QVariantMap invalid_distance(const QString& loop, double minimumCurrent) {
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

struct CursorSnapshotSource {
    std::shared_ptr<const ardirec::comtrade::IndexedDatFile> data;
    std::shared_ptr<const std::vector<double>> times;
    std::vector<CursorChannelInfo> channels;
    std::array<int, 3> voltage{{-1, -1, -1}};
    std::array<int, 3> current{{-1, -1, -1}};
    double nominalFrequency{50.0};
    double referenceTime{0.0};
    double currentFloor{1.0e-6};
};

namespace {
QVariantMap build_cursor_snapshot(const std::shared_ptr<const CursorSnapshotSource>& source,
                                  double absoluteTimeSeconds,
                                  const std::shared_ptr<std::atomic_bool>& cancel) {
    if (!source || !source->data || !source->times || source->times->empty()
        || source->channels.empty() || !std::isfinite(absoluteTimeSeconds)) {
        return {{QStringLiteral("valid"), false}};
    }

    const auto& times = *source->times;
    const double frequency = source->nominalFrequency > 1.0 ? source->nominalFrequency : 50.0;
    const auto [first, end] = one_cycle_window(times, absoluteTimeSeconds, frequency);
    if (first >= end || end - first < 4) return {{QStringLiteral("valid"), false}};

    const std::size_t channelCount = source->channels.size();
    std::vector<std::complex<long double>> accumulators(channelCount, {0.0L, 0.0L});
    std::vector<std::size_t> counts(channelCount, 0u);
    const double omega = 2.0 * kPi * frequency;

    for (std::size_t sample = first; sample < end; ++sample) {
        if (cancel && ((sample - first) & 7u) == 0u && cancel->load(std::memory_order_relaxed)) {
            return {{QStringLiteral("cancelled"), true}};
        }
        const long double angle = -static_cast<long double>(omega * (times[sample] - source->referenceTime));
        const std::complex<long double> basis{std::cos(angle), std::sin(angle)};
        for (std::size_t channel = 0; channel < channelCount; ++channel) {
            const double value = source->data->analogValue(sample, channel);
            if (!std::isfinite(value)) continue;
            accumulators[channel] += static_cast<long double>(value) * basis;
            ++counts[channel];
        }
    }

    if (cancel && cancel->load(std::memory_order_relaxed)) return {{QStringLiteral("cancelled"), true}};

    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(channelCount));
    std::vector<std::complex<double>> phasors(channelCount,
        {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        QVariantMap row{{QStringLiteral("index"), static_cast<int>(channel)},
                        {QStringLiteral("name"), source->channels[channel].name},
                        {QStringLiteral("unit"), source->channels[channel].unit},
                        {QStringLiteral("phase"), source->channels[channel].phase}};
        if (counts[channel] < 4) {
            row.insert(QStringLiteral("valid"), false);
            rows.push_back(row);
            continue;
        }
        const long double scale = std::sqrt(2.0L) / static_cast<long double>(counts[channel])
                                  * static_cast<long double>(source->channels[channel].displayScale);
        const auto value = accumulators[channel] * scale;
        const std::complex<double> phasor{static_cast<double>(value.real()), static_cast<double>(value.imag())};
        const double magnitude = std::abs(phasor);
        if (!std::isfinite(phasor.real()) || !std::isfinite(phasor.imag()) || !std::isfinite(magnitude)) {
            row.insert(QStringLiteral("valid"), false);
            rows.push_back(row);
            continue;
        }
        phasors[channel] = phasor;
        row.insert(QStringLiteral("valid"), true);
        row.insert(QStringLiteral("magnitude"), magnitude);
        row.insert(QStringLiteral("angle"), std::atan2(phasor.imag(), phasor.real()) * 180.0 / kPi);
        row.insert(QStringLiteral("real"), phasor.real());
        row.insert(QStringLiteral("imag"), phasor.imag());
        row.insert(QStringLiteral("realSI"), phasor.real() * source->channels[channel].siScale);
        row.insert(QStringLiteral("imagSI"), phasor.imag() * source->channels[channel].siScale);
        rows.push_back(row);
    }

    QVariantMap semantics{{QStringLiteral("vL1"), source->voltage[0]},
                          {QStringLiteral("vL2"), source->voltage[1]},
                          {QStringLiteral("vL3"), source->voltage[2]},
                          {QStringLiteral("iL1"), source->current[0]},
                          {QStringLiteral("iL2"), source->current[1]},
                          {QStringLiteral("iL3"), source->current[2]}};

    return {{QStringLiteral("valid"), true},
            {QStringLiteral("time"), std::clamp(absoluteTimeSeconds, times.front(), times.back())},
            {QStringLiteral("windowFirst"), static_cast<qulonglong>(first)},
            {QStringLiteral("windowEnd"), static_cast<qulonglong>(end)},
            {QStringLiteral("channels"), rows},
            {QStringLiteral("semantics"), semantics},
            {QStringLiteral("currentFloor"), source->currentFloor},
            {QStringLiteral("voltageSequence"), make_sequence(QStringLiteral("Voltage"), QStringLiteral("V"), source->voltage, phasors, source->channels)},
            {QStringLiteral("currentSequence"), make_sequence(QStringLiteral("Current"), QStringLiteral("I"), source->current, phasors, source->channels)}};
}
} // namespace

CursorSnapshotController::CursorSnapshotController(DocumentController* document, QObject* parent)
    : QObject(parent), m_document(document) {
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, [this]() {
            rebuildSource();
            m_cursorA.clear();
            m_cursorB.clear();
            emit cursorAChanged();
            emit cursorBChanged();
        });
        connect(m_document, &DocumentController::representationChanged, this, [this]() {
            rebuildSource();
            if (m_haveLastA) requestCursorA(m_lastTimeA);
            if (m_haveLastB) requestCursorB(m_lastTimeB);
        });
    }
    rebuildSource();
}

CursorSnapshotController::~CursorSnapshotController() {
    cancel(1);
    cancel(2);
}

void CursorSnapshotController::rebuildSource() {
    cancel(1);
    cancel(2);
    if (!m_document || !m_document->dataStoreSnapshot() || !m_document->timeIndexSnapshot()
        || m_document->analogCount() <= 0) {
        m_source.reset();
        return;
    }

    auto source = std::make_shared<CursorSnapshotSource>();
    source->data = m_document->dataStoreSnapshot();
    source->times = m_document->timeIndexSnapshot();
    source->nominalFrequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    source->referenceTime = m_document->dataStartSeconds();
    source->channels.reserve(static_cast<std::size_t>(m_document->analogCount()));

    double currentPeak = 0.0;
    for (int index = 0; index < m_document->analogCount(); ++index) {
        CursorChannelInfo info;
        info.name = m_document->channelName(index);
        info.unit = m_document->channelUnit(index);
        info.phase = phase_from_name(info.name);
        info.displayScale = m_document->channelDisplayScale(index);
        info.siScale = unit_scale_to_si(info.unit);
        source->channels.push_back(info);

        const QString role = m_document->analogRole(index);
        int phaseSlot = -1;
        if (info.phase == QStringLiteral("L1")) phaseSlot = 0;
        else if (info.phase == QStringLiteral("L2")) phaseSlot = 1;
        else if (info.phase == QStringLiteral("L3")) phaseSlot = 2;
        if (phaseSlot >= 0) {
            if (role == QStringLiteral("Voltage") && source->voltage[static_cast<std::size_t>(phaseSlot)] < 0)
                source->voltage[static_cast<std::size_t>(phaseSlot)] = index;
            if (role == QStringLiteral("Current") && source->current[static_cast<std::size_t>(phaseSlot)] < 0)
                source->current[static_cast<std::size_t>(phaseSlot)] = index;
        }
        if (role == QStringLiteral("Current") && phaseSlot >= 0) {
            const double peak = std::abs(m_document->channelPeak(index) * info.siScale);
            if (std::isfinite(peak)) currentPeak = std::max(currentPeak, peak);
        }
    }
    source->currentFloor = std::isfinite(currentPeak) && currentPeak > 0.0
                               ? std::max(1.0e-6, currentPeak * 1.0e-3)
                               : 1.0e-6;
    m_source = std::move(source);
}

void CursorSnapshotController::cancel(int cursor) noexcept {
    auto& token = cursor == 1 ? m_cancelA : m_cancelB;
    if (token) token->store(true, std::memory_order_relaxed);
    token.reset();
}

void CursorSnapshotController::requestCursorA(double absoluteTimeSeconds) {
    m_lastTimeA = absoluteTimeSeconds;
    m_haveLastA = true;
    request(1, absoluteTimeSeconds);
}

void CursorSnapshotController::requestCursorB(double absoluteTimeSeconds) {
    m_lastTimeB = absoluteTimeSeconds;
    m_haveLastB = true;
    request(2, absoluteTimeSeconds);
}

void CursorSnapshotController::request(int cursor, double absoluteTimeSeconds) {
    if (!m_source || !std::isfinite(absoluteTimeSeconds)) return;
    cancel(cursor);
    auto cancelToken = std::make_shared<std::atomic_bool>(false);
    quint64 generation = 0;
    if (cursor == 1) {
        m_cancelA = cancelToken;
        generation = ++m_generationA;
        if (!m_busyA) { m_busyA = true; emit busyAChanged(); }
    } else {
        m_cancelB = cancelToken;
        generation = ++m_generationB;
        if (!m_busyB) { m_busyB = true; emit busyBChanged(); }
    }
    const auto source = m_source;
    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, cursor, generation, cancelToken]() {
                const QVariantMap snapshot = watcher->result();
                watcher->deleteLater();
                if (cancelToken->load(std::memory_order_relaxed)) return;
                publish(cursor, generation, snapshot);
            });
    watcher->setFuture(QtConcurrent::run([source, absoluteTimeSeconds, cancelToken]() {
        return build_cursor_snapshot(source, absoluteTimeSeconds, cancelToken);
    }));
}

void CursorSnapshotController::publish(int cursor, quint64 generation, const QVariantMap& snapshot) {
    if (snapshot.value(QStringLiteral("cancelled")).toBool()) return;
    if (cursor == 1) {
        if (generation != m_generationA) return;
        m_cursorA = snapshot;
        m_cancelA.reset();
        if (m_busyA) { m_busyA = false; emit busyAChanged(); }
        emit cursorAChanged();
    } else {
        if (generation != m_generationB) return;
        m_cursorB = snapshot;
        m_cancelB.reset();
        if (m_busyB) { m_busyB = false; emit busyBChanged(); }
        emit cursorBChanged();
    }
}

QVariantMap CursorSnapshotController::distanceLoopsForSnapshot(const QVariantMap& snapshot,
                                                               double groundingFactorMagnitude,
                                                               double groundingFactorAngleDegrees) const {
    QVariantMap values;
    if (!snapshot.value(QStringLiteral("valid")).toBool()
        || !std::isfinite(groundingFactorMagnitude) || !std::isfinite(groundingFactorAngleDegrees)) return values;

    const QVariantList rows = snapshot.value(QStringLiteral("channels")).toList();
    const QVariantMap semantics = snapshot.value(QStringLiteral("semantics")).toMap();
    const std::array<int, 3> voltage{{semantics.value(QStringLiteral("vL1"), -1).toInt(),
                                      semantics.value(QStringLiteral("vL2"), -1).toInt(),
                                      semantics.value(QStringLiteral("vL3"), -1).toInt()}};
    const std::array<int, 3> current{{semantics.value(QStringLiteral("iL1"), -1).toInt(),
                                      semantics.value(QStringLiteral("iL2"), -1).toInt(),
                                      semantics.value(QStringLiteral("iL3"), -1).toInt()}};

    auto rowPhasor = [&rows](int index) -> std::complex<double> {
        if (index < 0 || index >= rows.size()) return {};
        const QVariantMap row = rows.at(index).toMap();
        if (!row.value(QStringLiteral("valid")).toBool()) return {};
        return {row.value(QStringLiteral("realSI")).toDouble(), row.value(QStringLiteral("imagSI")).toDouble()};
    };

    ardirec::distance::ThreePhasePhasors phasors;
    for (std::size_t phase = 0; phase < 3; ++phase) {
        phasors.voltage[phase] = rowPhasor(voltage[phase]);
        phasors.current[phase] = rowPhasor(current[phase]);
    }

    const double minimumCurrent = snapshot.value(QStringLiteral("currentFloor"), 1.0e-6).toDouble();
    const double angleRadians = groundingFactorAngleDegrees * kPi / 180.0;
    const auto groundingFactor = std::polar(std::max(0.0, groundingFactorMagnitude), angleRadians);

    struct LoopDef { const char* id; ardirec::distance::FaultLoop loop; };
    constexpr std::array<LoopDef, 6> loops{{
        {"L1-E", ardirec::distance::FaultLoop::L1E}, {"L2-E", ardirec::distance::FaultLoop::L2E},
        {"L3-E", ardirec::distance::FaultLoop::L3E}, {"L1-L2", ardirec::distance::FaultLoop::L1L2},
        {"L2-L3", ardirec::distance::FaultLoop::L2L3}, {"L3-L1", ardirec::distance::FaultLoop::L3L1},
    }};

    auto available = [&voltage, &current](ardirec::distance::FaultLoop loop) {
        switch (loop) {
        case ardirec::distance::FaultLoop::L1E: return voltage[0] >= 0 && current[0] >= 0 && current[1] >= 0 && current[2] >= 0;
        case ardirec::distance::FaultLoop::L2E: return voltage[1] >= 0 && current[0] >= 0 && current[1] >= 0 && current[2] >= 0;
        case ardirec::distance::FaultLoop::L3E: return voltage[2] >= 0 && current[0] >= 0 && current[1] >= 0 && current[2] >= 0;
        case ardirec::distance::FaultLoop::L1L2: return voltage[0] >= 0 && voltage[1] >= 0 && current[0] >= 0 && current[1] >= 0;
        case ardirec::distance::FaultLoop::L2L3: return voltage[1] >= 0 && voltage[2] >= 0 && current[1] >= 0 && current[2] >= 0;
        case ardirec::distance::FaultLoop::L3L1: return voltage[2] >= 0 && voltage[0] >= 0 && current[2] >= 0 && current[0] >= 0;
        }
        return false;
    };

    for (const auto& definition : loops) {
        const QString loopId = QString::fromLatin1(definition.id);
        if (!available(definition.loop)) {
            values.insert(loopId, invalid_distance(loopId, minimumCurrent));
            continue;
        }
        const auto result = ardirec::distance::distance_impedance(definition.loop, phasors,
                                                                   groundingFactor, minimumCurrent);
        if (!result.valid) {
            values.insert(loopId, invalid_distance(loopId, minimumCurrent));
            continue;
        }
        const auto z = result.impedance;
        values.insert(loopId, QVariantMap{{QStringLiteral("valid"), true},
                                          {QStringLiteral("r"), z.real()},
                                          {QStringLiteral("x"), z.imag()},
                                          {QStringLiteral("magnitude"), std::abs(z)},
                                          {QStringLiteral("angle"), std::atan2(z.imag(), z.real()) * 180.0 / kPi},
                                          {QStringLiteral("measuringCurrent"), std::abs(result.measuring_current)},
                                          {QStringLiteral("minimumCurrent"), minimumCurrent},
                                          {QStringLiteral("loop"), loopId}});
    }
    return values;
}
