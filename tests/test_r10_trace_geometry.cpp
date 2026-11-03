// SPDX-License-Identifier: GPL-3.0-or-later
#include "../apps/desktop/antialiased_trace_geometry.hpp"
#include <QGuiApplication>
#include <QPointF>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    try {
        const std::vector<QPointF> points{{0, 20}, {10, 20}, {20, 40}};
        const auto project = [](const QPointF& point) { return point; };
        const QColor color(20, 110, 210);
        QSGNode* result = ardirec::desktop::update_antialiased_trace(
            nullptr, points, false, color, project);
        require(result != nullptr, "must create a geometry node");
        auto* node = static_cast<QSGGeometryNode*>(result);
        require(dynamic_cast<QSGVertexColorMaterial*>(node->material()) != nullptr,
                "use vertex color material for antialiased edges");
        auto* geometry = node->geometry();
        require(geometry->drawingMode() == QSGGeometry::DrawTriangles,
                "GPU triangle geometry is required; backend-dependent thin lines are forbidden");
        require(geometry->vertexCount() == 12 && geometry->indexCount() == 36,
                "three regular points produce two four-band point connections");
        auto* v = geometry->vertexDataAsColoredPoint2D();
        require(v[0].a == 0 && v[1].a == 255 && v[2].a == 255 && v[3].a == 0,
                "outer feather is transparent and inner stroke is opaque");
        require(std::abs(v[1].y - 19.30F) < 0.02F,
                "core width is 1.40 logical pixels");
        require(std::abs(v[0].y - 18.45F) < 0.02F,
                "antialiased feather extends smoothly past core");
        const QSGNode* old = result;
        result = ardirec::desktop::update_antialiased_trace(result, points, false,
                                                             QColor(180, 60, 40), project);
        require(result == old, "same geometry count reuses scenegraph node");
        const auto* v2 = static_cast<QSGGeometryNode*>(result)->geometry()
                             ->vertexDataAsColoredPoint2D();
        require(v2[1].r == 180 && v2[1].g == 60, "changing trace color updates vertex colors");
        delete result;

        const std::vector<QPointF> extrema{{1, 4}, {1, 30}, {2, 7}, {2, 40}};
        auto* env = static_cast<QSGGeometryNode*>(
            ardirec::desktop::update_antialiased_trace(nullptr, extrema, true, color, project));
        require(env != nullptr, "envelope mesh exists");
        require(env->geometry()->vertexCount() == 16 && env->geometry()->indexCount() == 36,
                "envelope segments are rendered as separate min/max vertical strokes");
        auto* faces = env->geometry()->indexDataAsUInt();
        require(faces[0] == 0 && faces[18] == 8, "envelope pair does not connect adjacent buckets");
        delete env;
        std::cout << "R10 AA waveform stroke: PASS\n";
    } catch (const std::exception& e) {
        std::cerr << "R10 AA waveform stroke failed: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
