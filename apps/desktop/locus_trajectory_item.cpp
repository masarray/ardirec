// SPDX-License-Identifier: GPL-3.0-or-later
#include "locus_trajectory_item.hpp"

#include "locus_snapshot_controller.hpp"

#include <QColor>
#include <QPointF>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {
constexpr std::array<const char*, 6> kLoopIds{{"L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"}};
constexpr std::array<const char*, 6> kLoopColors{{"#00923f", "#e000d0", "#1769d2", "#6789ee", "#00a184", "#b568c4"}};
constexpr std::array<const char*, 3> kRawIds{{"L1", "L2", "L3"}};
constexpr std::array<const char*, 3> kRawColors{{"#d32f2f", "#d6a700", "#1976d2"}};

QSGGeometryNode* make_segment(const std::vector<QPointF>& points,
                              const QColor& color,
                              bool selected) {
    if (points.size() < 2) return nullptr;
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),
                                     static_cast<int>(points.size()));
    geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
    geometry->setLineWidth(selected ? 1.8f : 1.1f);
    geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
    auto* vertices = geometry->vertexDataAsPoint2D();
    for (std::size_t index = 0; index < points.size(); ++index) {
        vertices[index].set(static_cast<float>(points[index].x()), static_cast<float>(points[index].y()));
    }

    auto* material = new QSGFlatColorMaterial();
    QColor display = color;
    display.setAlphaF(selected ? 1.0f : 0.90f);
    material->setColor(display);

    auto* node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}
} // namespace

LocusTrajectoryItem::LocusTrajectoryItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

QObject* LocusTrajectoryItem::source() const {
    return m_source.data();
}

void LocusTrajectoryItem::setSource(QObject* value) {
    auto* typed = qobject_cast<LocusSnapshotController*>(value);
    if (m_source == typed) return;
    if (m_source) disconnect(m_source, nullptr, this, nullptr);
    m_source = typed;
    if (m_source) connect(m_source, &LocusSnapshotController::snapshotChanged, this, [this]() { update(); });
    emit sourceChanged();
    update();
}

void LocusTrajectoryItem::setRawPhase(bool value) {
    if (m_rawPhase == value) return;
    m_rawPhase = value;
    emit geometryChanged();
    update();
}

void LocusTrajectoryItem::setFirstLoop(int value) {
    value = std::clamp(value, 0, 5);
    if (m_firstLoop == value) return;
    m_firstLoop = value;
    emit geometryChanged();
    update();
}

void LocusTrajectoryItem::setLoopCount(int value) {
    value = std::clamp(value, 1, 6);
    if (m_loopCount == value) return;
    m_loopCount = value;
    emit geometryChanged();
    update();
}

void LocusTrajectoryItem::setVisibilityMask(int value) {
    if (m_visibilityMask == value) return;
    m_visibilityMask = value;
    emit geometryChanged();
    update();
}

void LocusTrajectoryItem::setSelectedLoop(const QString& value) {
    if (m_selectedLoop == value) return;
    m_selectedLoop = value;
    emit geometryChanged();
    update();
}

void LocusTrajectoryItem::setRHalf(double value) {
    if (!std::isfinite(value) || value <= 0.0) value = 1.0;
    if (qFuzzyCompare(m_rHalf + 1.0, value + 1.0)) return;
    m_rHalf = value;
    emit geometryChanged();
    update();
}

void LocusTrajectoryItem::setXHalf(double value) {
    if (!std::isfinite(value) || value <= 0.0) value = 1.0;
    if (qFuzzyCompare(m_xHalf + 1.0, value + 1.0)) return;
    m_xHalf = value;
    emit geometryChanged();
    update();
}

QSGNode* LocusTrajectoryItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    delete oldNode;
    auto* root = new QSGNode();
    if (!m_source || width() < 2.0 || height() < 2.0 || !(m_rHalf > 0.0) || !(m_xHalf > 0.0)) return root;
    const auto snapshot = m_source->nativeSnapshot();
    if (!snapshot) return root;

    const double widthPixels = width();
    const double heightPixels = height();
    const int available = m_rawPhase ? 3 : 6;
    const int start = m_rawPhase ? 0 : std::clamp(m_firstLoop, 0, available - 1);
    const int last = std::min(available, start + m_loopCount);

    for (int loopIndex = start; loopIndex < last; ++loopIndex) {
        const int maskBit = loopIndex;
        if ((m_visibilityMask & (1 << maskBit)) == 0) continue;
        const auto& points = m_rawPhase
                                 ? snapshot->rawPhase[static_cast<std::size_t>(loopIndex)]
                                 : snapshot->loops[static_cast<std::size_t>(loopIndex)];
        const QString id = QString::fromLatin1(m_rawPhase
                                                   ? kRawIds[static_cast<std::size_t>(loopIndex)]
                                                   : kLoopIds[static_cast<std::size_t>(loopIndex)]);
        const QColor color(QString::fromLatin1(m_rawPhase
                                                   ? kRawColors[static_cast<std::size_t>(loopIndex)]
                                                   : kLoopColors[static_cast<std::size_t>(loopIndex)]));
        const bool selected = m_selectedLoop.compare(id, Qt::CaseInsensitive) == 0;
        std::vector<QPointF> segment;
        segment.reserve(points.size());
        auto flush = [&]() {
            if (auto* node = make_segment(segment, color, selected)) root->appendChildNode(node);
            segment.clear();
        };
        for (const auto& point : points) {
            if (!point.valid || !std::isfinite(point.r) || !std::isfinite(point.x)) {
                flush();
                continue;
            }
            const double px = (static_cast<double>(point.r) + m_rHalf) / (2.0 * m_rHalf) * widthPixels;
            const double py = (m_xHalf - static_cast<double>(point.x)) / (2.0 * m_xHalf) * heightPixels;
            if (px < -widthPixels || px > widthPixels * 2.0 || py < -heightPixels || py > heightPixels * 2.0) {
                flush();
                continue;
            }
            segment.emplace_back(px, py);
        }
        flush();
    }
    return root;
}
