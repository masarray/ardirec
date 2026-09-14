// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"
#include "locus_snapshot_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
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
    require(!document.loading(), "golden COMTRADE load completes before timeout");
    require(document.error().isEmpty(), "golden COMTRADE fixture opens without error");
}

void wait_for_locus(LocusSnapshotController& locus, int previousRevision, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while ((locus.busy() || locus.revision() == previousRevision) && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(!locus.busy(), "golden locus calculation completes before timeout");
    require(locus.revision() != previousRevision, "golden locus publishes one new revision");
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR)
                                              / "distance_p1.cfg";
        DocumentController document;
        document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
        wait_for_document(document);

        LocusSnapshotController locus(&document);
        const int before = locus.revision();
        locus.request(document.dataStartSeconds(), document.durationSeconds(), 4000, 0.0, 0.0);
        wait_for_locus(locus, before);
        const auto snapshot = locus.nativeSnapshot();
        require(snapshot != nullptr, "R4 golden locus owns a native snapshot");
        require(snapshot->loops[0].size() == 48, "R4 golden L1-E locus keeps every fixture timestamp");

        const LocusNativePoint& golden = snapshot->loops[0].at(22);
        require(golden.valid != 0, "known energized cursor is measurement-valid");
        require_near(static_cast<double>(golden.r), 100.0, 0.2,
                     "known energized cursor preserves 100-ohm golden resistance");
        require(std::abs(static_cast<double>(golden.x)) < 0.2,
                "known energized cursor preserves near-zero golden reactance");

        const LocusNativePoint& lowCurrentTail = snapshot->loops[0].back();
        require(lowCurrentTail.valid == 0,
                "near-zero-current tail remains an explicit invalid/gap point");
        require(snapshot->earthRelevantMaxAbsR > 90.0 && snapshot->earthRelevantMaxAbsR < 150.0,
                "default earth relevant-fit resistance stays around the protection-valid 100-ohm locus");
        require(snapshot->earthRelevantMaxAbsX < 5.0,
                "default earth relevant-fit reactance is not dominated by invalid low-current samples");
        require(std::isfinite(snapshot->maxAbsR) && std::isfinite(snapshot->maxAbsX),
                "forensic Fit All extents remain finite and independently available");

        std::cout << "ardirec R4 locus golden/default-fit qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R4 locus golden/default-fit qualification: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
