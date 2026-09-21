// SPDX-License-Identifier: GPL-3.0-or-later
#include "cursor_snapshot_controller.hpp"

#include "ardirec/distance/distance.hpp"
#include "ardirec/power/symmetrical_components.hpp"
#include "ardirec/power/timestamped_dft.hpp"
#include "distance_compensation.hpp"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QVariantList>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <optional>
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

bool window_has_status_change(const std::vector<double>& edges,
                              double startSeconds,
                              double endSeconds) {
    if (edges.empty() || !std::isfinite(startSeconds) || !std::isfinite(endSeconds)
        || endSeconds < startSeconds) return false;
    constexpr double epsilon = 1.0e-10;
    const auto it = std::lower_bound(edges.begin(), edges.end(), startSeconds - epsilon);
    return it != edges.end() && *it <= endSeconds + epsilon;
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
    std::shared_ptr<const std::vector<double>> statusEdges;
    std::vector<CursorChannelInfo> channels;
    std::array<int, 3> voltage{{-1, -1, -1}};
    std::array<int, 3> current{{-1, -1, -1}};
    int residualCurrent{-1};
    double residualToSumMultiplier{1.0};
    ardirec::desktop::ClassicalGroundingFactors classicalGrounding;
    double calculationFrequency{50.0};
    QString calculationFrequencyProvenance{QStringLiteral("COMTRADE nominal")};
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
    const double frequency = source->calculationFrequency > 1.0 ? source->calculationFrequency : 50.0;
    const auto window = ardirec::power::trailing_cycle_window(times, absoluteTimeSeconds, frequency);
    if (!window.valid() || window.end - window.first < 4u) return {{QStringLiteral("valid"), false}};
    const bool statusWindowValid = !source->statusEdges
                                   || !window_has_status_change(*source->statusEdges,
                                                                window.start_seconds,
                                                                window.end_seconds);

    const std::size_t channelCount = source->channels.size();
    std::vector<std::complex<long double>> accumulators(channelCount, {0.0L, 0.0L});
    std::vector<long double> weightSums(channelCount, 0.0L);
    std::vector<std::size_t> counts(channelCount, 0u);
    const double omega = 2.0 * kPi * frequency;

    for (std::size_t sample = window.first; sample < window.end; ++sample) {
        if (cancel && ((sample - window.first) & 7u) == 0u && cancel->load(std::memory_order_relaxed)) {
            return {{QStringLiteral("cancelled"), true}};
        }
        const double weight = ardirec::power::timestamp_cell_weight(times, window, sample);
        if (!(weight > 0.0) || !std::isfinite(weight)) continue;
        const long double angle = -static_cast<long double>(omega * (times[sample] - source->referenceTime));
        const std::complex<long double> basis{std::cos(angle), std::sin(angle)};
        const long double weighted = static_cast<long double>(weight);
        for (std::size_t channel = 0; channel < channelCount; ++channel) {
            const double value = source->data->analogValue(sample, channel);
            if (!std::isfinite(value)) continue;
            accumulators[channel] += static_cast<long double>(value) * basis * weighted;
            weightSums[channel] += weighted;
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
        if (counts[channel] < 4u || !(weightSums[channel] > 0.0L)) {
            row.insert(QStringLiteral("valid"), false);
            rows.push_back(row);
            continue;
        }
        const long double scale = std::sqrt(2.0L) / weightSums[channel]
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
                          {QStringLiteral("iL3"), source->current[2]},
                          {QStringLiteral("iResidual"), source->residualCurrent},
                          {QStringLiteral("residualToSumMultiplier"), source->residualToSumMultiplier}};

    return {{QStringLiteral("valid"), true},
            {QStringLiteral("time"), std::clamp(absoluteTimeSeconds, times.front(), times.back())},
            {QStringLiteral("windowFirst"), static_cast<qulonglong>(window.first)},
            {QStringLiteral("windowEnd"), static_cast<qulonglong>(window.end)},
            {QStringLiteral("windowStart"), window.start_seconds},
            {QStringLiteral("windowFinish"), window.end_seconds},
            {QStringLiteral("statusWindowValid"), statusWindowValid},
            {QStringLiteral("calculationFrequency"), frequency},
            {QStringLiteral("calculationFrequencyProvenance"), source->calculationFrequencyProvenance},
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
    ++m_sourceRevision;
    cancel(1, true);
    cancel(2, true);
    if (!m_document || !m_document->dataStoreSnapshot() || !m_document->timeIndexSnapshot()
        || m_document->analogCount() <= 0) {
        m_source.reset();
        return;
    }

    auto source = std::make_shared<CursorSnapshotSource>();
    source->data = m_document->dataStoreSnapshot();
    source->times = m_document->timeIndexSnapshot();
    source->statusEdges = std::make_shared<const std::vector<double>>(m_document->digitalEdgeTimes());
    source->classicalGrounding = ardirec::desktop::read_classical_grounding_factors(m_document->distanceZonePath());
    source->calculationFrequency = m_document->calculationFrequency() > 1.0
                                       ? m_document->calculationFrequency()
                                       : (m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0);
    source->calculationFrequencyProvenance = m_document->calculationFrequencyProvenance();
    source->referenceTime = m_document->dataStartSeconds();
    source->channels.reserve(static_cast<std::size_t>(m_document->analogCount()));

    double currentPeak = 0.0;
    for (int index = 0; index < m_document->analogCount(); ++index) {
        CursorChannelInfo info;
        info.name = m_document->channelName(index);
        info.unit = m_document->channelUnit(index);
        info.phase = m_document->channelPhase(index);
        if (info.phase == QStringLiteral("Other")) info.phase = phase_from_name(info.name);
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
        } else if (role == QStringLiteral("Current") && info.phase == QStringLiteral("E") && source->residualCurrent < 0) {
            const auto multiplier = residual_to_sum_multiplier(info.name);
            if (multiplier) {
                source->residualCurrent = index;
                source->residualToSumMultiplier = *multiplier;
            }
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

void CursorSnapshotController::cancel(int cursor, bool clearBusy) noexcept {
    auto& token = cursor == 1 ? m_cancelA : m_cancelB;
    if (token) token->store(true, std::memory_order_relaxed);
    token.reset();
    if (cursor == 1) m_haveInFlightA = false;
    else m_haveInFlightB = false;
    if (!clearBusy) return;
    if (cursor == 1) {
        if (m_busyA) {
            m_busyA = false;
            emit busyAChanged();
        }
    } else if (m_busyB) {
        m_busyB = false;
        emit busyBChanged();
    }
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

    constexpr double sameTimeTolerance = 1.0e-10;
    const QVariantMap& committed = cursor == 1 ? m_cursorA : m_cursorB;
    const quint64 committedRevision = cursor == 1 ? m_snapshotSourceRevisionA : m_snapshotSourceRevisionB;
    const double committedTime = committed.value(QStringLiteral("time"),
                                                 std::numeric_limits<double>::quiet_NaN()).toDouble();
    if (committedRevision == m_sourceRevision
        && committed.value(QStringLiteral("valid")).toBool()
        && std::isfinite(committedTime)
        && std::abs(committedTime - absoluteTimeSeconds) <= sameTimeTolerance) {
        return;
    }

    const bool busy = cursor == 1 ? m_busyA : m_busyB;
    const bool haveInFlight = cursor == 1 ? m_haveInFlightA : m_haveInFlightB;
    const double inFlightTime = cursor == 1 ? m_inFlightTimeA : m_inFlightTimeB;
    const quint64 inFlightRevision = cursor == 1 ? m_inFlightSourceRevisionA : m_inFlightSourceRevisionB;
    if (busy && haveInFlight && inFlightRevision == m_sourceRevision
        && std::abs(inFlightTime - absoluteTimeSeconds) <= sameTimeTolerance) {
        // Multiple Phasor/Sequence consumers may ask for the same immutable
        // fundamental frame during first construction. Do not cancel and restart
        // an identical one-cycle DFT that is already in flight.
        return;
    }

    cancel(cursor);
    auto cancelToken = std::make_shared<std::atomic_bool>(false);
    quint64 generation = 0;
    const quint64 sourceRevision = m_sourceRevision;
    if (cursor == 1) {
        m_cancelA = cancelToken;
        m_inFlightTimeA = absoluteTimeSeconds;
        m_inFlightSourceRevisionA = sourceRevision;
        m_haveInFlightA = true;
        generation = ++m_generationA;
        ++m_launchedJobsA;
        if (!m_busyA) { m_busyA = true; emit busyAChanged(); }
    } else {
        m_cancelB = cancelToken;
        m_inFlightTimeB = absoluteTimeSeconds;
        m_inFlightSourceRevisionB = sourceRevision;
        m_haveInFlightB = true;
        generation = ++m_generationB;
        ++m_launchedJobsB;
        if (!m_busyB) { m_busyB = true; emit busyBChanged(); }
    }
    const auto source = m_source;
    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, cursor, generation, sourceRevision, cancelToken]() {
                const QVariantMap snapshot = watcher->result();
                watcher->deleteLater();
                if (cancelToken->load(std::memory_order_relaxed)) return;
                publish(cursor, generation, sourceRevision, snapshot);
            });
    watcher->setFuture(QtConcurrent::run([source, absoluteTimeSeconds, cancelToken]() {
        return build_cursor_snapshot(source, absoluteTimeSeconds, cancelToken);
    }));
}

void CursorSnapshotController::publish(int cursor, quint64 generation, quint64 sourceRevision,
                                       const QVariantMap& snapshot) {
    if (snapshot.value(QStringLiteral("cancelled")).toBool() || sourceRevision != m_sourceRevision) return;
    if (cursor == 1) {
        if (generation != m_generationA) return;
        m_cursorA = snapshot;
        m_snapshotSourceRevisionA = sourceRevision;
        m_haveInFlightA = false;
        m_cancelA.reset();
        if (m_busyA) { m_busyA = false; emit busyAChanged(); }
        emit cursorAChanged();
    } else {
        if (generation != m_generationB) return;
        m_cursorB = snapshot;
        m_snapshotSourceRevisionB = sourceRevision;
        m_haveInFlightB = false;
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
    const int residualIndex = semantics.value(QStringLiteral("iResidual"), -1).toInt();
    const double residualMultiplier = semantics.value(QStringLiteral("residualToSumMultiplier"), 1.0).toDouble();

    auto rowPhasor = [&rows](int index) -> std::optional<std::complex<double>> {
        if (index < 0 || index >= rows.size()) return std::nullopt;
        const QVariantMap row = rows.at(index).toMap();
        if (!row.value(QStringLiteral("valid")).toBool()) return std::nullopt;
        const std::complex<double> value{row.value(QStringLiteral("realSI")).toDouble(),
                                         row.value(QStringLiteral("imagSI")).toDouble()};
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag())) return std::nullopt;
        return value;
    };

    ardirec::distance::ThreePhasePhasors phasors;
    std::array<bool, 3> voltageValid{};
    std::array<bool, 3> currentValid{};
    for (std::size_t phase = 0; phase < 3; ++phase) {
        if (const auto value = rowPhasor(voltage[phase])) {
            phasors.voltage[phase] = *value;
            voltageValid[phase] = true;
        }
        if (const auto value = rowPhasor(current[phase])) {
            phasors.current[phase] = *value;
            currentValid[phase] = true;
        }
    }

    std::optional<std::complex<double>> measuredResidual;
    if (const auto value = rowPhasor(residualIndex)) measuredResidual = *value * residualMultiplier;

    const double minimumCurrent = snapshot.value(QStringLiteral("currentFloor"), 1.0e-6).toDouble();
    const double angleRadians = groundingFactorAngleDegrees * kPi / 180.0;
    const auto groundingFactor = std::polar(std::max(0.0, groundingFactorMagnitude), angleRadians);

    struct LoopDef { const char* id; ardirec::distance::FaultLoop loop; };
    constexpr std::array<LoopDef, 6> loops{{
        {"L1-E", ardirec::distance::FaultLoop::L1E}, {"L2-E", ardirec::distance::FaultLoop::L2E},
        {"L3-E", ardirec::distance::FaultLoop::L3E}, {"L1-L2", ardirec::distance::FaultLoop::L1L2},
        {"L2-L3", ardirec::distance::FaultLoop::L2L3}, {"L3-L1", ardirec::distance::FaultLoop::L3L1},
    }};

    const bool statusWindowValid = snapshot.value(QStringLiteral("statusWindowValid"), true).toBool();
    auto available = [&voltageValid, &currentValid, &measuredResidual](ardirec::distance::FaultLoop loop) {
        const bool residualAvailable = measuredResidual.has_value()
                                       || (currentValid[0] && currentValid[1] && currentValid[2]);
        switch (loop) {
        case ardirec::distance::FaultLoop::L1E: return voltageValid[0] && currentValid[0] && residualAvailable;
        case ardirec::distance::FaultLoop::L2E: return voltageValid[1] && currentValid[1] && residualAvailable;
        case ardirec::distance::FaultLoop::L3E: return voltageValid[2] && currentValid[2] && residualAvailable;
        case ardirec::distance::FaultLoop::L1L2: return voltageValid[0] && voltageValid[1] && currentValid[0] && currentValid[1];
        case ardirec::distance::FaultLoop::L2L3: return voltageValid[1] && voltageValid[2] && currentValid[1] && currentValid[2];
        case ardirec::distance::FaultLoop::L3L1: return voltageValid[2] && voltageValid[0] && currentValid[2] && currentValid[0];
        }
        return false;
    };

    const auto classical = m_source ? m_source->classicalGrounding
                                    : ardirec::desktop::ClassicalGroundingFactors{};
    for (const auto& definition : loops) {
        const QString loopId = QString::fromLatin1(definition.id);
        if (!statusWindowValid || !available(definition.loop)) {
            values.insert(loopId, invalid_distance(loopId, minimumCurrent));
            continue;
        }

        ardirec::distance::DistanceImpedance result;
        if (ardirec::distance::is_earth_loop(definition.loop) && classical.valid) {
            const std::complex<double> residual = measuredResidual.value_or(
                phasors.current[0] + phasors.current[1] + phasors.current[2]);
            result = ardirec::distance::distance_impedance_rerl_xexl(
                definition.loop, phasors, -residual,
                classical.re_over_rl, classical.xe_over_xl, minimumCurrent);
        } else {
            result = ardirec::distance::distance_impedance(
                definition.loop, phasors, groundingFactor, minimumCurrent, measuredResidual);
        }
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
