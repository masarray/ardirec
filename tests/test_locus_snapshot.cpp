// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"
#include "locus_snapshot_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QString>
#include <QUrl>

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

void wait_for_locus(LocusSnapshotController& locus, int previousRevision, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while ((locus.busy() || locus.revision() == previousRevision) && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(!locus.busy(), "async locus calculation completes before timeout");
    require(locus.revision() != previousRevision, "async locus publishes a new revision");
}

void load_and_wait(DocumentController& document, const std::filesystem::path& cfgPath) {
    document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
    wait_for_document(document);
    require(document.error().isEmpty(), "COMTRADE fixture opens without error");
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        DocumentController document;
        LocusSnapshotController locus(&document);

        // Existing production-path golden: energized 100+j0 ohm followed by a
        // near-zero-current opening. The opening must remain a gap, not an outlier.
        const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR) / "distance_p1.cfg";
        load_and_wait(document, cfgPath);
        require(document.sampleCount() == 48, "distance P1 fixture sample count");

        int before = locus.revision();
        locus.request(document.dataStartSeconds(), document.durationSeconds(), 4000, 0.0, 0.0);
        require(locus.busy(), "production locus request starts asynchronously");
        wait_for_locus(locus, before);

        auto snapshot = locus.nativeSnapshot();
        require(snapshot != nullptr, "production async locus publishes a native snapshot");
        require(snapshot->loops[0].size() == 48, "native L1-E locus retains each fixture timestamp below the point budget");

        const LocusNativePoint& energized = snapshot->loops[0].at(22);
        require(energized.valid != 0, "energized production-path locus point is valid");
        require_near(static_cast<double>(energized.r), 100.0, 0.2,
                     "production async locus recovers the 100-ohm golden resistance");
        require(std::abs(static_cast<double>(energized.x)) < 0.2,
                "production async locus golden reactance remains near zero");

        const LocusNativePoint& postOpen = snapshot->loops[0].back();
        require(postOpen.valid == 0,
                "near-zero post-open measuring current remains an explicit locus gap");
        require(std::isfinite(locus.maxAbsR()) && std::isfinite(locus.maxAbsX()),
                "native finite locus extents remain available for forensic Fit All");

        // SIGRA classical-method parity fixture:
        // - analog names deliberately do not encode L1/L2/L3, so explicit COMTRADE
        //   phase metadata must drive the production worker;
        // - a dedicated IE channel follows SIGRA's IE = -(IL1+IL2+IL3) convention;
        // - the sidecar RIO supplies RE/RL=1.0 and XE/XL=0.6 with no line angle,
        //   proving the ratios must remain independent rather than being collapsed
        //   into a synthetic complex kL;
        // - TRIP changes at 30 ms. SIGRA's trailing one-cycle calculated values are
        //   invalid while the 20 ms measuring window contains that status transition.
        const std::filesystem::path sigraPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR) / "distance_sigra_parity.cfg";
        load_and_wait(document, sigraPath);
        require(document.sampleCount() == 80, "SIGRA parity fixture sample count");
        require(document.channelPhase(0) == QStringLiteral("L1")
                    && document.channelPhase(1) == QStringLiteral("L2")
                    && document.channelPhase(2) == QStringLiteral("L3"),
                "explicit COMTRADE phase metadata is classified before name heuristics");
        require(document.digitalEdgeTimes().size() == 1,
                "SIGRA parity fixture exposes the TRIP status transition");
        require(locus.classicalGroundingValid(), "matching RIO sidecar selects classical earth compensation");
        require_near(locus.reOverRl(), 1.0, 1.0e-12, "RIO RE/RL is preserved directly");
        require_near(locus.xeOverXl(), 0.6, 1.0e-12, "RIO XE/XL is preserved directly");

        before = locus.revision();
        // kL arguments are intentionally zero. The earth-loop golden must come from
        // the sidecar's classical RE/RL-XE/XL model, not a line-angle conversion.
        locus.request(document.dataStartSeconds(), document.durationSeconds(), 4000, 0.0, 0.0);
        wait_for_locus(locus, before);
        snapshot = locus.nativeSnapshot();
        require(snapshot != nullptr, "SIGRA parity locus publishes a native snapshot");
        require(snapshot->loops[0].size() == 80,
                "SIGRA parity fixture remains unsimplified below the render budget");

        const LocusNativePoint& beforeTrip = snapshot->loops[0].at(25); // 25 ms, window 5..25 ms
        require(beforeTrip.valid != 0, "pre-trip one-cycle window remains valid");
        require_near(static_cast<double>(beforeTrip.r), 100.0, 0.25,
                     "classical RE/RL-XE/XL plus measured IE recovers the golden resistance");
        require(std::abs(static_cast<double>(beforeTrip.x)) < 0.25,
                "classical parity point keeps expected reactance");

        const LocusNativePoint& spanningTrip = snapshot->loops[0].at(40); // 40 ms, window spans 30 ms edge
        require(spanningTrip.valid == 0,
                "status change inside the SIGRA measuring window creates a locus gap");
        require(snapshot->statusRejectedCount > 0,
                "native snapshot records status-window validity rejections");

        const LocusNativePoint& afterTrip = snapshot->loops[0].at(55); // 55 ms, window is fully post-trip
        require(afterTrip.valid != 0, "post-trip steady one-cycle window becomes valid again");
        require_near(static_cast<double>(afterTrip.r), 100.0, 0.25,
                     "post-trip classical impedance returns to the golden value");
        require(std::abs(static_cast<double>(afterTrip.x)) < 0.25,
                "post-trip classical reactance returns to the golden value");

        std::cout << "ardirec locus snapshot tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec locus snapshot tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
