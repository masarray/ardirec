// SPDX-License-Identifier: GPL-3.0-or-later
// Regression for engineering reference lines: instantaneous zero = 50%,
// RMS zero = 90%, independent of window size or track data availability.
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
void verify_zero(QQuickItem* guides, QQuickItem* zero, int height,
                 const char* mode, qreal expected) {
    guides->setHeight(height);
    guides->setProperty("displayMode", mode);
    QCoreApplication::processEvents();
    require(std::abs(zero->y() - expected) < 0.6, "zero guide does not match renderer coordinates");
    require(std::abs(zero->height() - 2.0) < 0.01, "zero guide must stay 2px high");
    require(std::abs(zero->width() - guides->width()) < 0.01, "zero guide spans the entire plotting area");
}
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    try {
        QQmlEngine engine;
        QQmlComponent component(&engine,
            QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + "/PlotGuides.qml"));
        require(component.status() == QQmlComponent::Ready, "PlotGuides QML must load");
        std::unique_ptr<QObject> object(component.create());
        require(object != nullptr, "PlotGuides QML instance must create");
        auto* guides = qobject_cast<QQuickItem*>(object.get());
        require(guides != nullptr, "PlotGuides must be a QQuickItem");
        auto* zero = guides->findChild<QQuickItem*>("engineeringZeroLine");
        require(zero != nullptr, "zero line must be addressable for accessibility and tests");
        guides->setWidth(840);
        verify_zero(guides, zero, 200, "instantaneous", 99.0);
        verify_zero(guides, zero, 200, "rms", 179.0);
        verify_zero(guides, zero, 300, "rms", 269.0);
        verify_zero(guides, zero, 300, "instantaneous", 149.0);
        std::cout << "R9 plot guide coordinates: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "R9 plot guides: " << error.what() << '\n';
        return 1;
    }
}
