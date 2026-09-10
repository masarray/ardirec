// SPDX-License-Identifier: GPL-3.0-or-later
#include "rms_waveform_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>

RmsWaveformItem::RmsWaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
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
    update();
    emit viewChanged();
}

void RmsWaveformItem::setPanFraction(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_panFraction, value)) return;
    m_panFraction = value;
    update();
    emit viewChanged();
}

void RmsWaveformItem::reloadData() {
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
        m_nominalFrequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    }
    if (!std::isfinite(m_scalePeak) || m_scalePeak < 1.0e-12) m_scalePeak = 1.0;
    update();
}

void RmsWaveformItem::refreshRepresentation() {
    if (m_document) {
        m_displayScale = m_document->channelDisplayScale(m_channelIndex);
        m_scalePeak = m_document->channelPeak(m_channelIndex) / std::sqrt(2.0);
    } else {
        m_displayScale = 1.0;
        m_scalePeak = 1.0;
    }
    if (!std::isfinite(m_scalePeak) || m_scalePeak < 1.0e-12) m_scalePeak = 1.0;
    update();
}

QSGNode* RmsWaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    const auto data = m_data;
    const auto times = m_times;
    const std::size_t count = data && times ? std::min(data->frameCount(), times->size()) : 0;
    if (count < 2 || m_channelIndex < 0
        || static_cast<std::size_t>(m_channelIndex) >= (data ? data->analogCount() : 0)
        || width() <= 1.0 || height() <= 1.0) {
        delete node;
        return nullptr;
    }

    const double dataStart = times->front();
    const double dataEnd = (*times)[count - 1];
    const double fullDuration = std::max(0.0, dataEnd - dataStart);
    if (fullDuration <= 0.0) {
        delete node;
        return nullptr;
    }

    const double visibleDuration = fullDuration / std::clamp(m_zoomFactor, 1.0, 500.0);
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double startTime = dataStart + std::clamp(m_panFraction, 0.0, 1.0) * movable;
    const double endTime = startTime + visibleDuration;

    const auto logicalEnd = times->begin() + static_cast<std::ptrdiff_t>(count);
    auto first = std::lower_bound(times->begin(), logicalEnd, startTime);
    auto last = std::upper_bound(times->begin(), logicalEnd, endTime);
    std::size_t start = static_cast<std::size_t>(std::distance(times->begin(), first));
    std::size_t end = static_cast<std::size_t>(std::distance(times->begin(), last));
    start = std::min(start, count - 1);
    end = std::min(end, count);
    if (start > 0) --start;
    if (end < count) ++end;
    if (end <= start + 1) end = std::min(count, start + 2);
    if (end <= start + 1) {
        delete node;
        return nullptr;
    }
    const std::size_t visibleCount = end - start;

    const std::size_t pixelWidth = std::clamp<std::size_t>(static_cast<std::size_t>(width()), 64, 4096);
    const std::size_t targetPoints = std::min(visibleCount, std::max<std::size_t>(64, pixelWidth * 2));
    const std::size_t stride = std::max<std::size_t>(1, (visibleCount + targetPoints - 1) / targetPoints);
    std::size_t pointCount = (visibleCount + stride - 1) / stride;
    const bool appendFinal = start + (pointCount - 1) * stride != end - 1;
    if (appendFinal) ++pointCount;

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
        auto* replacement = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(pointCount));
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }
    geometry->setDrawingMode(QSGGeometry::DrawLineStrip);

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    const double scalePeak = std::max(1.0e-12, m_scalePeak);
    const double period = 1.0 / std::max(1.0, m_nominalFrequency);
    const std::size_t channel = static_cast<std::size_t>(m_channelIndex);

    const auto xForTime = [&](double value) {
        const double fraction = std::clamp((value - startTime) / visibleDuration, 0.0, 1.0);
        return static_cast<float>(fraction * static_cast<double>(w));
    };
    const auto yFor = [&](double value) {
        const double normalized = std::clamp(value / scalePeak, 0.0, 1.0);
        return static_cast<float>(static_cast<double>(h) * (0.90 - normalized * 0.80));
    };
    const auto rmsAt = [&](std::size_t index) {
        const double windowEnd = (*times)[index];
        const double windowStart = windowEnd - period;
        const auto firstIt = std::lower_bound(times->begin(),
                                              times->begin() + static_cast<std::ptrdiff_t>(index + 1),
                                              windowStart);
        const std::size_t firstIndex = static_cast<std::size_t>(std::distance(times->begin(), firstIt));
        long double sumSquares = 0.0L;
        std::size_t finiteCount = 0;
        for (std::size_t sample = firstIndex; sample <= index; ++sample) {
            const double value = data->analogValue(sample, channel);
            if (!std::isfinite(value)) continue;
            sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
            ++finiteCount;
        }
        if (finiteCount == 0) return 0.0;
        return std::sqrt(static_cast<double>(sumSquares / static_cast<long double>(finiteCount)))
               * std::abs(m_displayScale);
    };

    std::size_t out = 0;
    std::size_t lastIndex = start;
    for (std::size_t i = start; i < end && out < pointCount; i += stride) {
        const double value = rmsAt(i);
        vertices[out++].set(xForTime((*times)[i]), std::isfinite(value) ? yFor(value) : h * 0.9F);
        lastIndex = i;
    }
    if (lastIndex != end - 1 && out < pointCount) {
        const std::size_t i = end - 1;
        const double value = rmsAt(i);
        vertices[out++].set(xForTime((*times)[i]), std::isfinite(value) ? yFor(value) : h * 0.9F);
    }
    while (out < pointCount) vertices[out++] = vertices[out - 1];

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
