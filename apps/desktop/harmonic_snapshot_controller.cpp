// SPDX-License-Identifier: GPL-3.0-or-later
#include "harmonic_snapshot_controller.hpp"

#include "ardirec/power/harmonics.hpp"

#include <algorithm>
#include <cmath>
#include <span>

namespace {
double wrap_degrees(double angle) {
    while (angle <= -180.0) angle += 360.0;
    while (angle > 180.0) angle -= 360.0;
    return angle;
}
} // namespace

HarmonicSnapshotController::HarmonicSnapshotController(DocumentController* document, QObject* parent)
    : QObject(parent), m_document(document) {
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged,
                this, &HarmonicSnapshotController::clearCache);
        connect(m_document, &DocumentController::representationChanged,
                this, &HarmonicSnapshotController::clearCache);
    }
}

void HarmonicSnapshotController::clearCache() {
    m_cache.clear();
}

std::pair<std::size_t, std::size_t>
HarmonicSnapshotController::oneCycleWindow(double absoluteTimeSeconds) const {
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

    if (end > first + 2 && times[end - 1] - times[first] >= period * (1.0 - 1.0e-8)) {
        ++first;
    }
    if (end <= first) return {0, 0};
    return {first, end};
}

QString HarmonicSnapshotController::cacheKey(int channelIndex,
                                             double absoluteTimeSeconds,
                                             int maximumOrder) const {
    return QStringLiteral("%1|%2|%3|%4")
        .arg(channelIndex)
        .arg(QString::number(absoluteTimeSeconds, 'f', 12))
        .arg(maximumOrder)
        .arg(m_document ? m_document->valueRepresentation() : QStringLiteral("secondary"));
}

QVariantMap HarmonicSnapshotController::spectrumAt(int channelIndex,
                                                   double absoluteTimeSeconds,
                                                   int maximumOrder) {
    QVariantList bins;
    if (!m_document || channelIndex < 0 || channelIndex >= m_document->analogCount()) {
        return {{QStringLiteral("valid"), false}, {QStringLiteral("bins"), bins}};
    }

    maximumOrder = std::clamp(maximumOrder, 1, 50);
    const QString key = cacheKey(channelIndex, absoluteTimeSeconds, maximumOrder);
    if (const auto it = m_cache.constFind(key); it != m_cache.constEnd()) return it.value();

    const auto& samples = m_document->analogSamples(channelIndex);
    const auto& times = m_document->timeSeconds();
    const auto [first, end] = oneCycleWindow(absoluteTimeSeconds);
    const std::size_t cappedEnd = std::min({end, samples.size(), times.size()});
    if (first >= cappedEnd || cappedEnd - first < 4) {
        return {{QStringLiteral("valid"), false}, {QStringLiteral("bins"), bins}};
    }

    const double frequency = m_document->nominalFrequency() > 1.0
                                 ? m_document->nominalFrequency()
                                 : 50.0;
    const std::size_t count = cappedEnd - first;
    const auto spectrum = ardirec::power::harmonic_spectrum(
        std::span<const double>(samples.data() + first, count),
        std::span<const double>(times.data() + first, count),
        frequency,
        maximumOrder,
        absoluteTimeSeconds);
    if (!spectrum.valid) {
        return {{QStringLiteral("valid"), false}, {QStringLiteral("bins"), bins}};
    }

    const double signedScale = m_document->channelDisplayScale(channelIndex);
    const double representationScale = std::abs(signedScale);
    const double phaseOffset = signedScale < 0.0 ? 180.0 : 0.0;
    const double recordedFundamental = spectrum.fundamental_rms;
    const double dcMagnitude = std::abs(spectrum.dc_component) * representationScale;
    const double dcPercent = recordedFundamental > 1.0e-12
                                 ? std::abs(spectrum.dc_component) / recordedFundamental * 100.0
                                 : 0.0;

    bins.push_back(QVariantMap{{QStringLiteral("order"), 0},
                               {QStringLiteral("magnitude"), dcMagnitude},
                               {QStringLiteral("signedMagnitude"), spectrum.dc_component * signedScale},
                               {QStringLiteral("percent"), dcPercent},
                               {QStringLiteral("angle"), 0.0}});

    for (const auto& bin : spectrum.bins) {
        const double percent = recordedFundamental > 1.0e-12
                                   ? bin.magnitude_rms / recordedFundamental * 100.0
                                   : 0.0;
        bins.push_back(QVariantMap{{QStringLiteral("order"), bin.order},
                                   {QStringLiteral("magnitude"), bin.magnitude_rms * representationScale},
                                   {QStringLiteral("signedMagnitude"), bin.magnitude_rms * representationScale},
                                   {QStringLiteral("percent"), percent},
                                   {QStringLiteral("angle"), wrap_degrees(bin.angle_degrees + phaseOffset)}});
    }

    QVariantMap result{{QStringLiteral("valid"), true},
                       {QStringLiteral("dc"), spectrum.dc_component * signedScale},
                       {QStringLiteral("dcPercent"), dcPercent},
                       {QStringLiteral("fundamental"), spectrum.fundamental_rms * representationScale},
                       {QStringLiteral("thdPercent"), spectrum.thd_percent},
                       {QStringLiteral("dominantOrder"), spectrum.dominant_order},
                       {QStringLiteral("dominantMagnitude"), spectrum.dominant_rms * representationScale},
                       {QStringLiteral("dominantPercent"), spectrum.dominant_percent},
                       {QStringLiteral("maximumResolvableOrder"), spectrum.maximum_resolvable_order},
                       {QStringLiteral("sampleRate"), spectrum.estimated_sample_rate_hz},
                       {QStringLiteral("requestedOrder"), maximumOrder},
                       {QStringLiteral("bins"), bins},
                       {QStringLiteral("unit"), m_document->channelUnit(channelIndex)},
                       {QStringLiteral("name"), m_document->channelName(channelIndex)},
                       {QStringLiteral("windowEnd"), absoluteTimeSeconds},
                       {QStringLiteral("windowDuration"), 1.0 / frequency}};

    if (m_cache.size() >= 256) m_cache.clear();
    m_cache.insert(key, result);
    return result;
}
