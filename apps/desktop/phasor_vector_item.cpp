// SPDX-License-Identifier: GPL-3.0-or-later
#include "phasor_vector_item.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

QSGGeometryNode* make_line(float x1, float y1, float x2, float y2, const QColor& color, float width) {
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2);
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(width);
    auto* vertices = geometry->vertexDataAsPoint2D();
    vertices[0].set(x1, y1);
    vertices[1].set(x2, y2);

    auto* material = new QSGFlatColorMaterial();
    material->setColor(color);

    auto* node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode* make_arrow(float x, float y, double angleRadians, const QColor& color) {
    constexpr float head = 9.0f;
    const float x1 = x - static_cast<float>(std::cos(angleRadians - 0.45) * head);
    const float y1 = y + static_cast<float>(std::sin(angleRadians - 0.45) * head);
    const float x2 = x - static_cast<float>(std::cos(angleRadians + 0.45) * head);
    const float y2 = y + static_cast<float>(std::sin(angleRadians + 0.45) * head);

    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 3);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vertices = geometry->vertexDataAsPoint2D();
    vertices[0].set(x, y);
    vertices[1].set(x1, y1);
    vertices[2].set(x2, y2);

    auto* material = new QSGFlatColorMaterial();
    material->setColor(color);

    auto* node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
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
    delete oldNode;
    auto* root = new QSGNode();

    if (width() < 8.0 || height() < 8.0 || !(m_scaleMagnitude > 0.0)) return root;

    const float cx = static_cast<float>(width() * 0.5);
    const float cy = static_cast<float>(height() * 0.5);
    const float radius = static_cast<float>(std::max(4.0, std::min(width(), height()) * 0.43));

    for (const QVariant& value : m_vectors) {
        const QVariantMap row = value.toMap();
        if (!row.value(QStringLiteral("valid")).toBool()) continue;
        const double magnitude = row.value(QStringLiteral("magnitude")).toDouble();
        const double angleDegrees = row.value(QStringLiteral("angle")).toDouble();
        if (!std::isfinite(magnitude) || !std::isfinite(angleDegrees)) continue;

        const double angle = angleDegrees * kPi / 180.0;
        const double fraction = std::clamp(magnitude / m_scaleMagnitude, 0.0, 1.0);
        const float length = radius * 0.94f * static_cast<float>(fraction);
        const float ex = cx + static_cast<float>(std::cos(angle) * length);
        const float ey = cy - static_cast<float>(std::sin(angle) * length);
        const QColor color(row.value(QStringLiteral("color"), QStringLiteral("#6f7780")).toString());
        const QString phase = row.value(QStringLiteral("phase")).toString();
        const float lineWidth = phase == QStringLiteral("E") ? 1.8f : 2.4f;

        root->appendChildNode(make_line(cx, cy, ex, ey, color, lineWidth));
        root->appendChildNode(make_arrow(ex, ey, angle, color));
    }
    return root;
}
