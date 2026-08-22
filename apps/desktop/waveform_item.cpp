// SPDX-License-Identifier: GPL-3.0-or-later
#include "waveform_item.hpp"

#include "document_controller.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <limits>

WaveformItem::WaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void WaveformItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::waveformChanged, this, &WaveformItem::reloadSamples);
    }
    reloadSamples();
    emit documentChanged();
}

void WaveformItem::setZoomFactor(double value) {
    value = std::clamp(value, 1.0, 200.0);
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
    m_samples = m_document ? m_document->selectedSamples() : std::vector<double>{};
    update();
}

QSGNode* WaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (m_samples.empty() || width() <= 1.0 || height() <= 1.0) {
        delete node;
        return nullptr;
    }

    const std::size_t total = m_samples.size();
    const std::size_t visibleCount = std::max<std::size_t>(2, static_cast<std::size_t>(static_cast<double>(total) / m_zoomFactor));
    const std::size_t maxStart = total > visibleCount ? total - visibleCount : 0;
    const std::size_t start = static_cast<std::size_t>(std::round(m_panFraction * static_cast<double>(maxStart)));
    const std::size_t end = std::min(total, start + visibleCount);

    double minValue = std::numeric_limits<double>::infinity();
    double maxValue = -std::numeric_limits<double>::infinity();
    for (std::size_t i = start; i < end; ++i) {
        const double v = m_samples[i];
        if (!std::isfinite(v)) continue;
        minValue = std::min(minValue, v);
        maxValue = std::max(maxValue, v);
    }
    if (!std::isfinite(minValue) || !std::isfinite(maxValue)) {
        delete node;
        return nullptr;
    }
    if (std::abs(maxValue - minValue) < 1e-12) {
        minValue -= 1.0;
        maxValue += 1.0;
    }

    const std::size_t pixelBuckets = std::clamp<std::size_t>(static_cast<std::size_t>(width()), 64, 4096);
    const bool envelopeMode = (end - start) > pixelBuckets;
    const std::size_t pointCount = envelopeMode ? pixelBuckets * 2 : (end - start);

    if (!node) {
        node = new QSGGeometryNode;
        auto* material = new QSGFlatColorMaterial;
        material->setColor(QColor("#67d7ff"));
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    auto* geometry = node->geometry();
    if (!geometry || static_cast<std::size_t>(geometry->vertexCount()) != pointCount) {
        auto* replacement = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(pointCount));
        replacement->setDrawingMode(QSGGeometry::DrawLineStrip);
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    const double span = maxValue - minValue;
    const auto yFor = [&](double value) {
        const double normalized = (value - minValue) / span;
        return static_cast<float>(h * (0.90 - normalized * 0.80));
    };

    if (!envelopeMode) {
        const std::size_t count = end - start;
        for (std::size_t p = 0; p < count; ++p) {
            const double v = m_samples[start + p];
            const float x = count > 1 ? w * static_cast<float>(p) / static_cast<float>(count - 1) : 0.0F;
            vertices[p].set(x, std::isfinite(v) ? yFor(v) : h * 0.5F);
        }
    } else {
        for (std::size_t bucket = 0; bucket < pixelBuckets; ++bucket) {
            const std::size_t b0 = start + ((end - start) * bucket) / pixelBuckets;
            const std::size_t b1 = start + ((end - start) * (bucket + 1)) / pixelBuckets;
            double lo = std::numeric_limits<double>::infinity();
            double hi = -std::numeric_limits<double>::infinity();
            for (std::size_t i = b0; i < std::max(b0 + 1, b1) && i < end; ++i) {
                const double v = m_samples[i];
                if (!std::isfinite(v)) continue;
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
            if (!std::isfinite(lo)) lo = 0.0;
            if (!std::isfinite(hi)) hi = lo;
            const float x = pixelBuckets > 1 ? w * static_cast<float>(bucket) / static_cast<float>(pixelBuckets - 1) : 0.0F;
            vertices[bucket * 2].set(x, yFor(lo));
            vertices[bucket * 2 + 1].set(x, yFor(hi));
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
