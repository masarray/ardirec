// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"
#include "locus_snapshot_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

double percentile99(std::vector<double> values) {
    if (values.empty()) return 0.0;
    const std::size_t index = static_cast<std::size_t>(
        std::floor(0.99 * static_cast<double>(values.size() - 1u)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
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

        // The default 'relevant' fit is intentionally a robust statistic over
        // measurement-valid earth-loop points, not an absolute 100-ohm fixture
        // clamp. During the physical current-collapse transition, still-valid
        // one-cycle windows can legitimately move above 100 ohm; the invalid
        // near-zero-current tail must not enter the relevant-fit population.
        std::vector<double> validEarthR;
        std::vector<double> validEarthX;
        for (std::size_t loop = 0; loop < 3u; ++loop) {
            for (const LocusNativePoint& point : snapshot->loops[loop]) {
                if (!point.valid) continue;
                validEarthR.push_back(std::abs(static_cast<double>(point.r)));
                validEarthX.push_back(std::abs(static_cast<double>(point.x)));
            }
        }
        require(!validEarthR.empty() && validEarthR.size() == validEarthX.size(),
                "golden fixture exposes valid earth-loop trajectory samples");
        require_near(snapshot->earthRelevantMaxAbsR, percentile99(validEarthR), 1.0e-4,
                     "default earth relevant-fit resistance is p99 of valid trajectory points only");
        require_near(snapshot->earthRelevantMaxAbsX, percentile99(validEarthX), 1.0e-4,
                     "default earth relevant-fit reactance is p99 of valid trajectory points only");
        require(snapshot->earthRelevantMaxAbsR <= snapshot->earthMaxAbsR + 1.0e-9
                    && snapshot->earthRelevantMaxAbsX <= snapshot->earthMaxAbsX + 1.0e-9,
                "relevant fit never exceeds forensic Fit All extents");
        require(std::isfinite(snapshot->maxAbsR) && std::isfinite(snapshot->maxAbsX),
                "forensic Fit All extents remain finite and independently available");

        std::cout << "ardirec R4 locus golden/default-fit qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R4 locus golden/default-fit qualification: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
