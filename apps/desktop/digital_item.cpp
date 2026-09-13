// SPDX-License-Identifier: GPL-3.0-or-later
#include "digital_item.hpp"

#include <QFutureWatcher>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

struct DigitalGeometrySnapshot {
    struct Run {
        float beginFraction{0.0F};
        float endFraction{0.0F};
    };
    std::vector<Run> runs;
};

namespace {
using SnapshotPtr = std::shared_ptr<const DigitalGeometrySnapshot>;

[[nodiscard]] bool cancelled(const std::shared_ptr<std::atomic_bool>& flag) noexcept {
    return flag && flag->load(std::memory_order_relaxed);
}

[[nodiscard]] SnapshotPtr build_digital_geometry(
    const std::shared_ptr<const ardirec::comtrade::IndexedDatFile>& data,
    const std::shared_ptr<const std::vector<double>>& times,
    int channelIndex,
    double zoomFactor,
    double panFraction,
    int requestedPixelWidth,
    const std::shared_ptr<std::atomic_bool>& cancel) {
    if (!data || !times || channelIndex < 0 || requestedPixelWidth <= 1 || cancelled(cancel)) return {};

    const std::size_t count = std::min(data->frameCount(), times->size());
    const std::size_t channel = static_cast<std::size_t>(channelIndex);
    if (count == 0u || channel >= data->statusCount()) return {};

    const double dataStart = times->front();
    const double dataEnd = (*times)[count - 1u];
    const double fullDuration = std::max(0.0, dataEnd - dataStart);
    if (!(fullDuration > 0.0) || !std::isfinite(fullDuration)) return {};

    zoomFactor = std::clamp(zoomFactor, 1.0, 500.0);
    panFraction = std::clamp(panFraction, 0.0, 1.0);
    const double visibleDuration = fullDuration / zoomFactor;
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double viewStart = dataStart + panFraction * movable;
    const double viewEnd = viewStart + visibleDuration;

    const auto logicalEnd = times->begin() + static_cast<std::ptrdiff_t>(count);
    const auto firstIt = std::lower_bound(times->begin(), logicalEnd, viewStart);
    const auto endIt = std::upper_bound(times->begin(), logicalEnd, viewEnd);
    std::size_t start = static_cast<std::size_t>(std::distance(times->begin(), firstIt));
    std::size_t end = static_cast<std::size_t>(std::distance(times->begin(), endIt));
    start = std::min(start, count - 1u);
    end = std::min(end, count);
    if (end <= start) return {};

    const std::size_t pixelWidth = std::clamp<std::size_t>(
        static_cast<std::size_t>(requestedPixelWidth), 64u, 4096u);
    const double minimumVisibleRun = visibleDuration / static_cast<double>(pixelWidth);

    auto snapshot = std::make_shared<DigitalGeometrySnapshot>();
    snapshot->runs.reserve(std::min<std::size_t>(pixelWidth, 512u));

    const auto fractionFor = [&](double time) {
        return static_cast<float>(std::clamp((time - viewStart) / visibleDuration, 0.0, 1.0));
    };

    bool inRun = false;
    double runStart = viewStart;
    std::size_t checked = 0u;
    for (std::size_t i = start; i < end; ++i) {
        if ((checked++ & 0xFFu) == 0u && cancelled(cancel)) return {};
        const bool high = data->statusValue(i, channel);
        const double t0 = std::clamp((*times)[i], viewStart, viewEnd);
        if (high && !inRun) {
            runStart = t0;
            inRun = true;
        }

        const bool nextHigh = (i + 1u < end) ? data->statusValue(i + 1u, channel) : false;
        if (inRun && (!nextHigh || i + 1u >= end)) {
            double runEnd = viewEnd;
            if (i + 1u < count) runEnd = std::clamp((*times)[i + 1u], viewStart, viewEnd);
            runEnd = std::min(viewEnd, std::max(runEnd, runStart + minimumVisibleRun));

            // At overview zoom, multiple transitions can map into the same pixel.
            // Merge those runs rather than producing geometry proportional to the
            // number of source samples. Short assertions remain visibly preserved.
            if (!snapshot->runs.empty()) {
                auto& previous = snapshot->runs.back();
                const double previousEnd = viewStart + static_cast<double>(previous.endFraction) * visibleDuration;
                if (runStart - previousEnd <= minimumVisibleRun) {
                    previous.endFraction = std::max(previous.endFraction, fractionFor(runEnd));
                } else {
                    snapshot->runs.push_back({fractionFor(runStart), fractionFor(runEnd)});
                }
            } else {
                snapshot->runs.push_back({fractionFor(runStart), fractionFor(runEnd)});
            }
            inRun = false;
        }
    }

    if (cancelled(cancel)) return {};
    return snapshot;
}
} // namespace

DigitalItem::DigitalItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(16);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &DigitalItem::startRebuild);
}

DigitalItem::~DigitalItem() {
    m_rebuildTimer.stop();
    if (m_activeCancel) m_activeCancel->store(true, std::memory_order_relaxed);
}

void DigitalItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, &DigitalItem::reloadData);
    }
    reloadData();
    emit documentChanged();
}

void DigitalItem::setChannelIndex(int value) {
    if (m_channelIndex == value) return;
    m_channelIndex = value;
    reloadData();
    emit channelChanged();
}

void DigitalItem::setZoomFactor(double value) {
    value = std::clamp(value, 1.0, 500.0);
    if (qFuzzyCompare(m_zoomFactor, value)) return;
    m_zoomFactor = value;
    scheduleRebuild();
    emit viewChanged();
}

void DigitalItem::setPanFraction(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_panFraction, value)) return;
    m_panFraction = value;
    scheduleRebuild();
    emit viewChanged();
}

void DigitalItem::setActiveColor(const QColor& value) {
    if (m_activeColor == value) return;
    m_activeColor = value;
    update();
    emit activeColorChanged();
}

void DigitalItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())) scheduleRebuild();
    update();
}

void DigitalItem::clearPreparedGeometry() {
    SnapshotPtr empty;
    std::atomic_store_explicit(&m_renderSnapshot, std::move(empty), std::memory_order_release);
    update();
}

void DigitalItem::reloadData() {
    clearPreparedGeometry();
    if (!m_document) {
        m_data.reset();
        m_times.reset();
    } else {
        m_data = m_document->dataStoreSnapshot();
        m_times = m_document->timeIndexSnapshot();
    }
    scheduleRebuild();
}

void DigitalItem::scheduleRebuild() {
    ++m_rebuildGeneration;
    if (m_activeCancel) m_activeCancel->store(true, std::memory_order_relaxed);
    if (!m_rebuildTimer.isActive()) m_rebuildTimer.start();
}

void DigitalItem::startRebuild() {
    const quint64 generation = m_rebuildGeneration;
    const auto data = m_data;
    const auto times = m_times;
    const int channelIndex = m_channelIndex;
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
        [data, times, channelIndex, zoomFactor, panFraction, pixelWidth, cancel]() {
            return build_digital_geometry(data, times, channelIndex, zoomFactor,
                                          panFraction, pixelWidth, cancel);
        }));
}

QSGNode* DigitalItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    const auto snapshot = std::atomic_load_explicit(&m_renderSnapshot, std::memory_order_acquire);
    const std::size_t runCount = snapshot ? snapshot->runs.size() : 0u;
    if (runCount == 0u || width() <= 1.0 || height() <= 1.0) {
        delete node;
        return nullptr;
    }

    const std::size_t vertexCount = runCount * 6u;
    if (!node) {
        node = new QSGGeometryNode;
        auto* material = new QSGFlatColorMaterial;
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
    material->setColor(m_activeColor);
    node->markDirty(QSGNode::DirtyMaterial);

    auto* geometry = node->geometry();
    if (!geometry || static_cast<std::size_t>(geometry->vertexCount()) != vertexCount) {
        auto* replacement = new QSGGeometry(
            QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(vertexCount));
        replacement->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float top = static_cast<float>(height() * 0.20);
    const float bottom = static_cast<float>(height() * 0.80);

    std::size_t cursor = 0u;
    for (const auto& run : snapshot->runs) {
        const float x0 = std::clamp(run.beginFraction, 0.0F, 1.0F) * w;
        const float x1 = std::clamp(run.endFraction, 0.0F, 1.0F) * w;
        vertices[cursor++].set(x0, top);
        vertices[cursor++].set(x1, top);
        vertices[cursor++].set(x0, bottom);
        vertices[cursor++].set(x0, bottom);
        vertices[cursor++].set(x1, top);
        vertices[cursor++].set(x1, bottom);
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
