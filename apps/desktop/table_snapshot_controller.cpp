// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_snapshot_controller.hpp"

#include "ardirec/power/harmonics.hpp"
#include "ardirec/power/waveform_metrics.hpp"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

namespace {
constexpr double kMinimumMagnitude = 1.0e-12;
constexpr double kAbnormalThdPercent = 5.0;
constexpr double kAbnormalDcPercent = 5.0;
constexpr double kAbnormalCrestFactor = 2.0;

QString compact_name(QString value) {
    value = value.trimmed().toUpper();
    value.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    return value;
}

double wrap_degrees(double angle) {
    while (angle <= -180.0) angle += 360.0;
    while (angle > 180.0) angle -= 360.0;
    return angle;
}
} // namespace

TableSnapshotController::TableSnapshotController(DocumentController* document, QObject* parent)
    : QObject(parent), m_document(document) {
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged,
                this, &TableSnapshotController::clearCache);
        connect(m_document, &DocumentController::representationChanged,
                this, &TableSnapshotController::clearCache);
    }
}

void TableSnapshotController::clearCache() {
    m_cache.clear();
}

std::pair<std::size_t, std::size_t>
TableSnapshotController::oneCycleWindow(double absoluteTimeSeconds) const {
    if (!m_document) return {0, 0};
    const auto& times = m_document->timeSeconds();
    if (times.size() < 2) return {0, times.size()};

    const double frequency = m_document->nominalFrequency() > 1.0
                                 ? m_document->nominalFrequency()
                                 : 50.0;
    const double period = 1.0 / frequency;
    const double endTime = std::clamp(absoluteTimeSeconds,
                                      m_document->dataStartSeconds(),
                                      m_document->dataEndSeconds());
    const double startTime = std::max(m_document->dataStartSeconds(), endTime - period);

    const auto firstIt = std::lower_bound(times.begin(), times.end(), startTime);
    const auto endIt = std::upper_bound(times.begin(), times.end(), endTime);
    std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), firstIt));
    std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    end = std::min(end, times.size());

    if (end > first + 2 && times[end - 1] - times[first] >= period * (1.0 - 1.0e-8)) ++first;
    if (end <= first) return {0, 0};
    return {first, end};
}

QString TableSnapshotController::cacheKey(int channelIndex, double absoluteTimeSeconds) const {
    return QStringLiteral("%1|%2|%3")
        .arg(channelIndex)
        .arg(QString::number(absoluteTimeSeconds, 'f', 12))
        .arg(m_document ? m_document->valueRepresentation() : QStringLiteral("secondary"));
}

QString TableSnapshotController::channelPhase(int channelIndex) const {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) {
        return QStringLiteral("Other");
    }
    const QString name = compact_name(m_document->channelName(channelIndex));
    if (name.contains(QStringLiteral("L1")) || name.endsWith(QStringLiteral("IA"))
        || name.endsWith(QStringLiteral("VA")) || name.endsWith(QStringLiteral("UA"))) {
        return QStringLiteral("L1");
    }
    if (name.contains(QStringLiteral("L2")) || name.endsWith(QStringLiteral("IB"))
        || name.endsWith(QStringLiteral("VB")) || name.endsWith(QStringLiteral("UB"))) {
        return QStringLiteral("L2");
    }
    if (name.contains(QStringLiteral("L3")) || name.endsWith(QStringLiteral("IC"))
        || name.endsWith(QStringLiteral("VC")) || name.endsWith(QStringLiteral("UC"))) {
        return QStringLiteral("L3");
    }
    if (name.contains(QStringLiteral("3I0")) || name.contains(QStringLiteral("3V0"))
        || name.contains(QStringLiteral("3U0")) || name.contains(QStringLiteral("RES"))
        || name.contains(QStringLiteral("NEUTRAL")) || name.contains(QStringLiteral("EARTH"))
        || name.contains(QStringLiteral("GROUND"))) {
        return QStringLiteral("E");
    }
    return QStringLiteral("Other");
}

QVariantMap TableSnapshotController::snapshotAt(int channelIndex, double absoluteTimeSeconds) {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()
        || !std::isfinite(absoluteTimeSeconds)) {
        return {{QStringLiteral("valid"), false}};
    }

    const QString key = cacheKey(channelIndex, absoluteTimeSeconds);
    if (const auto it = m_cache.constFind(key); it != m_cache.constEnd()) return it.value();

    const auto& samples = m_document->analogSamples(channelIndex);
    const auto& times = m_document->timeSeconds();
    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    const std::size_t cappedEnd = std::min({end, samples.size(), times.size()});
    if (first >= cappedEnd || cappedEnd - first < 4) return {{QStringLiteral("valid"), false}};

    const double scale = m_document->channelDisplayScale(channelIndex);
    long double sumSquares = 0.0L;
    double cyclePeakAbs = 0.0;
    std::size_t finiteCount = 0;
    for (std::size_t i = first; i < cappedEnd; ++i) {
        const double value = samples[i];
        if (!std::isfinite(value)) continue;
        sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
        cyclePeakAbs = std::max(cyclePeakAbs, std::abs(value));
        ++finiteCount;
    }
    if (finiteCount < 4) return {{QStringLiteral("valid"), false}};

    const double recordedRms = std::sqrt(static_cast<double>(sumSquares / static_cast<long double>(finiteCount)));
    const double frequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    const std::size_t count = cappedEnd - first;
    const auto spectrum = ardirec::power::harmonic_spectrum(
        std::span<const double>(samples.data() + first, count),
        std::span<const double>(times.data() + first, count),
        frequency,
        25,
        absoluteTimeSeconds);

    const auto nearestIt = std::lower_bound(times.begin(), times.end(), absoluteTimeSeconds);
    std::size_t sampleIndex = nearestIt == times.end()
                                  ? times.size() - 1
                                  : static_cast<std::size_t>(std::distance(times.begin(), nearestIt));
    if (sampleIndex > 0 && sampleIndex < times.size()
        && std::abs(times[sampleIndex - 1] - absoluteTimeSeconds)
               < std::abs(times[sampleIndex] - absoluteTimeSeconds)) {
        --sampleIndex;
    }
    sampleIndex = std::min(sampleIndex, samples.size() - 1);
    const double instantaneous = std::isfinite(samples[sampleIndex]) ? samples[sampleIndex] * scale : 0.0;

    const double absScale = std::abs(scale);
    const double h1Recorded = spectrum.valid ? spectrum.fundamental_rms : 0.0;
    const double h1 = h1Recorded * absScale;
    double angle = spectrum.valid && !spectrum.bins.empty() ? spectrum.bins.front().angle_degrees : 0.0;
    if (scale < 0.0) angle = wrap_degrees(angle + 180.0);
    const double dcRecorded = spectrum.valid ? spectrum.dc_component : 0.0;
    const double dcPercent = h1Recorded > kMinimumMagnitude ? std::abs(dcRecorded) / h1Recorded * 100.0 : 0.0;
    const double displayedRms = recordedRms * absScale;
    const auto lastExtremeRecorded = ardirec::power::last_extreme_value(samples, times, absoluteTimeSeconds);
    const double displayedExtremum = lastExtremeRecorded.value_or(0.0) * scale;
    const double displayedCyclePeak = cyclePeakAbs * absScale;
    const double crestFactor = displayedRms > kMinimumMagnitude ? displayedCyclePeak / displayedRms : 0.0;

    auto harmonicPercent = [&spectrum](int order) {
        if (!spectrum.valid || spectrum.fundamental_rms <= kMinimumMagnitude) return 0.0;
        const auto it = std::find_if(spectrum.bins.begin(), spectrum.bins.end(),
                                     [order](const auto& bin) { return bin.order == order; });
        return it == spectrum.bins.end() ? 0.0 : it->magnitude_rms / spectrum.fundamental_rms * 100.0;
    };

    const double thd = spectrum.valid ? spectrum.thd_percent : 0.0;
    const bool abnormal = thd >= kAbnormalThdPercent
                          || dcPercent >= kAbnormalDcPercent
                          || crestFactor >= kAbnormalCrestFactor;

    QVariantMap result{{QStringLiteral("valid"), true},
                       {QStringLiteral("channelIndex"), channelIndex},
                       {QStringLiteral("name"), m_document->channelName(channelIndex)},
                       {QStringLiteral("role"), m_document->analogRole(channelIndex)},
                       {QStringLiteral("phase"), channelPhase(channelIndex)},
                       {QStringLiteral("unit"), m_document->channelUnit(channelIndex)},
                       {QStringLiteral("instant"), instantaneous},
                       {QStringLiteral("rms"), displayedRms},
                       {QStringLiteral("fundamental"), h1},
                       {QStringLiteral("angle"), angle},
                       {QStringLiteral("extremum"), displayedExtremum},
                       {QStringLiteral("cyclePeak"), displayedCyclePeak},
                       {QStringLiteral("crestFactor"), crestFactor},
                       {QStringLiteral("dc"), dcRecorded * scale},
                       {QStringLiteral("dcPercent"), dcPercent},
                       {QStringLiteral("thd"), thd},
                       {QStringLiteral("maxHarmonicOrder"), spectrum.maximum_resolvable_order},
                       {QStringLiteral("sampleRate"), spectrum.estimated_sample_rate_hz},
                       {QStringLiteral("h2"), harmonicPercent(2)},
                       {QStringLiteral("h3"), harmonicPercent(3)},
                       {QStringLiteral("h5"), harmonicPercent(5)},
                       {QStringLiteral("abnormal"), abnormal}};

    if (m_cache.size() >= 512) m_cache.clear();
    m_cache.insert(key, result);
    return result;
}

QVariantList TableSnapshotController::sortedChannels(const QVariantList& channelIndexes,
                                                      double absoluteTimeSeconds,
                                                      const QString& sortMode,
                                                      bool abnormalOnly) {
    struct Entry { int channel{}; QVariantMap snapshot; };
    std::vector<Entry> entries;
    entries.reserve(static_cast<std::size_t>(channelIndexes.size()));
    for (const QVariant& value : channelIndexes) {
        const int channel = value.toInt();
        if (channel < 0 || !m_document || channel >= m_document->analogCount()) continue;
        QVariantMap snapshot = snapshotAt(channel, absoluteTimeSeconds);
        if (!snapshot.value(QStringLiteral("valid")).toBool()) continue;
        if (abnormalOnly && !snapshot.value(QStringLiteral("abnormal")).toBool()) continue;
        entries.push_back({channel, std::move(snapshot)});
    }

    const QString mode = sortMode.trimmed().toLower();
    if (mode == QStringLiteral("signal")) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.snapshot.value(QStringLiteral("name")).toString().localeAwareCompare(
                       b.snapshot.value(QStringLiteral("name")).toString()) < 0;
        });
    } else if (mode == QStringLiteral("rms")) {
        const auto roleRank = [](const Entry& entry) {
            const QString role = entry.snapshot.value(QStringLiteral("role")).toString();
            if (role == QStringLiteral("Voltage")) return 0;
            if (role == QStringLiteral("Current")) return 1;
            return 2;
        };
        std::stable_sort(entries.begin(), entries.end(), [roleRank](const Entry& a, const Entry& b) {
            const int rankA = roleRank(a);
            const int rankB = roleRank(b);
            if (rankA != rankB) return rankA < rankB;
            return a.snapshot.value(QStringLiteral("rms")).toDouble()
                   > b.snapshot.value(QStringLiteral("rms")).toDouble();
        });
    } else if (mode == QStringLiteral("thd")) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.snapshot.value(QStringLiteral("thd")).toDouble()
                   > b.snapshot.value(QStringLiteral("thd")).toDouble();
        });
    } else if (mode == QStringLiteral("dc")) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.snapshot.value(QStringLiteral("dcPercent")).toDouble()
                   > b.snapshot.value(QStringLiteral("dcPercent")).toDouble();
        });
    } else if (mode == QStringLiteral("crest")) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.snapshot.value(QStringLiteral("crestFactor")).toDouble()
                   > b.snapshot.value(QStringLiteral("crestFactor")).toDouble();
        });
    }

    QVariantList result;
    result.reserve(static_cast<qsizetype>(entries.size()));
    for (const Entry& entry : entries) result.push_back(entry.channel);
    return result;
}

QVariantMap TableSnapshotController::summaryAt(const QVariantList& channelIndexes,
                                                double absoluteTimeSeconds) {
    int maxThdChannel = -1;
    int maxDcChannel = -1;
    int maxCrestChannel = -1;
    int maxVoltageRmsChannel = -1;
    int maxCurrentRmsChannel = -1;
    double maxThd = -1.0;
    double maxDcPercent = -1.0;
    double maxCrestFactor = -1.0;
    double maxVoltageRms = -1.0;
    double maxCurrentRms = -1.0;
    int validCount = 0;
    int abnormalCount = 0;
    int maxHarmonicOrder = 0;
    double sampleRate = 0.0;

    for (const QVariant& value : channelIndexes) {
        const int channel = value.toInt();
        const QVariantMap snapshot = snapshotAt(channel, absoluteTimeSeconds);
        if (!snapshot.value(QStringLiteral("valid")).toBool()) continue;
        ++validCount;
        if (snapshot.value(QStringLiteral("abnormal")).toBool()) ++abnormalCount;
        const double thd = snapshot.value(QStringLiteral("thd")).toDouble();
        const double dcPercent = snapshot.value(QStringLiteral("dcPercent")).toDouble();
        const double crest = snapshot.value(QStringLiteral("crestFactor")).toDouble();
        const double rms = snapshot.value(QStringLiteral("rms")).toDouble();
        const QString role = snapshot.value(QStringLiteral("role")).toString();
        maxHarmonicOrder = std::max(maxHarmonicOrder,
                                    snapshot.value(QStringLiteral("maxHarmonicOrder")).toInt());
        sampleRate = std::max(sampleRate, snapshot.value(QStringLiteral("sampleRate")).toDouble());
        if (thd > maxThd) { maxThd = thd; maxThdChannel = channel; }
        if (dcPercent > maxDcPercent) { maxDcPercent = dcPercent; maxDcChannel = channel; }
        if (crest > maxCrestFactor) { maxCrestFactor = crest; maxCrestChannel = channel; }
        if (role == QStringLiteral("Voltage") && rms > maxVoltageRms) {
            maxVoltageRms = rms;
            maxVoltageRmsChannel = channel;
        }
        if (role == QStringLiteral("Current") && rms > maxCurrentRms) {
            maxCurrentRms = rms;
            maxCurrentRmsChannel = channel;
        }
    }

    return {{QStringLiteral("count"), validCount},
            {QStringLiteral("abnormalCount"), abnormalCount},
            {QStringLiteral("maxThdChannel"), maxThdChannel},
            {QStringLiteral("maxThd"), std::max(0.0, maxThd)},
            {QStringLiteral("maxDcChannel"), maxDcChannel},
            {QStringLiteral("maxDcPercent"), std::max(0.0, maxDcPercent)},
            {QStringLiteral("maxCrestChannel"), maxCrestChannel},
            {QStringLiteral("maxCrestFactor"), std::max(0.0, maxCrestFactor)},
            {QStringLiteral("maxVoltageRmsChannel"), maxVoltageRmsChannel},
            {QStringLiteral("maxVoltageRms"), std::max(0.0, maxVoltageRms)},
            {QStringLiteral("maxCurrentRmsChannel"), maxCurrentRmsChannel},
            {QStringLiteral("maxCurrentRms"), std::max(0.0, maxCurrentRms)},
            {QStringLiteral("maxHarmonicOrder"), maxHarmonicOrder},
            {QStringLiteral("sampleRate"), sampleRate}};
}
