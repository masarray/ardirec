// SPDX-License-Identifier: GPL-3.0-or-later
#include "waveform_item.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <cmath>

WaveformItem::WaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

QSGNode* WaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    constexpr int kPoints = 600;
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), kPoints);
        geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        auto* material = new QSGFlatColorMaterial;
        material->setColor(QColor("#67d7ff"));
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    auto* vertices = node->geometry()->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    for (int i = 0; i < kPoints; ++i) {
        const float x = w * static_cast<float>(i) / static_cast<float>(kPoints - 1);
        const float t = static_cast<float>(i) / 28.0F;
        const float envelope = i > 260 ? 1.65F : 0.65F;
        const float y = h * 0.5F + std::sin(t) * h * 0.16F * envelope;
        vertices[i].set(x, y);
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
