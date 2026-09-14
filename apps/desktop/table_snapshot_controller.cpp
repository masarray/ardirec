// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_snapshot_controller.hpp"

#include "ardirec/power/harmonics.hpp"
#include "ardirec/power/waveform_metrics.hpp"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

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

struct TableFrameChannelInfo final {
    QString name;
    QString unit;
    QString role;
    QString phase;
    double displayScale{1.0};
};

double wrap_degrees(double angle) {
    while (angle <= -180.0) angle += 360.0;
    while (angle > 180.0) angle -= 360.0;
    return angle;
}

std::pair<std::size_t, std::size_t> frame_cycle_window(const TableFrameSource& source,
                                                        double absoluteTimeSeconds);
std::size_t frame_nearest_sample(const TableFrameSource& source,
                                 double absoluteTimeSeconds);
QVariantMap frame_summary(const std::vector<QVariantMap>& rows);
QVariantMap build_table_frame(const std::shared_ptr<const TableFrameSource>& source,
                              const std::vector<int>& channelIndexes,
                              double absoluteTimeSeconds,
                              const QString& sortMode,
                              bool abnormalOnly,
                              const std::shared_ptr<std::atomic_bool>& cancel);
} // namespace

struct TableFrameSource final {
    std::shared_ptr<const ardirec::comtrade::IndexedDatFile> data;
    std::shared_ptr<const std::vector<double>> times;
    std::vector<TableFrameChannelInfo> channels;
    double frequency{50.0};
};

namespace {
std::pair<std::size_t, std::size_t> frame_cycle_window(const TableFrameSource& source,
                                                        double absoluteTimeSeconds) {
    if (!source.times || source.times->empty()) return {0, 0};
    const auto& times = *source.times;
    if (times.size() < 2) return {0, times.size()};

    const double frequency = source.frequency > 1.0 ? source.frequency : 50.0;
    const double period = 1.0 / frequency;
    const double endTime = std::clamp(absoluteTimeSeconds, times.front(), times.back());
    // R5.4 owns the complete-backward-cycle qualification. R5.3 deliberately
    // preserves the existing Table measurement-window semantics while moving
    // the expensive work off the GUI thread.
    const double startTime = std::max(times.front(), endTime - period);

    const auto firstIt = std::lower_bound(times.begin(), times.end(), startTime);
    const auto endIt = std::upper_bound(times.begin(), times.end(), endTime);
    std::size_t first = static_cast<std::size_t>(std::distance(times.begin(), firstIt));
    std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), endIt));
    end = std::min(end, times.size());
    if (end > first + 2 && times[end - 1] - times[first] >= period * (1.0 - 1.0e-8)) ++first;
    if (end <= first) return {0, 0};
    return {first, end};
}

std::size_t frame_nearest_sample(const TableFrameSource& source,
                                 double absoluteTimeSeconds) {
    if (!source.times || source.times->empty()) return 0;
    const auto& times = *source.times;
    absoluteTimeSeconds = std::clamp(absoluteTimeSeconds, times.front(), times.back());
    auto it = std::lower_bound(times.begin(), times.end(), absoluteTimeSeconds);
    std::size_t index = static_cast<std::size_t>(std::distance(times.begin(), it));
    if (index >= times.size()) index = times.size() - 1;
    if (index > 0 && index < times.size()) {
        const double before = std::abs(absoluteTimeSeconds - times[index - 1]);
        const double after = std::abs(times[index] - absoluteTimeSeconds);
        if (before <= after) --index;
    }
    return index;
}

QVariantMap build_table_row(const TableFrameSource& source,
                            int channelIndex,
                            double absoluteTimeSeconds,
                            const std::shared_ptr<std::atomic_bool>& cancel) {
    if (!source.data || !source.times || channelIndex < 0
        || static_cast<std::size_t>(channelIndex) >= source.channels.size()) {
        return {{QStringLiteral("valid"), false}};
    }

    const auto& times = *source.times;
    const auto [first, end] = frame_cycle_window(source, absoluteTimeSeconds);
    const std::size_t cappedEnd = std::min(end, times.size());
    if (first >= cappedEnd || cappedEnd - first < 4) return {{QStringLiteral("valid"), false}};

    const std::size_t count = cappedEnd - first;
    std::vector<double> samples;
    samples.reserve(count);
    long double sumSquares = 0.0L;
    double cyclePeakAbs = 0.0;
    std::size_t finiteCount = 0;
    const std::size_t channel = static_cast<std::size_t>(channelIndex);
    for (std::size_t sample = first; sample < cappedEnd; ++sample) {
        if (cancel && ((sample - first) & 63u) == 0u
            && cancel->load(std::memory_order_relaxed)) {
            return {{QStringLiteral("cancelled"), true}};
        }
        const double value = source.data->analogValue(sample, channel);
        samples.push_back(value);
        if (!std::isfinite(value)) continue;
        sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
        cyclePeakAbs = std::max(cyclePeakAbs, std::abs(value));
        ++finiteCount;
    }
    if (finiteCount < 4) return {{QStringLiteral("valid"), false}};

    const auto& info = source.channels[channel];
    const double scale = info.displayScale;
    const double absScale = std::abs(scale);
    const double recordedRms = std::sqrt(static_cast<double>(sumSquares / static_cast<long double>(finiteCount)));
    const double frequency = source.frequency > 1.0 ? source.frequency : 50.0;
    const auto timeWindow = std::span<const double>(times.data() + first, count);
    const double referenceTime = times[cappedEnd - 1u];
    const auto spectrum = ardirec::power::harmonic_spectrum(
        std::span<const double>(samples.data(), samples.size()),
        timeWindow,
        frequency,
        25,
        referenceTime);

    if (cancel && cancel->load(std::memory_order_relaxed)) {
        return {{QStringLiteral("cancelled"), true}};
    }

    const std::size_t instantIndex = frame_nearest_sample(source, absoluteTimeSeconds);
    const double instantRecorded = source.data->analogValue(instantIndex, channel);
    const double instantaneous = std::isfinite(instantRecorded) ? instantRecorded * scale : 0.0;

    const double h1Recorded = spectrum.valid ? spectrum.fundamental_rms : 0.0;
    const double h1 = h1Recorded * absScale;
    double angle = spectrum.valid && !spectrum.bins.empty() ? spectrum.bins.front().angle_degrees : 0.0;
    if (scale < 0.0) angle = wrap_degrees(angle + 180.0);
    angle = wrap_degrees(angle + 360.0 * frequency * (absoluteTimeSeconds - referenceTime));

    const double dcRecorded = spectrum.valid ? spectrum.dc_component : 0.0;
    const double dcPercent = h1Recorded > kMinimumMagnitude
                                 ? std::abs(dcRecorded) / h1Recorded * 100.0
                                 : 0.0;
    const double displayedRms = recordedRms * absScale;
    const auto lastExtremeRecorded = ardirec::power::last_extreme_value(
        std::span<const double>(samples.data(), samples.size()), timeWindow, referenceTime);
    const double displayedExtremum = lastExtremeRecorded.value_or(0.0) * scale;
    const double displayedCyclePeak = cyclePeakAbs * absScale;
    const double crestFactor = displayedRms > kMinimumMagnitude
                                   ? displayedCyclePeak / displayedRms
                                   : 0.0;

    auto harmonicPercent = [&spectrum](int order) {
        if (!spectrum.valid || spectrum.fundamental_rms <= kMinimumMagnitude) return 0.0;
        const auto it = std::find_if(spectrum.bins.begin(), spectrum.bins.end(),
                                     [order](const auto& bin) { return bin.order == order; });
        return it == spectrum.bins.end()
                   ? 0.0
                   : it->magnitude_rms / spectrum.fundamental_rms * 100.0;
    };

    const double thd = spectrum.valid ? spectrum.thd_percent : 0.0;
    const bool abnormal = thd >= kAbnormalThdPercent
                          || dcPercent >= kAbnormalDcPercent
                          || crestFactor >= kAbnormalCrestFactor;

    return {{QStringLiteral("valid"), true},
            {QStringLiteral("channelIndex"), channelIndex},
            {QStringLiteral("name"), info.name},
            {QStringLiteral("role"), info.role},
            {QStringLiteral("phase"), info.phase},
            {QStringLiteral("unit"), info.unit},
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
}

QVariantMap frame_summary(const std::vector<QVariantMap>& rows) {
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

    for (const QVariantMap& row : rows) {
        if (!row.value(QStringLiteral("valid")).toBool()) continue;
        ++validCount;
        if (row.value(QStringLiteral("abnormal")).toBool()) ++abnormalCount;
        const int channel = row.value(QStringLiteral("channelIndex"), -1).toInt();
        const double thd = row.value(QStringLiteral("thd")).toDouble();
        const double dcPercent = row.value(QStringLiteral("dcPercent")).toDouble();
        const double crest = row.value(QStringLiteral("crestFactor")).toDouble();
        const double rms = row.value(QStringLiteral("rms")).toDouble();
        const QString role = row.value(QStringLiteral("role")).toString();
        maxHarmonicOrder = std::max(maxHarmonicOrder,
                                    row.value(QStringLiteral("maxHarmonicOrder")).toInt());
        sampleRate = std::max(sampleRate, row.value(QStringLiteral("sampleRate")).toDouble());
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

void sort_table_rows(std::vector<QVariantMap>& rows, const QString& sortMode) {
    const QString mode = sortMode.trimmed().toLower();
    if (mode == QStringLiteral("signal")) {
        std::stable_sort(rows.begin(), rows.end(), [](const QVariantMap& a, const QVariantMap& b) {
            return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                       b.value(QStringLiteral("name")).toString()) < 0;
        });
    } else if (mode == QStringLiteral("rms")) {
        const auto roleRank = [](const QVariantMap& row) {
            const QString role = row.value(QStringLiteral("role")).toString();
            if (role == QStringLiteral("Voltage")) return 0;
            if (role == QStringLiteral("Current")) return 1;
            return 2;
        };
        std::stable_sort(rows.begin(), rows.end(), [roleRank](const QVariantMap& a, const QVariantMap& b) {
            const int rankA = roleRank(a);
            const int rankB = roleRank(b);
            if (rankA != rankB) return rankA < rankB;
            return a.value(QStringLiteral("rms")).toDouble()
                   > b.value(QStringLiteral("rms")).toDouble();
        });
    } else if (mode == QStringLiteral("thd")) {
        std::stable_sort(rows.begin(), rows.end(), [](const QVariantMap& a, const QVariantMap& b) {
            return a.value(QStringLiteral("thd")).toDouble()
                   > b.value(QStringLiteral("thd")).toDouble();
        });
    } else if (mode == QStringLiteral("dc")) {
        std::stable_sort(rows.begin(), rows.end(), [](const QVariantMap& a, const QVariantMap& b) {
            return a.value(QStringLiteral("dcPercent")).toDouble()
                   > b.value(QStringLiteral("dcPercent")).toDouble();
        });
    } else if (mode == QStringLiteral("crest")) {
        std::stable_sort(rows.begin(), rows.end(), [](const QVariantMap& a, const QVariantMap& b) {
            return a.value(QStringLiteral("crestFactor")).toDouble()
                   > b.value(QStringLiteral("crestFactor")).toDouble();
        });
    }
}

QVariantMap build_table_frame(const std::shared_ptr<const TableFrameSource>& source,
                              const std::vector<int>& channelIndexes,
                              double absoluteTimeSeconds,
                              const QString& sortMode,
                              bool abnormalOnly,
                              const std::shared_ptr<std::atomic_bool>& cancel) {
    if (!source || !source->data || !source->times || source->times->empty()
        || !std::isfinite(absoluteTimeSeconds)) {
        return {{QStringLiteral("valid"), false}};
    }

    std::vector<QVariantMap> allRows;
    allRows.reserve(channelIndexes.size());
    for (const int channel : channelIndexes) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            return {{QStringLiteral("cancelled"), true}};
        }
        QVariantMap row = build_table_row(*source, channel, absoluteTimeSeconds, cancel);
        if (row.value(QStringLiteral("cancelled")).toBool()) return row;
        if (row.value(QStringLiteral("valid")).toBool()) allRows.push_back(std::move(row));
    }

    const QVariantMap summary = frame_summary(allRows);
    std::vector<QVariantMap> displayRows;
    displayRows.reserve(allRows.size());
    for (const QVariantMap& row : allRows) {
        if (abnormalOnly && !row.value(QStringLiteral("abnormal")).toBool()) continue;
        displayRows.push_back(row);
    }
    sort_table_rows(displayRows, sortMode);

    QVariantList rows;
    QVariantList displayedChannels;
    rows.reserve(static_cast<qsizetype>(displayRows.size()));
    displayedChannels.reserve(static_cast<qsizetype>(displayRows.size()));
    for (const QVariantMap& row : displayRows) {
        rows.push_back(row);
        displayedChannels.push_back(row.value(QStringLiteral("channelIndex")));
    }

    const double committedTime = std::clamp(absoluteTimeSeconds,
                                            source->times->front(),
                                            source->times->back());
    return {{QStringLiteral("valid"), true},
            {QStringLiteral("time"), committedTime},
            {QStringLiteral("rows"), rows},
            {QStringLiteral("displayedChannels"), displayedChannels},
            {QStringLiteral("summary"), summary},
            {QStringLiteral("sortMode"), sortMode.trimmed().toLower()},
            {QStringLiteral("abnormalOnly"), abnormalOnly},
            {QStringLiteral("requestedCount"), static_cast<int>(channelIndexes.size())}};
}
} // namespace

TableSnapshotController::TableSnapshotController(DocumentController* document, QObject* parent)
    : QObject(parent), m_document(document) {
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, [this]() {
            m_lastFrameRequest.reset();
            rebuildFrameSource(false);
        });
        connect(m_document, &DocumentController::representationChanged, this, [this]() {
            rebuildFrameSource(true);
        });
    }
    rebuildFrameSource(false);
}

TableSnapshotController::~TableSnapshotController() {
    cancelFrameWork(true);
}

void TableSnapshotController::clearCache() {
    m_cache.clear();
    m_touchCounter = 0;
}

void TableSnapshotController::cancelFrameWork(bool clearBusy) noexcept {
    if (m_frameCancel) m_frameCancel->store(true, std::memory_order_relaxed);
    m_frameCancel.reset();
    m_activeFrameRequest.reset();
    m_pendingFrameRequest.reset();
    ++m_frameGeneration;
    if (clearBusy && m_busy) {
        m_busy = false;
        emit busyChanged();
    }
}

void TableSnapshotController::rebuildFrameSource(bool preserveLastRequest) {
    const std::optional<FrameRequest> retry = preserveLastRequest ? m_lastFrameRequest : std::nullopt;
    cancelFrameWork(true);
    clearCache();
    m_frameSource.reset();
    m_committedFrameRequest.reset();
    if (!preserveLastRequest) m_lastFrameRequest.reset();

    if (!m_frame.isEmpty()) {
        m_frame.clear();
        emit frameChanged();
    }

    if (!m_document || !m_document->dataStoreSnapshot() || !m_document->timeIndexSnapshot()
        || m_document->analogCount() <= 0) {
        return;
    }

    auto source = std::make_shared<TableFrameSource>();
    source->data = m_document->dataStoreSnapshot();
    source->times = m_document->timeIndexSnapshot();
    source->frequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    source->channels.reserve(static_cast<std::size_t>(m_document->analogCount()));
    for (int index = 0; index < m_document->analogCount(); ++index) {
        source->channels.push_back(TableFrameChannelInfo{
            m_document->channelName(index),
            m_document->channelUnit(index),
            m_document->analogRole(index),
            m_document->channelPhase(index),
            m_document->channelDisplayScale(index),
        });
    }
    m_frameSource = std::move(source);

    if (retry) {
        QVariantList channels;
        channels.reserve(static_cast<qsizetype>(retry->channels.size()));
        for (const int channel : retry->channels) channels.push_back(channel);
        requestFrame(channels, retry->absoluteTimeSeconds, retry->sortMode, retry->abnormalOnly);
    }
}

TableSnapshotController::FrameRequest
TableSnapshotController::makeFrameRequest(const QVariantList& channelIndexes,
                                          double absoluteTimeSeconds,
                                          const QString& sortMode,
                                          bool abnormalOnly) const {
    FrameRequest request;
    request.absoluteTimeSeconds = absoluteTimeSeconds;
    request.sortMode = sortMode.trimmed().toLower();
    if (request.sortMode != QStringLiteral("signal")
        && request.sortMode != QStringLiteral("rms")
        && request.sortMode != QStringLiteral("thd")
        && request.sortMode != QStringLiteral("dc")
        && request.sortMode != QStringLiteral("crest")) {
        request.sortMode = QStringLiteral("record");
    }
    request.abnormalOnly = abnormalOnly;
    const int channelCount = m_frameSource
                                 ? static_cast<int>(m_frameSource->channels.size())
                                 : (m_document ? m_document->analogCount() : 0);
    request.channels.reserve(static_cast<std::size_t>(channelIndexes.size()));
    for (const QVariant& value : channelIndexes) {
        const int channel = value.toInt();
        if (channel < 0 || channel >= channelCount) continue;
        if (std::find(request.channels.begin(), request.channels.end(), channel) == request.channels.end())
            request.channels.push_back(channel);
    }
    return request;
}

bool TableSnapshotController::sameFrameRequest(const FrameRequest& lhs,
                                               const FrameRequest& rhs) noexcept {
    return lhs.channels == rhs.channels
           && std::abs(lhs.absoluteTimeSeconds - rhs.absoluteTimeSeconds) <= 1.0e-12
           && lhs.sortMode == rhs.sortMode
           && lhs.abnormalOnly == rhs.abnormalOnly;
}

void TableSnapshotController::requestFrame(const QVariantList& channelIndexes,
                                           double absoluteTimeSeconds,
                                           const QString& sortMode,
                                           bool abnormalOnly) {
    if (!m_frameSource || !std::isfinite(absoluteTimeSeconds)) return;
    const FrameRequest request = makeFrameRequest(channelIndexes,
                                                  absoluteTimeSeconds,
                                                  sortMode,
                                                  abnormalOnly);
    m_lastFrameRequest = request;

    if (m_busy) {
        if (m_activeFrameRequest && sameFrameRequest(request, *m_activeFrameRequest)) {
            m_pendingFrameRequest.reset();
        } else {
            m_pendingFrameRequest = request;
        }
        return;
    }

    if (m_committedFrameRequest && sameFrameRequest(request, *m_committedFrameRequest)
        && m_frame.value(QStringLiteral("valid")).toBool()) {
        return;
    }
    launchFrameRequest(request);
}

void TableSnapshotController::launchFrameRequest(const FrameRequest& request) {
    if (!m_frameSource) return;
    m_activeFrameRequest = request;
    auto cancelToken = std::make_shared<std::atomic_bool>(false);
    m_frameCancel = cancelToken;
    const quint64 generation = ++m_frameGeneration;
    if (!m_busy) {
        m_busy = true;
        emit busyChanged();
    }

    const auto source = m_frameSource;
    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, generation, request, cancelToken]() {
                const QVariantMap result = watcher->result();
                watcher->deleteLater();
                completeFrameRequest(generation, request, cancelToken, result);
            });
    watcher->setFuture(QtConcurrent::run(
        [source, request, cancelToken]() {
            return build_table_frame(source,
                                     request.channels,
                                     request.absoluteTimeSeconds,
                                     request.sortMode,
                                     request.abnormalOnly,
                                     cancelToken);
        }));
}

void TableSnapshotController::completeFrameRequest(
    quint64 generation,
    const FrameRequest& request,
    const std::shared_ptr<std::atomic_bool>& cancelToken,
    const QVariantMap& result) {
    if (cancelToken->load(std::memory_order_relaxed) || generation != m_frameGeneration) return;

    if (m_pendingFrameRequest) {
        const FrameRequest pending = *m_pendingFrameRequest;
        m_pendingFrameRequest.reset();

        if (m_committedFrameRequest && sameFrameRequest(pending, *m_committedFrameRequest)) {
            m_activeFrameRequest.reset();
            m_frameCancel.reset();
            if (m_busy) {
                m_busy = false;
                emit busyChanged();
            }
            return;
        }

        if (!sameFrameRequest(pending, request)) {
            m_activeFrameRequest.reset();
            m_frameCancel.reset();
            launchFrameRequest(pending);
            return;
        }
    }

    m_activeFrameRequest.reset();
    m_frameCancel.reset();
    if (!result.value(QStringLiteral("cancelled")).toBool()) {
        m_frame = result;
        m_committedFrameRequest = request;
        emit frameChanged();
    }
    if (m_busy) {
        m_busy = false;
        emit busyChanged();
    }
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

SampleSnapshotKey TableSnapshotController::cacheKey(int channelIndex,
                                                     double absoluteTimeSeconds) const {
    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    const quint64 lastSample = end > 0u ? static_cast<quint64>(end - 1u) : 0u;
    return SampleSnapshotKey{
        lastSample,
        static_cast<quint64>(first),
        channelIndex,
        0,
        m_document && m_document->valueRepresentation() == QStringLiteral("primary")};
}

QString TableSnapshotController::channelPhase(int channelIndex) const {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) {
        return QStringLiteral("Other");
    }
    return m_document->channelPhase(channelIndex);
}

QVariantMap TableSnapshotController::adjustedForReference(const CacheEntry& entry,
                                                           int channelIndex,
                                                           double absoluteTimeSeconds) const {
    QVariantMap result = entry.value;
    if (!m_document || !std::isfinite(absoluteTimeSeconds)) return result;

    const double instantaneous = m_document->sampleValue(channelIndex, absoluteTimeSeconds);
    result.insert(QStringLiteral("instant"), std::isfinite(instantaneous) ? instantaneous : 0.0);

    if (std::isfinite(entry.referenceTime)) {
        const double frequency = m_document->nominalFrequency() > 1.0
                                     ? m_document->nominalFrequency()
                                     : 50.0;
        const double delta = absoluteTimeSeconds - entry.referenceTime;
        const double baseAngle = result.value(QStringLiteral("angle")).toDouble();
        result.insert(QStringLiteral("angle"), wrap_degrees(baseAngle + 360.0 * frequency * delta));
    }
    return result;
}

void TableSnapshotController::trimCache() {
    while (m_cache.size() > m_maxCacheEntries) {
        auto victim = m_cache.end();
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (victim == m_cache.end() || it.value().touch < victim.value().touch) victim = it;
        }
        if (victim == m_cache.end()) break;
        m_cache.erase(victim);
    }
}

QVariantMap TableSnapshotController::snapshotAt(int channelIndex, double absoluteTimeSeconds) {
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()
        || !std::isfinite(absoluteTimeSeconds)) {
        return {{QStringLiteral("valid"), false}};
    }

    const SampleSnapshotKey key = cacheKey(channelIndex, absoluteTimeSeconds);
    if (auto it = m_cache.find(key); it != m_cache.end()) {
        it.value().touch = ++m_touchCounter;
        return adjustedForReference(it.value(), channelIndex, absoluteTimeSeconds);
    }

    const auto& times = m_document->timeSeconds();
    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    const std::size_t cappedEnd = std::min(end, times.size());
    if (first >= cappedEnd || cappedEnd - first < 4) return {{QStringLiteral("valid"), false}};

    std::vector<double> samples;
    m_document->copyRecordedAnalogRange(channelIndex, first, cappedEnd, samples);
    const std::size_t count = std::min(samples.size(), cappedEnd - first);
    if (count < 4) return {{QStringLiteral("valid"), false}};

    const double scale = m_document->channelDisplayScale(channelIndex);
    long double sumSquares = 0.0L;
    double cyclePeakAbs = 0.0;
    std::size_t finiteCount = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const double value = samples[i];
        if (!std::isfinite(value)) continue;
        sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
        cyclePeakAbs = std::max(cyclePeakAbs, std::abs(value));
        ++finiteCount;
    }
    if (finiteCount < 4) return {{QStringLiteral("valid"), false}};

    const double recordedRms = std::sqrt(static_cast<double>(sumSquares / static_cast<long double>(finiteCount)));
    const double frequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    const auto timeWindow = std::span<const double>(times.data() + first, count);
    const double referenceTime = times[std::min(cappedEnd - 1u, times.size() - 1u)];
    const auto spectrum = ardirec::power::harmonic_spectrum(
        std::span<const double>(samples.data(), count),
        timeWindow,
        frequency,
        25,
        referenceTime);

    const double instantaneous = m_document->sampleValue(channelIndex, referenceTime);
    const double safeInstantaneous = std::isfinite(instantaneous) ? instantaneous : 0.0;

    const double absScale = std::abs(scale);
    const double h1Recorded = spectrum.valid ? spectrum.fundamental_rms : 0.0;
    const double h1 = h1Recorded * absScale;
    double angle = spectrum.valid && !spectrum.bins.empty() ? spectrum.bins.front().angle_degrees : 0.0;
    if (scale < 0.0) angle = wrap_degrees(angle + 180.0);
    const double dcRecorded = spectrum.valid ? spectrum.dc_component : 0.0;
    const double dcPercent = h1Recorded > kMinimumMagnitude ? std::abs(dcRecorded) / h1Recorded * 100.0 : 0.0;
    const double displayedRms = recordedRms * absScale;
    const auto lastExtremeRecorded = ardirec::power::last_extreme_value(
        std::span<const double>(samples.data(), count), timeWindow, referenceTime);
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
                       {QStringLiteral("instant"), safeInstantaneous},
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

    CacheEntry entry{result, referenceTime, ++m_touchCounter};
    m_cache.insert(key, entry);
    trimCache();
    return adjustedForReference(entry, channelIndex, absoluteTimeSeconds);
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
