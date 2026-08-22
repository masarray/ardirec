// SPDX-License-Identifier: GPL-3.0-or-later
#include "digital_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <vector>

DigitalItem::DigitalItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void DigitalItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, &DigitalItem::reloadSamples);
    }
    reloadSamples();
    emit documentChanged();
}

void DigitalItem::setChannelIndex(int value) {
    if (m_channelIndex == value) return;
    m_channelIndex = value;
    reloadSamples();
    emit channelChanged();
}

void DigitalItem::setZoomFactor(double value) {
    value = std::clamp(value, 1.0, 500.0);
    if (qFuzzyCompare(m_zoomFactor, value)) return;
    m_zoomFactor = value;
    update();
    emit viewChanged();
}

void DigitalItem::setPanFraction(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_panFraction, value)) return;
    m_panFraction = value;
    update();
    emit viewChanged();
}

void DigitalItem::setActiveColor(const QColor& value) {
    if (m_activeColor == value) return;
    m_activeColor = value;
    update();
    emit activeColorChanged();
}

void DigitalItem::reloadSamples() {
    m_samples = m_document ? m_document->statusSamples(m_channelIndex) : std::vector<std::uint8_t>{};
    update();
}

QSGNode* DigitalItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!m_document || m_samples.empty() || width() <= 1.0 || height() <= 1.0) {
        delete node;
        return nullptr;
    }

    const auto range = m_document->visibleSampleRange(m_zoomFactor, m_panFraction);
    const std::size_t start = range.first;
    const std::size_t end = std::min(range.second, m_samples.size());
    const auto& times = m_document->timeSeconds();
    if (start >= end || start >= times.size()) {
        delete node;
        return nullptr;
    }

    const double fullDuration = m_document->durationSeconds();
    const double visibleDuration = fullDuration > 0.0 ? fullDuration / m_zoomFactor : 0.0;
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double viewStart = m_document->dataStartSeconds() + m_panFraction * movable;
    const double viewEnd = viewStart + visibleDuration;
    if (visibleDuration <= 0.0) {
        delete node;
        return nullptr;
    }

    struct Run { double begin; double end; };
    std::vector<Run> runs;
    bool inRun = false;
    double runStart = viewStart;

    const auto sampleBoundary = [&](std::size_t index) {
        if (index < times.size()) return times[index];
        return viewEnd;
    };

    for (std::size_t i = start; i < end; ++i) {
        const bool high = m_samples[i] != 0;
        const double t0 = std::clamp(sampleBoundary(i), viewStart, viewEnd);
        if (high && !inRun) {
            runStart = t0;
            inRun = true;
        }
        const bool nextHigh = (i + 1 < end) ? (m_samples[i + 1] != 0) : false;
        if (inRun && (!nextHigh || i + 1 >= end)) {
            double t1 = viewEnd;
            if (i + 1 < times.size()) t1 = std::clamp(times[i + 1], viewStart, viewEnd);
            if (t1 <= runStart) t1 = std::min(viewEnd, runStart + visibleDuration / std::max(1.0, width()));
            runs.push_back({runStart, t1});
            inRun = false;
        }
    }

    if (runs.empty()) {
        delete node;
        return nullptr;
    }

    const std::size_t vertexCount = runs.size() * 6;
    if (!node) {
        node = new QSGGeometryNode;
        auto* material = new QSGFlatColorMaterial;
        material->setColor(m_activeColor);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    } else {
        static_cast<QSGFlatColorMaterial*>(node->material())->setColor(m_activeColor);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    auto* geometry = node->geometry();
    if (!geometry || static_cast<std::size_t>(geometry->vertexCount()) != vertexCount) {
        auto* replacement = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(vertexCount));
        replacement->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float top = static_cast<float>(height() * 0.20);
    const float bottom = static_cast<float>(height() * 0.80);

    std::size_t cursor = 0;
    for (const auto& run : runs) {
        const float x0 = static_cast<float>((run.begin - viewStart) / visibleDuration) * w;
        const float x1 = static_cast<float>((run.end - viewStart) / visibleDuration) * w;
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
