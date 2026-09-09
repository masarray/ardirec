// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"
#include "document_controller.hpp"

#include <QCoreApplication>
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
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR) / "distance_p1.cfg";

        DocumentController document;
        document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
        require(document.error().isEmpty(), "distance P1 fixture opens without error");
        require(document.sampleCount() == 48, "distance P1 fixture sample count");

        AnalysisController analysis(&document);
        for (const char* loop : {"L1-E", "L2-E", "L3-E", "L1-L2", "L2-L3", "L3-L1"}) {
            require(analysis.distanceLoopAvailable(QString::fromLatin1(loop)), "all six protection loops are available");
        }

        require_near(analysis.distanceCurrentFloor(), 0.001, 1.0e-12,
                     "distance current floor is 0.1 percent of the displayed record peak");

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
