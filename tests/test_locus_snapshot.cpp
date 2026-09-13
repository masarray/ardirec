// SPDX-License-Identifier: GPL-3.0-or-later
#include "cursor_snapshot_controller.hpp"
#include "document_controller.hpp"
#include "locus_snapshot_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QString>
#include <QUrl>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

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

void wait_for_cursor(CursorSnapshotController& cursor, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (cursor.busyA() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(!cursor.busyA(), "async cursor snapshot completes before timeout");
    require(cursor.cursorA().value(QStringLiteral("valid")).toBool(),
            "async cursor publishes a valid snapshot");
}

void load_and_wait(DocumentController& document, const std::filesystem::path& cfgPath) {
    document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
    wait_for_document(document);
    require(document.error().isEmpty(), "COMTRADE fixture opens without error");
}

struct TemporaryFixture final {
    std::filesystem::path directory;
    std::filesystem::path cfg;

    TemporaryFixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory = std::filesystem::temp_directory_path()
                    / ("ardirec_multirate_" + std::to_string(stamp));
        std::filesystem::create_directories(directory);
        cfg = directory / "distance_multirate.cfg";
        const auto dat = directory / "distance_multirate.dat";

        {
            std::ofstream out(cfg);
            require(static_cast<bool>(out), "create temporary multi-rate CFG");
            out << "ARDIREC MULTIRATE DFT,REFERENCE RECORDER,1999\n"
                << "6,6A,0D\n"
                << "1,V1,A,LINE,V,1,0,0,-1000,1000,100,100,S\n"
                << "2,V2,B,LINE,V,1,0,0,-1000,1000,100,100,S\n"
                << "3,V3,C,LINE,V,1,0,0,-1000,1000,100,100,S\n"
                << "4,I1,A,LINE,A,1,0,0,-1000,1000,1,1,S\n"
                << "5,I2,B,LINE,A,1,0,0,-1000,1000,1,1,S\n"
                << "6,I3,C,LINE,A,1,0,0,-1000,1000,1,1,S\n"
                << "50\n"
                << "2\n"
                << "1000,91\n"
                << "2000,271\n"
                << "01/01/2026,00:00:00.000000\n"
                << "01/01/2026,00:00:00.120000\n"
                << "ASCII\n"
                << "1\n";
        }

        constexpr double actualFrequency = 50.4;
        constexpr double voltageRms = 100.0;
        constexpr double currentRms = 1.0;
        const double sqrt2 = std::sqrt(2.0);
        const double phases[3]{0.0, -2.0 * kPi / 3.0, 2.0 * kPi / 3.0};

        std::ofstream out(dat);
        require(static_cast<bool>(out), "create temporary multi-rate DAT");
        out.setf(std::ios::fixed);
        out.precision(9);
        int sample = 1;
        auto emit = [&](double t) {
            out << sample++ << ',' << static_cast<long long>(std::llround(t * 1.0e6));
            for (const double phase : phases) {
                const double value = voltageRms * sqrt2 * std::cos(2.0 * kPi * actualFrequency * t + phase)
                                     + 25.0 * std::cos(2.0 * kPi * 3.0 * actualFrequency * t
                                                       + 3.0 * phase + 20.0 * kPi / 180.0);
                out << ',' << value;
            }
            for (const double phase : phases) {
                const double value = currentRms * sqrt2 * std::cos(2.0 * kPi * actualFrequency * t + phase)
                                     + 0.20 * std::cos(2.0 * kPi * 3.0 * actualFrequency * t
                                                       + 3.0 * phase - 35.0 * kPi / 180.0)
                                     + 0.05;
                out << ',' << value;
            }
            out << '\n';
        };

        for (int index = 0; index <= 90; ++index) emit(static_cast<double>(index) * 0.001);
        for (int index = 1; index <= 180; ++index) emit(0.090 + static_cast<double>(index) * 0.0005);
        require(sample == 272, "temporary multi-rate fixture contains 271 samples");
    }

    ~TemporaryFixture() {
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
    }
};
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        DocumentController document;
        CursorSnapshotController cursor(&document);
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

        // SIGRA classical-method parity fixture keeps R1.1 semantics locked.
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
        locus.request(document.dataStartSeconds(), document.durationSeconds(), 4000, 0.0, 0.0);
        wait_for_locus(locus, before);
        snapshot = locus.nativeSnapshot();
        require(snapshot != nullptr, "SIGRA parity locus publishes a native snapshot");
        require(snapshot->loops[0].size() == 80,
                "SIGRA parity fixture remains unsimplified below the render budget");

        const LocusNativePoint& beforeTrip = snapshot->loops[0].at(25);
        require(beforeTrip.valid != 0, "pre-trip one-cycle window remains valid");
        require_near(static_cast<double>(beforeTrip.r), 100.0, 0.25,
                     "classical RE/RL-XE/XL plus measured IE recovers the golden resistance");
        require(std::abs(static_cast<double>(beforeTrip.x)) < 0.25,
                "classical parity point keeps expected reactance");

        const LocusNativePoint& spanningTrip = snapshot->loops[0].at(40);
        require(spanningTrip.valid == 0,
                "status change inside the SIGRA measuring window creates a locus gap");
        require(snapshot->statusRejectedCount > 0,
                "native snapshot records status-window validity rejections");

        const LocusNativePoint& afterTrip = snapshot->loops[0].at(55);
        require(afterTrip.valid != 0, "post-trip steady one-cycle window becomes valid again");
        require_near(static_cast<double>(afterTrip.r), 100.0, 0.25,
                     "post-trip classical impedance returns to the golden value");
        require(std::abs(static_cast<double>(afterTrip.x)) < 0.25,
                "post-trip classical reactance returns to the golden value");

        // R1.2 deterministic COMTRADE multi-rate fixture. The 100 ms one-cycle
        // measurement window crosses the 1000 -> 2000 sample/s boundary at 90 ms.
        // The underlying system is 50.4 Hz while COMTRADE nominal remains 50 Hz.
        // Third-harmonic/DC contamination makes count-normalized DFT measurably wrong;
        // timestamp-cell integration must preserve the 100+j0 ohm fundamental ratio.
        TemporaryFixture multiRate;
        load_and_wait(document, multiRate.cfg);
        require(document.sampleCount() == 271, "multi-rate fixture sample count");
        require_near(document.nominalFrequency(), 50.0, 1.0e-12,
                     "multi-rate fixture retains COMTRADE nominal frequency");
        require_near(document.calculationFrequency(), 50.4, 0.03,
                     "bounded prefault V1 estimator recovers calculation frequency");
        require(document.calculationFrequencyProvenance().startsWith(QStringLiteral("PREFault estimated")),
                "calculation-frequency provenance identifies the prefault estimator");

        cursor.requestCursorA(0.100);
        wait_for_cursor(cursor);
        const QVariantMap cursorSnapshot = cursor.cursorA();
        require_near(cursorSnapshot.value(QStringLiteral("calculationFrequency")).toDouble(), 50.4, 0.03,
                     "cursor snapshot consumes the shared calculation frequency");
        const QVariantMap cursorLoops = cursor.distanceLoopsForSnapshot(cursorSnapshot, 0.0, 0.0);
        const QVariantMap cursorL1E = cursorLoops.value(QStringLiteral("L1-E")).toMap();
        require(cursorL1E.value(QStringLiteral("valid")).toBool(),
                "multi-rate cursor L1-E remains valid across rate boundary");
        require_near(cursorL1E.value(QStringLiteral("r")).toDouble(), 100.0, 0.35,
                     "timestamp-weighted cursor DFT preserves 100-ohm resistance across rate boundary");
        require(std::abs(cursorL1E.value(QStringLiteral("x")).toDouble()) < 0.20,
                "timestamp-weighted cursor DFT keeps reactance near zero across rate boundary");

        before = locus.revision();
        locus.request(document.dataStartSeconds(), document.durationSeconds(), 4000, 0.0, 0.0);
        wait_for_locus(locus, before);
        snapshot = locus.nativeSnapshot();
        require(snapshot != nullptr && snapshot->loops[0].size() == 271,
                "multi-rate production locus remains unsimplified below render budget");
        const LocusNativePoint& crossingRateBoundary = snapshot->loops[0].at(110); // exactly 100 ms
        require(crossingRateBoundary.valid != 0,
                "multi-rate locus point is valid when its one-cycle window crosses the rate boundary");
        require_near(static_cast<double>(crossingRateBoundary.r), 100.0, 0.35,
                     "timestamp-weighted locus DFT preserves 100-ohm resistance across rate boundary");
        require(std::abs(static_cast<double>(crossingRateBoundary.x)) < 0.20,
                "timestamp-weighted locus DFT keeps reactance near zero across rate boundary");

        std::cout << "ardirec locus snapshot tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec locus snapshot tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
