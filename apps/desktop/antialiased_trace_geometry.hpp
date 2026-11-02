// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Unified Qt Quick Scene Graph stroke geometry for Instant and RMS waveforms.
// 1px GPU lines are backend-dependent and aliased. Build a fixed-width, AA
// triangle mesh instead. This transforms *only* screen-space stroke edges;
// source COMTRADE samples, LOD extrema and RMS values are NEVER smoothed.
#include <QColor>
#include <QPointF>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ardirec::desktop {
template <typename PointContainer, typename Project>
QSGNode* update_antialiased_trace(QSGNode* oldNode,
                                 const PointContainer& points,
                                 bool envelopeMode,
                                 const QColor& traceColor,
                                 Project project) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    const std::size_t count = points.size();
    const std::size_t segments = envelopeMode ? count / 2u : (count > 0 ? count - 1u : 0u);
    // Bounding is intentional: the asynchronous workers already cap geometry
    // at <= 4096 x 2 points per lane, keeping this upload predictable.
    if (count < 2 || segments == 0 || count > 16384u) {
        delete node;
        return nullptr;
    }
    if (!node) {
        node = new QSGGeometryNode;
        auto* material = new QSGVertexColorMaterial;
        material->setFlag(QSGMaterial::Blending, true);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    const int vertexCount = static_cast<int>(count * 4u);
    const int indexCount = static_cast<int>(segments * 18u);
    auto* geometry = node->geometry();
    if (!geometry || geometry->vertexCount() != vertexCount || geometry->indexCount() != indexCount) {
        auto* replacement = new QSGGeometry(
            QSGGeometry::defaultAttributes_ColoredPoint2D(), vertexCount, indexCount,
            QSGGeometry::UnsignedIntType);
        replacement->setDrawingMode(QSGGeometry::DrawTriangles);
        replacement->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }

    auto* vertices = geometry->vertexDataAsColoredPoint2D();
    auto* indices = geometry->indexDataAsUInt();
    // Preserve premultiplied alpha. A 1.85px opaque core with 0.70px feather
    // on each side is legible even on a dense grid without distorting transients.
    const float halfCore = 0.925F;
    const float outer = halfCore + 0.70F;
    const float opacity = static_cast<float>(traceColor.alphaF());
    const auto red = static_cast<quint8>(std::lround(traceColor.red() * opacity));
    const auto green = static_cast<quint8>(std::lround(traceColor.green() * opacity));
    const auto blue = static_cast<quint8>(std::lround(traceColor.blue() * opacity));
    const auto alpha = static_cast<quint8>(std::lround(255.0F * opacity));

    for (std::size_t i = 0; i < count; ++i) {
        const QPointF here = project(points[i]);
        QPointF previous;
        QPointF following;
        if (envelopeMode) {
            const std::size_t partner = (i & 1u) ? i - 1u : std::min(i + 1u, count - 1u);
            previous = project(points[i & ~std::size_t{1}]);
            following = project(points[partner == i ? i : ((i & 1u) ? i : partner)]);
        } else {
            previous = project(points[i > 0 ? i - 1 : i]);
            following = project(points[std::min(i + 1, count - 1)]);
        }
        const float dx = static_cast<float>(following.x() - previous.x());
        const float dy = static_cast<float>(following.y() - previous.y());
        const float length = std::hypot(dx, dy);
        const float nx = length > 1e-5F ? -dy / length : 0.0F;
        const float ny = length > 1e-5F ? dx / length : 1.0F;
        const float x = static_cast<float>(here.x());
        const float y = static_cast<float>(here.y());
        auto* group = vertices + 4 * i;
        group[0].set(x - nx * outer, y - ny * outer, 0, 0, 0, 0);
        group[1].set(x - nx * halfCore, y - ny * halfCore, red, green, blue, alpha);
        group[2].set(x + nx * halfCore, y + ny * halfCore, red, green, blue, alpha);
        group[3].set(x + nx * outer, y + ny * outer, 0, 0, 0, 0);
    }

    for (std::size_t s = 0; s < segments; ++s) {
        const std::size_t a = envelopeMode ? 2 * s : s;
        const std::size_t b = a + 1;
        for (std::uint32_t band = 0; band < 3; ++band) {
            const std::uint32_t ia = static_cast<std::uint32_t>(4 * a) + band;
            const std::uint32_t ib = static_cast<std::uint32_t>(4 * b) + band;
            const std::size_t offset = s * 18 + 6 * band;
            indices[offset + 0] = ia;
            indices[offset + 1] = ia + 1;
            indices[offset + 2] = ib;
            indices[offset + 3] = ib;
            indices[offset + 4] = ia + 1;
            indices[offset + 5] = ib + 1;
        }
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
} // namespace ardirec::desktop
