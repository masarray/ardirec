// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"
#include "document_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

double wrapped_angle_delta(double a, double b) {
    double delta = a - b;
    while (delta <= -180.0) delta += 360.0;
    while (delta > 180.0) delta -= 360.0;
    return delta;
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
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR) / "distance_p1.cfg";

        DocumentController document;
        document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
        require(document.loading(), "COMTRADE open is asynchronous");
        wait_for_document(document);
        require(document.error().isEmpty(), "distance P1 fixture opens without error");
        require(document.sampleCount() == 48, "distance P1 fixture sample count");
        require(document.dataStoreSnapshot() != nullptr, "lazy DAT source is retained by the document");

        AnalysisController analysis(&document);
        for (const char* loop : {"L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"}) {
            require(analysis.distanceLoopAvailable(QString::fromLatin1(loop)), "all six protection loops are available");
        }

        // A steady 50 Hz waveform must keep the same fixed-reference phasor angle when the
        // analysis cursor advances. A cursor-relative reference would rotate by about 36 degrees
        // over this 2 ms move and recreate the UI "spinning wheel" regression.
        const QVariantMap phasorAt20ms = analysis.phasorAt(0, 0.020);
        const QVariantMap phasorAt22ms = analysis.phasorAt(0, 0.022);
        require(phasorAt20ms.value(QStringLiteral("valid")).toBool(), "20 ms voltage phasor is valid");
        require(phasorAt22ms.value(QStringLiteral("valid")).toBool(), "22 ms voltage phasor is valid");
        const double angle20 = phasorAt20ms.value(QStringLiteral("angle")).toDouble();
        const double angle22 = phasorAt22ms.value(QStringLiteral("angle")).toDouble();
        require(std::abs(wrapped_angle_delta(angle22, angle20)) < 0.1,
                "phasor angle uses a fixed record reference instead of cursor-relative rotation");

        require_near(analysis.distanceCurrentFloor(), 0.001, 1.0e-12,
                     "distance current floor is 0.1 percent of the displayed record peak");

        constexpr double cursorTime = 0.022;
        const QVariantMap batched = analysis.distanceLoopsAt(cursorTime, 0.0, 0.0);
        require(batched.size() == 6, "batched cursor API returns all six protection loops");
        for (const char* loopName : {"L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"}) {
            const QString loop = QString::fromLatin1(loopName);
            const QVariantMap batchValue = batched.value(loop).toMap();
            const QVariantMap singleValue = analysis.distanceLoopAt(loop, cursorTime, 0.0, 0.0);
            require(batchValue.value(QStringLiteral("valid")).toBool()
                        == singleValue.value(QStringLiteral("valid")).toBool(),
                    "batched and single-loop cursor validity agree");
            if (singleValue.value(QStringLiteral("valid")).toBool()) {
                require_near(batchValue.value(QStringLiteral("r")).toDouble(),
                             singleValue.value(QStringLiteral("r")).toDouble(), 1.0e-12,
                             "batched and single-loop R agree");
                require_near(batchValue.value(QStringLiteral("x")).toDouble(),
                             singleValue.value(QStringLiteral("x")).toDouble(), 1.0e-12,
                             "batched and single-loop X agree");
                require_near(batchValue.value(QStringLiteral("measuringCurrent")).toDouble(),
                             singleValue.value(QStringLiteral("measuringCurrent")).toDouble(), 1.0e-12,
                             "batched and single-loop measuring current agree");
            }
        }

        const QVariantList locus = analysis.distanceLocus(QStringLiteral("L1-E"),
                                                           document.dataStartSeconds(),
                                                           document.durationSeconds(),
                                                           4000,
                                                           0.0,
                                                           0.0);
        require(locus.size() == 48, "sample-aligned locus keeps every COMTRADE timestamp when below maximumPoints");

        const QVariantMap stable = locus.at(22).toMap();
        require(stable.value(QStringLiteral("valid")).toBool(), "energized locus point is valid");
        require_near(stable.value(QStringLiteral("time")).toDouble(), 0.022, 1.0e-12,
                     "locus point time matches COMTRADE sample timestamp");
        require_near(stable.value(QStringLiteral("r")).toDouble(), 100.0, 0.2,
                     "synthetic energized loop recovers target resistance");
        require(std::abs(stable.value(QStringLiteral("x")).toDouble()) < 0.2,
                "synthetic energized loop reactance remains near zero");

        const QVariantMap postOpen = locus.back().toMap();
        require(!postOpen.value(QStringLiteral("valid")).toBool(),
                "near-zero post-open measuring current is rejected from the locus");
        require_near(postOpen.value(QStringLiteral("time")).toDouble(), document.dataEndSeconds(), 1.0e-12,
                     "invalid locus gap retains its COMTRADE timestamp");

        const QVariantList decimated = analysis.distanceLocus(QStringLiteral("L1-E"),
                                                               document.dataStartSeconds(),
                                                               document.durationSeconds(),
                                                               16,
                                                               0.0,
                                                               0.0);
        require(decimated.size() <= 16, "large locus views are decimated to the requested maximum point budget");
        require_near(decimated.front().toMap().value(QStringLiteral("time")).toDouble(),
                     document.dataStartSeconds(), 1.0e-12,
                     "decimated locus retains the first visible sample");
        require_near(decimated.back().toMap().value(QStringLiteral("time")).toDouble(),
                     document.dataEndSeconds(), 1.0e-12,
                     "decimated locus retains the last visible sample");

        std::cout << "ardirec analysis tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec analysis tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
