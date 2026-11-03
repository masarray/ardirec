// SPDX-License-Identifier: GPL-3.0-or-later
#include "phasor_vector_geometry.hpp"
#include "phasor_vector_item.hpp"

#include <QGuiApplication>
#include <QSGGeometryNode>
#include <QSGNode>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool valid, const char* message) {
    if (!valid) throw std::runtime_error(message);
}
void near(double value, double expected, double margin, const char* message) {
    require(std::isfinite(value) && std::abs(value - expected) <= margin, message);
}
class ExposedPhasor final : public PhasorVectorItem {
public:
    using PhasorVectorItem::updatePaintNode;
};
QVariantList rows(double magnitude, double angle = 0.0) {
    return QVariantList{QVariantMap{
        {QStringLiteral("valid"), true},
        {QStringLiteral("magnitude"), magnitude},
        {QStringLiteral("angle"), angle},
        {QStringLiteral("color"), QStringLiteral("#ff241a")},
        {QStringLiteral("phase"), QStringLiteral("L1")}
    }};
}
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    try {
        using ardirec::desktop::project_phasor;
        const auto longVector = project_phasor(500, 300, 60.0, 67.2, 0.0);
        near(longVector.radius, 120.0, 0.001, "ring radius matches QML grid");
        near(longVector.length, 120.0 * 0.90 * (60.0/67.2), 0.01,
             "FIT is proportional to true magnitude");
        require(longVector.showArrowhead, "long vector has a head");
        require(longVector.headLength <= 9.0F && longVector.headHalfWidth <= 4.0F,
                "head has bounded size; no spikes");
        near(longVector.tip.y(), 150.0, 0.01, "zero degree points right");
        const auto downVector = project_phasor(500, 300, 60.0, 67.2, -90.0);
        require(downVector.tip.y() > downVector.origin.y(), "negative angle points down");
        const auto small = project_phasor(500, 300, 0.5, 67.2, -118.0);
        require(small.length < 1.0 && !small.showArrowhead,
                "small measured vector cannot draw oversized tip or fake length");
        const auto zero = project_phasor(500, 300, 0.0, 67.2, 35.0);
        require(zero.length == 0.0F && !zero.showArrowhead, "zero magnitude remains zero");
        const auto invalid = project_phasor(500, 300, 50, 0.0, 0);
        require(invalid.length == 0.0F, "invalid scale cannot create geometry");

        ExposedPhasor item;
        item.setWidth(500.0);
        item.setHeight(300.0);
        item.setScaleMagnitude(67.2);
        item.setVectors(rows(60.0));
        QSGNode* root = item.updatePaintNode(nullptr, nullptr);
        require(root && root->childCount() == 1, "single retained vector node");
        QSGNode* vectorNode = root->firstChild();
        auto* shaft = static_cast<QSGGeometryNode*>(vectorNode->firstChild());
        auto* arrow = static_cast<QSGGeometryNode*>(vectorNode->lastChild());
        require(shaft->geometry()->drawingMode() == QSGGeometry::DrawTriangles,
                "shaft must avoid raster-thin GPU DrawLines");
        require(shaft->geometry()->vertexCount() == 8
             && shaft->geometry()->indexCount() == 18, "bounded AA shaft topology");
        require(arrow->geometry()->vertexCount() == 15
             && arrow->geometry()->indexCount() == 21, "bounded feathered arrow topology");
        const auto* v = shaft->geometry()->vertexDataAsColoredPoint2D();
        require(v[0].a == 0 && v[1].a == 255 && v[2].a == 255 && v[3].a == 0,
                "feather fades to transparent beside opaque stroke");
        const auto* tip = arrow->geometry()->vertexDataAsColoredPoint2D();
        near(tip[0].x, longVector.tip.x(), 0.01, "head tip joins measured vector endpoint");
        near(tip[0].y, longVector.tip.y(), 0.01, "head tip y joins measured endpoint");
        item.setVectors(rows(0.5));
        QSGNode* recycled = item.updatePaintNode(root, nullptr);
        require(recycled == root && recycled->firstChild() == vectorNode,
                "scrubbing preserves scenegraph nodes");
        tip = arrow->geometry()->vertexDataAsColoredPoint2D();
        for (int i = 0; i < 15; ++i)
            require(tip[i].a == 0, "short vector cannot show a detached large arrow");
        delete root;
        std::cout << "R11 phasor geometry and arrowhead: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "R11 phasor geometry: " << ex.what() << '\n';
        return 1;
    }
}
