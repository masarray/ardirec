// SPDX-License-Identifier: GPL-3.0-or-later
#include "rms_waveform_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

RmsWaveformItem::RmsWaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void RmsWaveformItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, &RmsWaveformItem::reloadSamples);
    }
    reloadSamples();
    emit documentChanged();
}

void RmsWaveformItem::setChannelIndex(int value) {
    if (m_channelIndex == value) return;
    m_channelIndex = value;
    reloadSamples();
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

void RmsWaveformItem::reloadSamples() {
    if (!m_document) {
        m_samples.clear();
        m_times.clear();
        m_rmsSamples.clear();
    } else {
        m_samples = m_document->analogSamples(m_channelIndex);
        m_times = m_document->timeSeconds();
        if (m_times.size() > m_samples.size()) m_times.resize(m_samples.size());
        if (m_samples.size() > m_times.size()) m_samples.resize(m_times.size());
        rebuildRms();
    }
    update();
}

void RmsWaveformItem::rebuildRms() {
    m_rmsSamples.assign(m_samples.size(), 0.0);
    if (!m_document || m_samples.empty() || m_times.size() != m_samples.size()) return;

    const double frequency = m_document->nominalFrequency() > 1.0 ? m_document->nominalFrequency() : 50.0;
    const double period = 1.0 / frequency;
    std::size_t left = 0;
    long double sumSquares = 0.0L;
    std::size_t finiteCount = 0;

    for (std::size_t right = 0; right < m_samples.size(); ++right) {
        const double value = m_samples[right];
        if (std::isfinite(value)) {
            sumSquares += static_cast<long double>(value) * static_cast<long double>(value);
            ++finiteCount;
        }

        while (left < right && m_times[right] - m_times[left] >= period) {
            const double old = m_samples[left];
            if (std::isfinite(old)) {
                sumSquares -= static_cast<long double>(old) * static_cast<long double>(old);
                if (finiteCount > 0) --finiteCount;
            }
            ++left;
        }

        m_rmsSamples[right] = finiteCount > 0
                                  ? std::sqrt(static_cast<double>(sumSquares / static_cast<long double>(finiteCount)))
                                  : 0.0;
    }
}

QSGNode* RmsWaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (m_rmsSamples.size() < 2 || m_times.size() < 2 || width() <= 1.0 || height() <= 1.0) {
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
    start = std::min(start, m_rmsSamples.size() - 1);
    end = std::min(end, m_rmsSamples.size());
    if (end <= start + 1) end = std::min(m_rmsSamples.size(), start + 2);
    const std::size_t visibleCount = end - start;

    double scalePeak = m_document ? m_document->channelPeak(m_channelIndex) / std::sqrt(2.0) : 1.0;
    if (!std::isfinite(scalePeak) || scalePeak < 1.0e-12) scalePeak = 1.0;
    for (std::size_t i = start; i < end; ++i) {
        if (std::isfinite(m_rmsSamples[i])) scalePeak = std::max(scalePeak, m_rmsSamples[i] * 1.03);
    }

    const std::size_t pixelWidth = std::clamp<std::size_t>(static_cast<std::size_t>(width()), 64, 4096);
    const std::size_t targetPoints = std::max<std::size_t>(64, pixelWidth * 2);
    const std::size_t stride = std::max<std::size_t>(1, (visibleCount + targetPoints - 1) / targetPoints);
    const std::size_t pointCount = std::max<std::size_t>(2, (visibleCount + stride - 1) / stride);

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
    const auto xForTime = [&](double value) {
        const double fraction = std::clamp((value - startTime) / visibleDuration, 0.0, 1.0);
        return static_cast<float>(fraction * static_cast<double>(w));
    };
    const auto yFor = [&](double value) {
        const double normalized = std::clamp(value / scalePeak, 0.0, 1.0);
        return static_cast<float>(static_cast<double>(h) * (0.90 - normalized * 0.80));
    };

    std::size_t out = 0;
    for (std::size_t i = start; i < end && out < pointCount; i += stride) {
        const double value = m_rmsSamples[i];
        vertices[out++].set(xForTime(m_times[i]), std::isfinite(value) ? yFor(value) : h * 0.9F);
    }
    while (out < pointCount) {
        const std::size_t i = end - 1;
        const double value = m_rmsSamples[i];
        vertices[out++].set(xForTime(m_times[i]), std::isfinite(value) ? yFor(value) : h * 0.9F);
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
