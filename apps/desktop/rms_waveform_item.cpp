// SPDX-License-Identifier: GPL-3.0-or-later
#include "rms_waveform_item.hpp"
#include "rms_cycle_window.hpp"

#include <QFutureWatcher>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

struct RmsGeometrySnapshot {
    struct Point {
        float xFraction{0.0F};
        float normalizedRms{0.0F};
    };
    std::vector<Point> points;
};

namespace {
using SnapshotPtr = std::shared_ptr<const RmsGeometrySnapshot>;

[[nodiscard]] bool cancelled(const std::shared_ptr<std::atomic_bool>& flag) noexcept {
    return flag && flag->load(std::memory_order_relaxed);
}

[[nodiscard]] SnapshotPtr build_rms_geometry(
    const std::shared_ptr<const ardirec::comtrade::IndexedDatFile>& data,
    const std::shared_ptr<const std::vector<double>>& times,
    int channelIndex,
    double displayScale,
    double scalePeak,
    double nominalFrequency,
    double zoomFactor,
    double panFraction,
    int requestedPixelWidth,
    const std::shared_ptr<std::atomic_bool>& cancel) {
    if (!data || !times || channelIndex < 0 || requestedPixelWidth <= 1 || cancelled(cancel)) return {};

    const std::size_t count = std::min(data->frameCount(), times->size());
    const std::size_t channel = static_cast<std::size_t>(channelIndex);
    if (count < 2 || channel >= data->analogCount()) return {};

    const double dataStart = times->front();
    const double dataEnd = (*times)[count - 1];
    const double fullDuration = std::max(0.0, dataEnd - dataStart);
    if (!(fullDuration > 0.0) || !std::isfinite(fullDuration)) return {};

    zoomFactor = std::clamp(zoomFactor, 1.0, 500.0);
    panFraction = std::clamp(panFraction, 0.0, 1.0);
    const double visibleDuration = fullDuration / zoomFactor;
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double startTime = dataStart + panFraction * movable;
    const double endTime = startTime + visibleDuration;

    const auto logicalEnd = times->begin() + static_cast<std::ptrdiff_t>(count);
    auto first = std::lower_bound(times->begin(), logicalEnd, startTime);
    auto last = std::upper_bound(times->begin(), logicalEnd, endTime);
    std::size_t start = static_cast<std::size_t>(std::distance(times->begin(), first));
    std::size_t end = static_cast<std::size_t>(std::distance(times->begin(), last));
    start = std::min(start, count - 1u);
    end = std::min(end, count);
    if (start > 0) --start;
    if (end < count) ++end;
    if (end <= start + 1u) end = std::min(count, start + 2u);
    if (end <= start + 1u) return {};

    const std::size_t visibleCount = end - start;
    const std::size_t pixelWidth = std::clamp<std::size_t>(
        static_cast<std::size_t>(requestedPixelWidth), 64u, 4096u);
    const std::size_t targetPoints = std::min(
        visibleCount, std::max<std::size_t>(64u, pixelWidth * 2u));
    const std::size_t stride = std::max<std::size_t>(
        1u, (visibleCount + targetPoints - 1u) / targetPoints);

    if (!std::isfinite(scalePeak) || scalePeak < 1.0e-12) scalePeak = 1.0;
    if (!std::isfinite(displayScale)) displayScale = 1.0;
    if (!std::isfinite(nominalFrequency) || nominalFrequency <= 1.0) nominalFrequency = 50.0;
    const double magnitudeScale = std::abs(displayScale);

    auto snapshot = std::make_shared<RmsGeometrySnapshot>();
    snapshot->points.reserve(targetPoints + 1u);

    const auto appendPoint = [&](std::size_t index) -> bool {
        if (cancelled(cancel) || index >= count) return false;
        const auto window = ardirec::desktop::rms_cycle_window_for_sample(
            *times, index, nominalFrequency);
        if (!window.valid()) return true;

        long double sumSquares = 0.0L;
        std::size_t finiteCount = 0;
        std::size_t checked = 0;
        for (std::size_t sample = window.first; sample < window.end; ++sample) {
            if ((checked++ & 0xFFu) == 0u && cancelled(cancel)) return false;
            const double value = data->analogValue(sample, channel);
            if (!std::isfinite(value)) continue;
            sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
            ++finiteCount;
        }

        double rms = 0.0;
        if (finiteCount > 0) {
            const std::size_t presentCount = window.presentSamples();
            const std::size_t invalidPresent = presentCount > finiteCount
                                                   ? presentCount - finiteCount
                                                   : 0u;
            const std::size_t denominator = window.normalizationSamples > invalidPresent
                                                ? window.normalizationSamples - invalidPresent
                                                : finiteCount;
            if (denominator > 0) {
                rms = std::sqrt(static_cast<double>(
                          sumSquares / static_cast<long double>(denominator)))
                      * magnitudeScale;
            }
        }
        if (!std::isfinite(rms)) rms = 0.0;

        const double xFraction = std::clamp(
            ((*times)[index] - startTime) / visibleDuration, 0.0, 1.0);
        const double normalized = std::clamp(rms / scalePeak, 0.0, 1.0);
        snapshot->points.push_back({static_cast<float>(xFraction),
                                    static_cast<float>(normalized)});
        return true;
    };

    std::size_t lastIndex = start;
    for (std::size_t i = start; i < end; i += stride) {
        if (!appendPoint(i)) return {};
        lastIndex = i;
    }
    if (lastIndex != end - 1u && !appendPoint(end - 1u)) return {};

    if (cancelled(cancel) || snapshot->points.size() < 2u) return {};
    return snapshot;
}
} // namespace

RmsWaveformItem::RmsWaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(16);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &RmsWaveformItem::startRebuild);
}

RmsWaveformItem::~RmsWaveformItem() {
    m_rebuildTimer.stop();
    if (m_activeCancel) m_activeCancel->store(true, std::memory_order_relaxed);
}

void RmsWaveformItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, &RmsWaveformItem::reloadData);
        connect(m_document, &DocumentController::representationChanged, this, &RmsWaveformItem::refreshRepresentation);
    }
    reloadData();
    emit documentChanged();
}

void RmsWaveformItem::setChannelIndex(int value) {
    if (m_channelIndex == value) return;
    m_channelIndex = value;
    reloadData();
    emit channelIndexChanged();
}

void RmsWaveformItem::setTraceColor(const QColor& value) {
    if (m_traceColor == value) return;
    m_traceColor = value;
    update();
    emit traceColorChanged();
}

void RmsWaveformItem::setZoomFactor(double value) {
    value = std::clamp(value, 1.0, 500.0);
    if (qFuzzyCompare(m_zoomFactor, value)) return;
    m_zoomFactor = value;
    scheduleRebuild();
    emit viewChanged();
}

void RmsWaveformItem::setPanFraction(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_panFraction, value)) return;
    m_panFraction = value;
    scheduleRebuild();
    emit viewChanged();
}

void RmsWaveformItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())) scheduleRebuild();
    update();
}

void RmsWaveformItem::clearPreparedGeometry() {
    SnapshotPtr empty;
    std::atomic_store_explicit(&m_renderSnapshot, std::move(empty), std::memory_order_release);
    update();
}

void RmsWaveformItem::reloadData() {
    clearPreparedGeometry();
    if (!m_document) {
        m_data.reset();
        m_times.reset();
        m_displayScale = 1.0;
        m_scalePeak = 1.0;
        m_nominalFrequency = 50.0;
    } else {
        m_data = m_document->dataStoreSnapshot();
        m_times = m_document->timeIndexSnapshot();
        m_displayScale = m_document->channelDisplayScale(m_channelIndex);
        m_scalePeak = m_document->channelPeak(m_channelIndex) / std::sqrt(2.0);
        m_nominalFrequency = m_document->nominalFrequency() > 1.0
                                 ? m_document->nominalFrequency()
                                 : 50.0;
    }
    if (!std::isfinite(m_scalePeak) || m_scalePeak < 1.0e-12) m_scalePeak = 1.0;
    scheduleRebuild();
}

void RmsWaveformItem::refreshRepresentation() {
    clearPreparedGeometry();
    if (m_document) {
        m_displayScale = m_document->channelDisplayScale(m_channelIndex);
        m_scalePeak = m_document->channelPeak(m_channelIndex) / std::sqrt(2.0);
    } else {
        m_displayScale = 1.0;
        m_scalePeak = 1.0;
    }
    if (!std::isfinite(m_scalePeak) || m_scalePeak < 1.0e-12) m_scalePeak = 1.0;
    scheduleRebuild();
}

void RmsWaveformItem::scheduleRebuild() {
    ++m_rebuildGeneration;
    if (m_activeCancel) m_activeCancel->store(true, std::memory_order_relaxed);
    if (!m_rebuildTimer.isActive()) m_rebuildTimer.start();
}

void RmsWaveformItem::startRebuild() {
    const quint64 generation = m_rebuildGeneration;
    const auto data = m_data;
    const auto times = m_times;
    const int channelIndex = m_channelIndex;
    const double displayScale = m_displayScale;
    const double scalePeak = m_scalePeak;
    const double nominalFrequency = m_nominalFrequency;
    const double zoomFactor = m_zoomFactor;
    const double panFraction = m_panFraction;
    const int pixelWidth = static_cast<int>(std::clamp(width(), 0.0, 4096.0));

    if (!data || !times || channelIndex < 0 || pixelWidth <= 1) {
        clearPreparedGeometry();
        return;
    }

    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_activeCancel = cancel;
    using Watcher = QFutureWatcher<SnapshotPtr>;
    auto* watcher = new Watcher(this);
    connect(watcher, &Watcher::finished, this, [this, watcher, generation, cancel]() {
        SnapshotPtr snapshot;
        try {
            snapshot = watcher->result();
        } catch (...) {
            snapshot.reset();
        }
        watcher->deleteLater();
        if (generation != m_rebuildGeneration || cancelled(cancel)) return;
        if (m_activeCancel == cancel) m_activeCancel.reset();
        std::atomic_store_explicit(&m_renderSnapshot, std::move(snapshot), std::memory_order_release);
        update();
    });

    watcher->setFuture(QtConcurrent::run(
        [data, times, channelIndex, displayScale, scalePeak, nominalFrequency,
         zoomFactor, panFraction, pixelWidth, cancel]() {
            return build_rms_geometry(data, times, channelIndex, displayScale, scalePeak,
                                      nominalFrequency, zoomFactor, panFraction,
                                      pixelWidth, cancel);
        }));
}

QSGNode* RmsWaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    const auto snapshot = std::atomic_load_explicit(&m_renderSnapshot, std::memory_order_acquire);
    const std::size_t pointCount = snapshot ? snapshot->points.size() : 0u;
    if (pointCount < 2u || width() <= 1.0 || height() <= 1.0) {
        delete node;
        return nullptr;
    }

    if (!node) {
        node = new QSGGeometryNode;
        auto* material = new QSGFlatColorMaterial;
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
    material->setColor(m_traceColor);
    node->markDirty(QSGNode::DirtyMaterial);

    auto* geometry = node->geometry();
    if (!geometry || static_cast<std::size_t>(geometry->vertexCount()) != pointCount) {
        auto* replacement = new QSGGeometry(
            QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(pointCount));
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }
    geometry->setDrawingMode(QSGGeometry::DrawLineStrip);

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    for (std::size_t i = 0; i < pointCount; ++i) {
        const auto& point = snapshot->points[i];
        const float x = std::clamp(point.xFraction, 0.0F, 1.0F) * w;
        const float normalized = std::clamp(point.normalizedRms, 0.0F, 1.0F);
        const float y = h * (0.90F - normalized * 0.80F);
        vertices[i].set(x, y);
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
