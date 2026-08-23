// SPDX-License-Identifier: GPL-3.0-or-later
#include "waveform_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

WaveformItem::WaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void WaveformItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, &WaveformItem::reloadSamples);
        connect(m_document, &DocumentController::representationChanged, this, &WaveformItem::refreshRepresentation);
    }
    reloadSamples();
    emit documentChanged();
}

void WaveformItem::setChannelIndex(int value) {
    if (m_channelIndex == value) return;
    m_channelIndex = value;
    reloadSamples();
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
    m_panFraction = std::clamp(m_panFraction, 0.0, 1.0);
    update();
    emit viewChanged();
}

void WaveformItem::setPanFraction(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_panFraction, value)) return;
    m_panFraction = value;
    update();
    emit viewChanged();
}

void WaveformItem::reloadSamples() {
    if (!m_document) {
        m_samples.clear();
        m_times.clear();
        m_displayScale = 1.0;
    } else {
        m_samples = m_document->analogSamples(m_channelIndex);
        m_times = m_document->timeSeconds();
        if (m_times.size() > m_samples.size()) m_times.resize(m_samples.size());
        if (m_samples.size() > m_times.size()) m_samples.resize(m_times.size());
        m_displayScale = m_document->channelDisplayScale(m_channelIndex);
    }
    update();
}

void WaveformItem::refreshRepresentation() {
    m_displayScale = m_document ? m_document->channelDisplayScale(m_channelIndex) : 1.0;
    update();
}

QSGNode* WaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (m_samples.size() < 2 || m_times.size() < 2 || width() <= 1.0 || height() <= 1.0) {
        delete node;
        return nullptr;
    }

    const double dataStart = m_times.front();
    const double dataEnd = m_times.back();
    const double fullDuration = std::max(0.0, dataEnd - dataStart);
    if (fullDuration <= 0.0) {
        delete node;
        return nullptr;
    }

    const double visibleDuration = fullDuration / std::clamp(m_zoomFactor, 1.0, 500.0);
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double startTime = dataStart + std::clamp(m_panFraction, 0.0, 1.0) * movable;
    const double endTime = startTime + visibleDuration;

    auto first = std::lower_bound(m_times.begin(), m_times.end(), startTime);
    auto last = std::upper_bound(m_times.begin(), m_times.end(), endTime);
    std::size_t start = static_cast<std::size_t>(std::distance(m_times.begin(), first));
    std::size_t end = static_cast<std::size_t>(std::distance(m_times.begin(), last));
    start = std::min(start, m_samples.size() - 1);
    end = std::min(end, m_samples.size());
    if (end <= start + 1) end = std::min(m_samples.size(), start + 2);
    const std::size_t visibleCount = end - start;

    double peak = 0.0;
    for (std::size_t i = start; i < end; ++i) {
        const double value = m_samples[i] * m_displayScale;
        if (std::isfinite(value)) peak = std::max(peak, std::abs(value));
    }
    if (!std::isfinite(peak) || peak < 1.0e-12) peak = 1.0;
    peak *= 1.08;

    const std::size_t pixelWidth = std::clamp<std::size_t>(static_cast<std::size_t>(width()), 64, 4096);
    const bool envelopeMode = visibleCount > pixelWidth * 8;
    std::size_t pointCount = 0;
    std::size_t stride = 1;
    if (envelopeMode) {
        pointCount = pixelWidth * 2;
    } else {
        const std::size_t targetPoints = std::max<std::size_t>(64, pixelWidth * 2);
        stride = std::max<std::size_t>(1, (visibleCount + targetPoints - 1) / targetPoints);
        pointCount = (visibleCount + stride - 1) / stride;
        pointCount = std::max<std::size_t>(2, pointCount);
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
        auto* replacement = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(pointCount));
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }
    geometry->setDrawingMode(envelopeMode ? QSGGeometry::DrawLines : QSGGeometry::DrawLineStrip);

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    const auto xForTime = [&](double value) {
        const double fraction = std::clamp((value - startTime) / visibleDuration, 0.0, 1.0);
        return static_cast<float>(fraction * static_cast<double>(w));
    };
    const auto yFor = [&](double value) {
        const double normalized = std::clamp(value / peak, -1.0, 1.0);
        return static_cast<float>(static_cast<double>(h) * (0.5 - normalized * 0.43));
    };

    if (!envelopeMode) {
        std::size_t out = 0;
        for (std::size_t i = start; i < end && out < pointCount; i += stride) {
            const double value = m_samples[i] * m_displayScale;
            vertices[out++].set(xForTime(m_times[i]), std::isfinite(value) ? yFor(value) : h * 0.5F);
        }
        while (out < pointCount) {
            const std::size_t i = end - 1;
            const double value = m_samples[i] * m_displayScale;
            vertices[out++].set(xForTime(m_times[i]), std::isfinite(value) ? yFor(value) : h * 0.5F);
        }
    } else {
        std::vector<double> lows(pixelWidth, std::numeric_limits<double>::infinity());
        std::vector<double> highs(pixelWidth, -std::numeric_limits<double>::infinity());
        for (std::size_t i = start; i < end; ++i) {
            const double value = m_samples[i] * m_displayScale;
            if (!std::isfinite(value)) continue;
            const double fraction = std::clamp((m_times[i] - startTime) / visibleDuration, 0.0, 1.0);
            const std::size_t bucket = std::min(pixelWidth - 1,
                                                static_cast<std::size_t>(fraction * static_cast<double>(pixelWidth - 1)));
            lows[bucket] = std::min(lows[bucket], value);
            highs[bucket] = std::max(highs[bucket], value);
        }
        double previous = 0.0;
        for (std::size_t bucket = 0; bucket < pixelWidth; ++bucket) {
            double lo = lows[bucket];
            double hi = highs[bucket];
            if (!std::isfinite(lo) || !std::isfinite(hi)) {
                lo = previous;
                hi = previous;
            } else {
                previous = (lo + hi) * 0.5;
            }
            const float x = pixelWidth > 1
                                ? w * static_cast<float>(bucket) / static_cast<float>(pixelWidth - 1)
                                : 0.0F;
            vertices[bucket * 2].set(x, yFor(lo));
            vertices[bucket * 2 + 1].set(x, yFor(hi));
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
