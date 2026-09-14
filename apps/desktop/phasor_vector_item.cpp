// SPDX-License-Identifier: GPL-3.0-or-later
#include "phasor_vector_item.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

QSGGeometryNode* make_line_node() {
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2);
    geometry->setDrawingMode(QSGGeometry::DrawLines);

    auto* material = new QSGFlatColorMaterial();

    auto* node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode* make_arrow_node() {
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 3);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);

    auto* material = new QSGFlatColorMaterial();

    auto* node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

void update_line(QSGGeometryNode* node,
                 float x1, float y1, float x2, float y2,
                 const QColor& color, float width) {
    auto* geometry = node->geometry();
    geometry->setLineWidth(width);
    auto* vertices = geometry->vertexDataAsPoint2D();
    vertices[0].set(x1, y1);
    vertices[1].set(x2, y2);
    node->markDirty(QSGNode::DirtyGeometry);

    auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
    if (material->color() != color) {
        material->setColor(color);
        node->markDirty(QSGNode::DirtyMaterial);
    }
}

void update_arrow(QSGGeometryNode* node,
                  float x, float y, double angleRadians,
                  const QColor& color) {
    constexpr float head = 9.0f;
    const float x1 = x - static_cast<float>(std::cos(angleRadians - 0.45) * head);
    const float y1 = y + static_cast<float>(std::sin(angleRadians - 0.45) * head);
    const float x2 = x - static_cast<float>(std::cos(angleRadians + 0.45) * head);
    const float y2 = y + static_cast<float>(std::sin(angleRadians + 0.45) * head);

    auto* geometry = node->geometry();
    auto* vertices = geometry->vertexDataAsPoint2D();
    vertices[0].set(x, y);
    vertices[1].set(x1, y1);
    vertices[2].set(x2, y2);
    node->markDirty(QSGNode::DirtyGeometry);

    auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
    if (material->color() != color) {
        material->setColor(color);
        node->markDirty(QSGNode::DirtyMaterial);
    }
}

class VectorNode final : public QSGNode {
public:
    VectorNode() {
        line = make_line_node();
        arrow = make_arrow_node();
        appendChildNode(line);
        appendChildNode(arrow);
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

std::vector<QVariantMap> valid_rows(const QVariantList& vectors) {
    std::vector<QVariantMap> rows;
    rows.reserve(static_cast<std::size_t>(vectors.size()));
    for (const QVariant& value : vectors) {
        const QVariantMap row = value.toMap();
        if (!row.value(QStringLiteral("valid")).toBool()) continue;
        const double magnitude = row.value(QStringLiteral("magnitude")).toDouble();
        const double angleDegrees = row.value(QStringLiteral("angle")).toDouble();
        if (!std::isfinite(magnitude) || !std::isfinite(angleDegrees)) continue;
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

    const float cx = static_cast<float>(width() * 0.5);
    const float cy = static_cast<float>(height() * 0.5);
    const float radius = static_cast<float>(std::max(4.0, std::min(width(), height()) * 0.43));

    while (root->childCount() < static_cast<int>(rows.size()))
        root->appendChildNode(new VectorNode());
    while (root->childCount() > static_cast<int>(rows.size())) {
        QSGNode* child = root->lastChild();
        root->removeChildNode(child);
        delete child;
    }

    QSGNode* child = root->firstChild();
    for (const QVariantMap& row : rows) {
        auto* vectorNode = static_cast<VectorNode*>(child);
        const double magnitude = row.value(QStringLiteral("magnitude")).toDouble();
        const double angleDegrees = row.value(QStringLiteral("angle")).toDouble();
        const double angle = angleDegrees * kPi / 180.0;
        const double fraction = std::clamp(magnitude / m_scaleMagnitude, 0.0, 1.0);
        const float length = radius * 0.94f * static_cast<float>(fraction);
        const float ex = cx + static_cast<float>(std::cos(angle) * length);
        const float ey = cy - static_cast<float>(std::sin(angle) * length);
        const QColor color(row.value(QStringLiteral("color"), QStringLiteral("#6f7780")).toString());
        const QString phase = row.value(QStringLiteral("phase")).toString();
        const float lineWidth = phase == QStringLiteral("E") ? 1.8f : 2.4f;

        update_line(vectorNode->line, cx, cy, ex, ey, color, lineWidth);
        update_arrow(vectorNode->arrow, ex, ey, angle, color);
        child = child->nextSibling();
    }
    return root;
}
