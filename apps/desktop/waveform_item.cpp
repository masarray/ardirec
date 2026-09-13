// SPDX-License-Identifier: GPL-3.0-or-later
#include "waveform_item.hpp"

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

struct WaveformGeometrySnapshot {
    struct Point {
        float xFraction{0.0F};
        float normalizedValue{0.0F};
    };
    std::vector<Point> points;
    bool envelopeMode{false};
};

namespace {
using SnapshotPtr = std::shared_ptr<const WaveformGeometrySnapshot>;

[[nodiscard]] bool cancelled(const std::shared_ptr<std::atomic_bool>& flag) noexcept {
    return flag && flag->load(std::memory_order_relaxed);
}

[[nodiscard]] SnapshotPtr build_waveform_geometry(
    const std::shared_ptr<const ardirec::comtrade::IndexedDatFile>& data,
    const std::shared_ptr<const std::vector<double>>& times,
    const std::shared_ptr<const ardirec::comtrade::AnalogLodIndex>& lod,
    int channelIndex,
    double displayScale,
    double zoomFactor,
    double panFraction,
    int requestedPixelWidth,
    const std::shared_ptr<std::atomic_bool>& cancel) {
    if (!data || !times || channelIndex < 0 || requestedPixelWidth <= 1 || cancelled(cancel)) return {};

    const std::size_t count = std::min(data->frameCount(), times->size());
    const std::size_t channel = static_cast<std::size_t>(channelIndex);
    if (count < 2u || channel >= data->analogCount()) return {};

    const double dataStart = times->front();
    const double dataEnd = (*times)[count - 1u];
    const double fullDuration = std::max(0.0, dataEnd - dataStart);
    if (!(fullDuration > 0.0) || !std::isfinite(fullDuration)) return {};

    zoomFactor = std::clamp(zoomFactor, 1.0, 500.0);
    panFraction = std::clamp(panFraction, 0.0, 1.0);
    if (!std::isfinite(displayScale)) displayScale = 1.0;

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

    // Preserve continuity to viewport borders with one genuine neighbour on
    // each side. X is clipped to [0,1] in the prepared snapshot.
    if (start > 0u) --start;
    if (end < count) ++end;
    if (end <= start + 1u) end = std::min(count, start + 2u);
    if (end <= start + 1u) return {};

    const std::size_t visibleCount = end - start;
    const std::size_t pixelWidth = std::clamp<std::size_t>(
        static_cast<std::size_t>(requestedPixelWidth), 64u, 4096u);
    const bool envelopeMode = visibleCount > pixelWidth * 8u;

    auto snapshot = std::make_shared<WaveformGeometrySnapshot>();
    snapshot->envelopeMode = envelopeMode;
    double peak = 0.0;

    const auto xFractionFor = [&](std::size_t index) {
        return static_cast<float>(std::clamp(
            ((*times)[index] - startTime) / visibleDuration, 0.0, 1.0));
    };

    if (envelopeMode) {
        std::vector<double> bucketLows(pixelWidth, std::numeric_limits<double>::infinity());
        std::vector<double> bucketHighs(pixelWidth, -std::numeric_limits<double>::infinity());
        const std::size_t samplesPerPixel = std::max<std::size_t>(1u, visibleCount / pixelWidth);
        const bool lodCompatible = lod && lod->block_size > 0u
                                   && channel < lod->channel_count
                                   && lod->block_size <= samplesPerPixel * 2u;

        const auto accumulate = [&](std::size_t representativeIndex, double rawLow, double rawHigh) {
            if (representativeIndex >= count || !std::isfinite(rawLow) || !std::isfinite(rawHigh)) return;
            double low = rawLow * displayScale;
            double high = rawHigh * displayScale;
            if (low > high) std::swap(low, high);
            if (!std::isfinite(low) || !std::isfinite(high)) return;
            const double fraction = std::clamp(
                ((*times)[representativeIndex] - startTime) / visibleDuration, 0.0, 1.0);
            const std::size_t bucket = std::min(
                pixelWidth - 1u,
                static_cast<std::size_t>(fraction * static_cast<double>(pixelWidth - 1u)));
            bucketLows[bucket] = std::min(bucketLows[bucket], low);
            bucketHighs[bucket] = std::max(bucketHighs[bucket], high);
            peak = std::max(peak, std::max(std::abs(low), std::abs(high)));
        };

        const auto accumulateRaw = [&](std::size_t firstIndex, std::size_t lastIndex) -> bool {
            std::size_t checked = 0u;
            for (std::size_t i = firstIndex; i < lastIndex; ++i) {
                if ((checked++ & 0xFFu) == 0u && cancelled(cancel)) return false;
                const double value = data->analogValue(i, channel);
                if (std::isfinite(value)) accumulate(i, value, value);
            }
            return true;
        };

        if (lodCompatible) {
            const std::size_t blockSize = lod->block_size;
            const std::size_t firstFullBlock = (start + blockSize - 1u) / blockSize;
            const std::size_t lastFullBlockExclusive = end / blockSize;

            if (firstFullBlock < lastFullBlockExclusive) {
                const std::size_t prefixEnd = std::min(end, firstFullBlock * blockSize);
                if (!accumulateRaw(start, prefixEnd)) return {};

                std::size_t checked = 0u;
                for (std::size_t block = firstFullBlock; block < lastFullBlockExclusive; ++block) {
                    if ((checked++ & 0xFFu) == 0u && cancelled(cancel)) return {};
                    double low = 0.0;
                    double high = 0.0;
                    if (!lod->blockExtrema(channel, block, low, high)) continue;
                    const std::size_t blockStart = block * blockSize;
                    const std::size_t representative = std::min(
                        count - 1u, blockStart + (blockSize - 1u) / 2u);
                    accumulate(representative, low, high);
                }

                const std::size_t suffixStart = std::max(start, lastFullBlockExclusive * blockSize);
                if (!accumulateRaw(suffixStart, end)) return {};
            } else if (!accumulateRaw(start, end)) {
                return {};
            }
        } else if (!accumulateRaw(start, end)) {
            return {};
        }

        if (!std::isfinite(peak) || peak < 1.0e-12) peak = 1.0;
        peak *= 1.08;

        snapshot->points.reserve(pixelWidth * 2u);
        double previous = 0.0;
        for (std::size_t bucket = 0; bucket < pixelWidth; ++bucket) {
            if ((bucket & 0xFFu) == 0u && cancelled(cancel)) return {};
            double low = bucketLows[bucket];
            double high = bucketHighs[bucket];
            if (!std::isfinite(low) || !std::isfinite(high)) {
                low = previous;
                high = previous;
            } else {
                previous = (low + high) * 0.5;
            }
            const float x = pixelWidth > 1u
                                ? static_cast<float>(bucket) / static_cast<float>(pixelWidth - 1u)
                                : 0.0F;
            snapshot->points.push_back({x, static_cast<float>(std::clamp(low / peak, -1.0, 1.0))});
            snapshot->points.push_back({x, static_cast<float>(std::clamp(high / peak, -1.0, 1.0))});
        }
    } else {
        std::size_t checked = 0u;
        for (std::size_t i = start; i < end; ++i) {
            if ((checked++ & 0xFFu) == 0u && cancelled(cancel)) return {};
            const double value = data->analogValue(i, channel) * displayScale;
            if (std::isfinite(value)) peak = std::max(peak, std::abs(value));
        }
        if (!std::isfinite(peak) || peak < 1.0e-12) peak = 1.0;
        peak *= 1.08;

        const std::size_t targetPoints = std::min(
            visibleCount, std::max<std::size_t>(64u, pixelWidth * 2u));
        const std::size_t stride = std::max<std::size_t>(
            1u, (visibleCount + targetPoints - 1u) / targetPoints);
        snapshot->points.reserve(targetPoints + 1u);

        std::size_t lastIndex = start;
        for (std::size_t i = start; i < end; i += stride) {
            if (cancelled(cancel)) return {};
            const double value = data->analogValue(i, channel) * displayScale;
            const double normalized = std::isfinite(value) ? std::clamp(value / peak, -1.0, 1.0) : 0.0;
            snapshot->points.push_back({xFractionFor(i), static_cast<float>(normalized)});
            lastIndex = i;
        }
        if (lastIndex != end - 1u) {
            const std::size_t i = end - 1u;
            const double value = data->analogValue(i, channel) * displayScale;
            const double normalized = std::isfinite(value) ? std::clamp(value / peak, -1.0, 1.0) : 0.0;
            snapshot->points.push_back({xFractionFor(i), static_cast<float>(normalized)});
        }
    }

    if (cancelled(cancel) || snapshot->points.size() < 2u) return {};
    return snapshot;
}
} // namespace

WaveformItem::WaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(16);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &WaveformItem::startRebuild);
}

WaveformItem::~WaveformItem() {
    m_rebuildTimer.stop();
    if (m_activeCancel) m_activeCancel->store(true, std::memory_order_relaxed);
}

void WaveformItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, &WaveformItem::reloadData);
        connect(m_document, &DocumentController::representationChanged, this, &WaveformItem::refreshRepresentation);
    }
    reloadData();
    emit documentChanged();
}

void WaveformItem::setChannelIndex(int value) {
    if (m_channelIndex == value) return;
    m_channelIndex = value;
    reloadData();
    emit channelIndexChanged();
}

void WaveformItem::setTraceColor(const QColor& value) {
    if (m_traceColor == value) return;
    m_traceColor = value;
    update();
    emit traceColorChanged();
}

void WaveformItem::setZoomFactor(double value) {
    value = std::clamp(value, 1.0, 500.0);
    if (qFuzzyCompare(m_zoomFactor, value)) return;
    m_zoomFactor = value;
    scheduleRebuild();
    emit viewChanged();
}

void WaveformItem::setPanFraction(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_panFraction, value)) return;
    m_panFraction = value;
    scheduleRebuild();
    emit viewChanged();
}

void WaveformItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())) scheduleRebuild();
    update();
}

void WaveformItem::clearPreparedGeometry() {
    SnapshotPtr empty;
    std::atomic_store_explicit(&m_renderSnapshot, std::move(empty), std::memory_order_release);
    update();
}

void WaveformItem::reloadData() {
    clearPreparedGeometry();
    if (!m_document) {
        m_data.reset();
        m_times.reset();
        m_lod.reset();
        m_displayScale = 1.0;
    } else {
        m_data = m_document->dataStoreSnapshot();
        m_times = m_document->timeIndexSnapshot();
        m_lod = m_document->analogLodSnapshot();
        m_displayScale = m_document->channelDisplayScale(m_channelIndex);
    }
    scheduleRebuild();
}

void WaveformItem::refreshRepresentation() {
    clearPreparedGeometry();
    m_displayScale = m_document ? m_document->channelDisplayScale(m_channelIndex) : 1.0;
    scheduleRebuild();
}

void WaveformItem::scheduleRebuild() {
    ++m_rebuildGeneration;
    if (m_activeCancel) m_activeCancel->store(true, std::memory_order_relaxed);
    if (!m_rebuildTimer.isActive()) m_rebuildTimer.start();
}

void WaveformItem::startRebuild() {
    const quint64 generation = m_rebuildGeneration;
    const auto data = m_data;
    const auto times = m_times;
    const auto lod = m_lod;
    const int channelIndex = m_channelIndex;
    const double displayScale = m_displayScale;
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
        [data, times, lod, channelIndex, displayScale, zoomFactor, panFraction, pixelWidth, cancel]() {
            return build_waveform_geometry(data, times, lod, channelIndex, displayScale,
                                           zoomFactor, panFraction, pixelWidth, cancel);
        }));
}

QSGNode* WaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
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
    geometry->setDrawingMode(snapshot->envelopeMode ? QSGGeometry::DrawLines : QSGGeometry::DrawLineStrip);

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    for (std::size_t i = 0; i < pointCount; ++i) {
        const auto& point = snapshot->points[i];
        const float x = std::clamp(point.xFraction, 0.0F, 1.0F) * w;
        const float normalized = std::clamp(point.normalizedValue, -1.0F, 1.0F);
        const float y = h * (0.5F - normalized * 0.43F);
        vertices[i].set(x, y);
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
