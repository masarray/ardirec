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
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR) / "distance_p1.cfg";

        DocumentController document;
        LocusSnapshotController locus(&document);

        document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
        wait_for_document(document);
        require(document.error().isEmpty(), "distance P1 fixture opens without error");
        require(document.sampleCount() == 48, "distance P1 fixture sample count");

        const int before = locus.revision();
        locus.request(document.dataStartSeconds(), document.durationSeconds(), 4000, 0.0, 0.0);
        require(locus.busy(), "production locus request starts asynchronously");
        wait_for_locus(locus, before);

        const auto snapshot = locus.nativeSnapshot();
        require(snapshot != nullptr, "production async locus publishes a native snapshot");
        require(snapshot->loops[0].size() == 48, "native L1-E locus retains each fixture timestamp below the point budget");

        // Index 22 is t=22 ms. The synthetic voltage/current pair is constructed so
        // the L1-E apparent impedance is 100+j0 ohm while the circuit is energized.
        const LocusNativePoint& energized = snapshot->loops[0].at(22);
        require(energized.valid != 0, "energized production-path locus point is valid");
        require_near(static_cast<double>(energized.r), 100.0, 0.2,
                     "production async locus recovers the 100-ohm golden resistance");
        require(std::abs(static_cast<double>(energized.x)) < 0.2,
                "production async locus golden reactance remains near zero");

        // The fixture current collapses to microamp scale after opening. The final
        // full-cycle window is therefore below the explicit measurement-current floor
        // and must be represented as a gap, not an invented impedance value.
        const LocusNativePoint& postOpen = snapshot->loops[0].back();
        require(postOpen.valid == 0,
                "near-zero post-open measuring current remains an explicit locus gap");

        // Transitional finite values are deliberately not clipped here. They remain
        // available to the operator through Fit All; R1 only changes the default view
        // transform via the explicit Fit Relevant policy.
        require(std::isfinite(locus.maxAbsR()) && std::isfinite(locus.maxAbsX()),
                "native finite locus extents remain available for forensic Fit All");

        std::cout << "ardirec locus snapshot tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec locus snapshot tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
