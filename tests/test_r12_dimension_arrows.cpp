// SPDX-License-Identifier: GPL-3.0-or-later
// Live QML contract: outward-facing time dimension arrows and exact C1/C2 ticks.
// No screenshots, hard-coded cursor times or unsupported font glyphs.
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}
void near(double actual, double expected, const char* message) {
    require(std::isfinite(actual) && std::abs(actual - expected) < 1.0, message);
}
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    try {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(ARDIREC_QML_DIR) + "/EventStrip.qml"));
        require(component.status() == QQmlComponent::Ready, "EventStrip QML must load");
        std::unique_ptr<QObject> owner(component.create());
        require(owner != nullptr, "EventStrip QML object creation");
        auto* strip = qobject_cast<QQuickItem*>(owner.get());
        require(strip != nullptr, "EventStrip must be a QQuickItem");
        strip->setWidth(680);
        strip->setHeight(46);
        strip->setProperty("axisWidth", 170.0);
        strip->setProperty("viewStart", 0.0);
        strip->setProperty("visibleDuration", 1.0);
        strip->setProperty("cursorATime", 0.1);
        strip->setProperty("cursorBTime", 0.4);
        QCoreApplication::processEvents();

        auto* dimension = strip->findChild<QQuickItem*>("engineeringTimeDimension");
        auto* left = strip->findChild<QObject*>("dimensionLeftArrowPath");
        auto* right = strip->findChild<QObject*>("dimensionRightArrowPath");
        auto* leftTip = strip->findChild<QObject*>("dimensionLeftTip");
        auto* rightTip = strip->findChild<QObject*>("dimensionRightTip");
        auto* arrows = strip->findChild<QQuickItem*>("dimensionOutwardArrows");
        auto* leftTick = strip->findChild<QQuickItem*>("dimensionLeftTick");
        auto* rightTick = strip->findChild<QQuickItem*>("dimensionRightTick");
        require(dimension && left && right && leftTip && rightTip && arrows
                && leftTick && rightTick, "all QML dimension elements must exist");

        // 680 total - 170 label gutter = 510px active plot.
        const double lx = 51.0, rx = 204.0;
        near(dimension->property("leftX").toDouble(), lx, "C1 maps to correct x");
        near(dimension->property("rightX").toDouble(), rx, "C2 maps to correct x");
        near(leftTick->x(), lx, "left vertical endpoint tick is at C1");
        near(rightTick->x() + 1.0, rx, "right vertical endpoint tick is at C2");
        // Correct graphical convention: | <--- Δt ---> |
        near(left->property("startX").toDouble(), lx + 10.0, "left arrow tail");
        near(leftTip->property("x").toDouble(), lx + 4.0, "left arrow points outward");
        near(right->property("startX").toDouble(), rx - 10.0, "right arrow tail");
        near(rightTip->property("x").toDouble(), rx - 4.0, "right arrow points outward");
        require(arrows->isVisible(), "dimension arrows visible with sufficient space");

        strip->setProperty("cursorATime", 0.4);
        strip->setProperty("cursorBTime", 0.1);
        QCoreApplication::processEvents();
        near(dimension->property("leftX").toDouble(), lx,
             "swapping cursors preserves left endpoint");
        near(dimension->property("rightX").toDouble(), rx,
             "swapping cursors preserves right endpoint");

        strip->setProperty("cursorATime", 0.1);
        strip->setProperty("cursorBTime", 0.105);
        QCoreApplication::processEvents();
        require(!arrows->isVisible(), "short cursor span hides overlapping arrows");
        near(dimension->property("span").toDouble(), 2.55,
             "short span remains calibrated, not inflated to a minimum width");
        std::cout << "R12 outward time dimension: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "R12 time dimension: " << e.what() << '\n';
        return 1;
    }
}
