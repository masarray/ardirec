// SPDX-License-Identifier: GPL-3.0-or-later
#include "cursor_snapshot_controller.hpp"
#include "document_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

void wait_for_document(DocumentController& document, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (document.loading() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(!document.loading(), "background COMTRADE load completes before timeout");
    require(document.error().isEmpty(), "isolation fixture opens without error");
}

void wait_for_cursor(CursorSnapshotController& cursor, bool cursorA, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while ((cursorA ? cursor.busyA() : cursor.busyB()) && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(!(cursorA ? cursor.busyA() : cursor.busyB()), "cursor snapshot completes before timeout");
}

double wrap_degrees(double value) {
    while (value <= -180.0) value += 360.0;
    while (value > 180.0) value -= 360.0;
    return value;
}

struct ScreenVector final {
    double fraction{0.0};
    double angleDegrees{0.0};
    double x{0.0};
    double y{0.0};
};

ScreenVector screen_vector(const QVariantMap& snapshot,
                           const DocumentController& document,
                           int channelIndex,
                           double stableScale) {
    const QVariantList channels = snapshot.value(QStringLiteral("channels")).toList();
    require(channelIndex >= 0 && channelIndex < channels.size(), "snapshot contains requested C1 channel");
    const QVariantMap row = channels.at(channelIndex).toMap();
    require(row.value(QStringLiteral("valid")).toBool(), "C1 phasor row is valid");
    const double magnitude = row.value(QStringLiteral("magnitude")).toDouble();
    const double angle = wrap_degrees(row.value(QStringLiteral("angle")).toDouble() + 90.0);
    require(stableScale > 0.0, "stable record scale is positive");
    const double fraction = std::min(1.0, magnitude / stableScale);
    const double radians = angle * kPi / 180.0;
    (void)document;
    return {fraction, angle, std::cos(radians) * fraction, -std::sin(radians) * fraction};
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR)
                                              / "distance_sigra_parity.cfg";
        DocumentController document;
        document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
        wait_for_document(document);
        require(document.analogCount() >= 3, "isolation fixture contains three-phase voltage channels");

        CursorSnapshotController cursor(&document);
        constexpr double fixedC1 = 0.025;
        cursor.requestCursorA(fixedC1);
        wait_for_cursor(cursor, true);
        const QVariantMap frozenA = cursor.cursorA();
        require(frozenA.value(QStringLiteral("valid")).toBool(), "fixed C1 snapshot is valid");
        require_near(frozenA.value(QStringLiteral("time")).toDouble(), fixedC1, 1.0e-12,
                     "C1 snapshot carries the fixed cursor timestamp");

        double stableScale = 0.0;
        for (int channel = 0; channel < document.analogCount(); ++channel) {
            if (document.analogRole(channel) == QStringLiteral("Voltage"))
                stableScale = std::max(stableScale, std::abs(document.channelPeak(channel)));
        }
        stableScale *= 1.02;
        const ScreenVector frozenScreen = screen_vector(frozenA, document, 0, stableScale);

        for (double c2 : {0.030, 0.035, 0.045, 0.055, 0.065}) {
            cursor.requestCursorB(c2);
            wait_for_cursor(cursor, false);
            require(cursor.cursorB().value(QStringLiteral("valid")).toBool(), "swept C2 snapshot is valid");

            require(cursor.cursorA() == frozenA,
                    "sweeping C2 cannot mutate any numeric field in the committed C1 snapshot");
            const QVariantMap currentA = cursor.cursorA();
            const ScreenVector currentScreen = screen_vector(currentA, document, 0, stableScale);
            require_near(currentScreen.fraction, frozenScreen.fraction, 1.0e-15,
                         "C2 sweep cannot change C1 normalized radial magnitude");
            require_near(currentScreen.angleDegrees, frozenScreen.angleDegrees, 1.0e-12,
                         "C2 sweep cannot change C1 screen angle");
            require_near(currentScreen.x, frozenScreen.x, 1.0e-15,
                         "C2 sweep cannot change C1 normalized screen x transform");
            require_near(currentScreen.y, frozenScreen.y, 1.0e-15,
                         "C2 sweep cannot change C1 normalized screen y transform");
        }

        std::cout << "ardirec R4 C1/C2 isolation: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R4 C1/C2 isolation: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
