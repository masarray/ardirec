// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Screen-space geometry only. Magnitudes and angles originate from the
// committed one-cycle DFT snapshot; never alter signal, zero or phase data.
#include <QPointF>

#include <algorithm>
#include <cmath>

namespace ardirec::desktop {

struct PhasorStroke {
    QPointF origin;
    QPointF tip;
    QPointF shaftEnd;
    QPointF headLeft;
    QPointF headRight;
    float radius{0.0F};
    float length{0.0F};
    float headLength{0.0F};
    float headHalfWidth{0.0F};
    bool showArrowhead{false};
};

inline PhasorStroke project_phasor(double width, double height,
                                    double magnitude, double scale,
                                    double angleDegrees) {
    PhasorStroke shape;
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(magnitude)
        || !std::isfinite(scale) || !std::isfinite(angleDegrees)
        || width <= 0.0 || height <= 0.0 || scale <= 0.0) {
        return shape;
    }

    constexpr double pi = 3.1415926535897932384626433832795;
    const double radians = angleDegrees * pi / 180.0;
    const float ux = static_cast<float>(std::cos(radians));
    const float uy = static_cast<float>(-std::sin(radians));
    const float nx = -uy;
    const float ny = ux;
    shape.origin = {width * 0.5, height * 0.5};
    shape.radius = static_cast<float>(std::max(4.0, std::min(width, height) * 0.40));
    const double fraction = std::clamp(magnitude / scale, 0.0, 1.0);
    shape.length = shape.radius * 0.90F * static_cast<float>(fraction);
    shape.tip = shape.origin + QPointF(ux * shape.length, uy * shape.length);
    shape.shaftEnd = shape.tip;
    // For near-zero genuine vectors, drawing a constant-size 9px triangle
    // creates an inverted/overgrown "arrow". Do not falsify magnitude.
    shape.showArrowhead = shape.length >= 14.0F;
    if (shape.showArrowhead) {
        shape.headLength = std::clamp(shape.length * 0.22F, 6.0F, 11.0F);
        shape.headHalfWidth = std::clamp(shape.length * 0.080F, 3.0F, 5.5F);
        const QPointF base = shape.tip - QPointF(ux * shape.headLength, uy * shape.headLength);
        shape.headLeft = base + QPointF(nx * shape.headHalfWidth, ny * shape.headHalfWidth);
        shape.headRight = base - QPointF(nx * shape.headHalfWidth, ny * shape.headHalfWidth);
        shape.shaftEnd = shape.tip - QPointF(ux * shape.headLength * 0.55F,
                                            uy * shape.headLength * 0.55F);
    }
    return shape;
}
} // namespace ardirec::desktop
