// SPDX-License-Identifier: GPL-3.0-or-later
#include "phasor_vector_item.hpp"
#include "phasor_vector_geometry.hpp"

#include <QColor>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
using Colored = QSGGeometry::ColoredPoint2D;

struct Premultiplied {
    quint8 red;
    quint8 green;
    quint8 blue;
    quint8 alpha;
};

Premultiplied tint(const QColor& color) {
    const float alpha = static_cast<float>(color.alphaF());
    return {
        static_cast<quint8>(std::lround(color.red() * alpha)),
        static_cast<quint8>(std::lround(color.green() * alpha)),
        static_cast<quint8>(std::lround(color.blue() * alpha)),
        static_cast<quint8>(std::lround(255.0F * alpha))
    };
}

void paint(Colored& vertex, const QPointF& point, Premultiplied color, bool transparent = false) {
    if (transparent) {
        vertex.set(static_cast<float>(point.x()), static_cast<float>(point.y()), 0, 0, 0, 0);
    } else {
        vertex.set(static_cast<float>(point.x()), static_cast<float>(point.y()),
                   color.red, color.green, color.blue, color.alpha);
    }
}

QSGGeometryNode* geometry_node(int vertices, int triangles) {
    auto* node = new QSGGeometryNode;
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),
                                     vertices, triangles * 3);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    auto* material = new QSGVertexColorMaterial;
    material->setFlag(QSGMaterial::Blending, true);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

class VectorNode final : public QSGNode {
public:
    VectorNode() {
        line = geometry_node(8, 6);
        arrow = geometry_node(15, 7);
        appendChildNode(line);
        appendChildNode(arrow);
        // Four vertices per endpoint; left feather / inner core / inner core / right feather.
        auto* indexes = line->geometry()->indexDataAsUShort();
        for (int band = 0; band < 3; ++band) {
            const int at = band * 6;
            indexes[at + 0] = static_cast<quint16>(band);
            indexes[at + 1] = static_cast<quint16>(band + 1);
            indexes[at + 2] = static_cast<quint16>(band + 4);
            indexes[at + 3] = static_cast<quint16>(band + 4);
            indexes[at + 4] = static_cast<quint16>(band + 1);
            indexes[at + 5] = static_cast<quint16>(band + 5);
        }
        indexes = arrow->geometry()->indexDataAsUShort();
        indexes[0] = 0;
        indexes[1] = 1;
        indexes[2] = 2;
        // Three fading edge strips with no separate detached triangular tip.
        for (int edge = 0; edge < 3; ++edge) {
            const int v = 3 + 4 * edge;
            const int index = 3 + 6 * edge;
            indexes[index + 0] = static_cast<quint16>(v);
            indexes[index + 1] = static_cast<quint16>(v + 1);
            indexes[index + 2] = static_cast<quint16>(v + 2);
            indexes[index + 3] = static_cast<quint16>(v + 2);
            indexes[index + 4] = static_cast<quint16>(v + 1);
            indexes[index + 5] = static_cast<quint16>(v + 3);
        }
    }
    QSGGeometryNode* line{nullptr};
    QSGGeometryNode* arrow{nullptr};
};

class VectorRootNode final : public QSGNode {};

void clear_vectors(VectorRootNode* root) {
    while (QSGNode* child = root->firstChild()) {
        root->removeChildNode(child);
        delete child;
    }
}

void update_vector(VectorNode* node, const ardirec::desktop::PhasorStroke& shape,
                   const QColor& color, bool earth) {
    const Premultiplied ink = tint(color);
    // Thick, readable, smooth, calibrated shaft. QSG DrawLines/setLineWidth
    // are backend-dependent (typically rasterized at 1 physical pixel).
    const float halfCore = earth ? 1.15F : 1.65F;
    const float feather = 0.75F;
    const QPointF delta = shape.tip - shape.origin;
    const float len = std::hypot(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
    const QPointF normal = len > 1e-5F
        ? QPointF(-delta.y() / len, delta.x() / len) : QPointF(0, 1);
    const float offsets[4] = {-halfCore-feather, -halfCore, halfCore, halfCore+feather};
    Colored* line = node->line->geometry()->vertexDataAsColoredPoint2D();
    for (int end = 0; end < 2; ++end) {
        const QPointF position = end == 0 ? shape.origin : shape.shaftEnd;
        for (int i = 0; i < 4; ++i)
            paint(line[4 * end + i], position + normal * offsets[i],
                  ink, i == 0 || i == 3 || len < 0.2F);
    }
    node->line->markDirty(QSGNode::DirtyGeometry);

    Colored* arrow = node->arrow->geometry()->vertexDataAsColoredPoint2D();
    const QPointF corners[3] = {shape.tip, shape.headLeft, shape.headRight};
    if (!shape.showArrowhead) {
        // Retain the QSG node while hiding an oversized head on a short vector.
        for (int v = 0; v < 15; ++v) paint(arrow[v], shape.tip, ink, true);
    } else {
        for (int v = 0; v < 3; ++v) paint(arrow[v], corners[v], ink);
        constexpr float featherWidth = 0.9F;
        for (int edge = 0; edge < 3; ++edge) {
            const QPointF a = corners[edge];
            const QPointF b = corners[(edge + 1) % 3];
            const QPointF segment = b - a;
            const float distance = std::hypot(static_cast<float>(segment.x()),
                                              static_cast<float>(segment.y()));
            const QPointF outward = distance > 1e-5F
                ? QPointF(segment.y() / distance, -segment.x() / distance)
                : QPointF(0, 0);
            const int v = 3 + 4 * edge;
            paint(arrow[v + 0], a, ink);
            paint(arrow[v + 1], b, ink);
            paint(arrow[v + 2], a + outward * featherWidth, ink, true);
            paint(arrow[v + 3], b + outward * featherWidth, ink, true);
        }
    }
    node->arrow->markDirty(QSGNode::DirtyGeometry);
}

std::vector<QVariantMap> valid_rows(const QVariantList& vectors) {
    std::vector<QVariantMap> rows;
    rows.reserve(static_cast<std::size_t>(vectors.size()));
    for (const QVariant& value : vectors) {
        const QVariantMap row = value.toMap();
        if (!row.value(QStringLiteral("valid")).toBool()) continue;
        const double magnitude = row.value(QStringLiteral("magnitude")).toDouble();
        const double angle = row.value(QStringLiteral("angle")).toDouble();
        if (!std::isfinite(magnitude) || magnitude < 0.0 || !std::isfinite(angle)) continue;
        rows.push_back(row);
    }
    return rows;
}
} // namespace

PhasorVectorItem::PhasorVectorItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void PhasorVectorItem::setVectors(const QVariantList& vectors) {
    if (m_vectors == vectors) return;
    m_vectors = vectors;
    emit vectorsChanged();
    update();
}

void PhasorVectorItem::setScaleMagnitude(double value) {
    if (!std::isfinite(value)) value = 0.0;
    value = std::max(0.0, value);
    if (qFuzzyCompare(m_scaleMagnitude + 1.0, value + 1.0)) return;
    m_scaleMagnitude = value;
    emit scaleMagnitudeChanged();
    update();
}

QSGNode* PhasorVectorItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* root = oldNode ? static_cast<VectorRootNode*>(oldNode) : new VectorRootNode();
    const auto rows = valid_rows(m_vectors);
    if (width() < 8.0 || height() < 8.0 || !(m_scaleMagnitude > 0.0) || rows.empty()) {
        clear_vectors(root);
        return root;
    }

    while (root->childCount() < static_cast<int>(rows.size()))
        root->appendChildNode(new VectorNode);
    while (root->childCount() > static_cast<int>(rows.size())) {
        QSGNode* child = root->lastChild();
        root->removeChildNode(child);
        delete child;
    }

    QSGNode* child = root->firstChild();
    for (const QVariantMap& row : rows) {
        auto* vectorNode = static_cast<VectorNode*>(child);
        const auto geometry = ardirec::desktop::project_phasor(
            width(), height(), row.value(QStringLiteral("magnitude")).toDouble(),
            m_scaleMagnitude, row.value(QStringLiteral("angle")).toDouble());
        const QColor color(row.value(QStringLiteral("color"),
                       QStringLiteral("#6f7780")).toString());
        update_vector(vectorNode, geometry, color,
                      row.value(QStringLiteral("phase")).toString() == QStringLiteral("E"));
        child = child->nextSibling();
    }
    return root;
}
