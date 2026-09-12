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

void verify_balanced_snapshot(const QVariantMap& snapshot,
                              const QString& expectedPositiveName,
                              double expectedMagnitude,
                              double tolerance) {
    require(snapshot.value(QStringLiteral("valid")).toBool(), "balanced sequence snapshot is valid");
    require(snapshot.value(QStringLiteral("provenance")).toString() == QStringLiteral("DERIVED"),
            "sequence snapshot carries DERIVED provenance");

    const QVariantMap positive = snapshot.value(QStringLiteral("positive")).toMap();
    const QVariantMap negative = snapshot.value(QStringLiteral("negative")).toMap();
    const QVariantMap zero = snapshot.value(QStringLiteral("zero")).toMap();
    require(positive.value(QStringLiteral("name")).toString() == expectedPositiveName,
            "positive sequence uses the expected engineering name");
    require(positive.value(QStringLiteral("provenance")).toString() == QStringLiteral("DERIVED"),
            "derived sequence row carries provenance");
    require_near(positive.value(QStringLiteral("magnitude")).toDouble(), expectedMagnitude, tolerance,
                 "balanced fixture recovers positive-sequence magnitude");
    require(std::abs(negative.value(QStringLiteral("magnitude")).toDouble()) < tolerance,
            "balanced fixture has negligible negative sequence");
    require(std::abs(zero.value(QStringLiteral("magnitude")).toDouble()) < tolerance,
            "balanced fixture has negligible zero sequence");
    require(std::abs(snapshot.value(QStringLiteral("negativePercent")).toDouble()) < tolerance,
            "balanced fixture has negligible negative-sequence ratio");
    require(std::abs(snapshot.value(QStringLiteral("zeroPercent")).toDouble()) < tolerance,
            "balanced fixture has negligible zero-sequence ratio");
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

        AnalysisController analysis(&document);
        constexpr double cursorTime = 0.022;

        const QVariantMap voltage = analysis.sequenceComponentsAt(QStringLiteral("Voltage"), cursorTime);
        verify_balanced_snapshot(voltage, QStringLiteral("V1"), 50.0, 2.0e-4);
        require(voltage.value(QStringLiteral("unit")).toString() == QStringLiteral("V"),
                "voltage sequence preserves engineering unit");
        require(voltage.value(QStringLiteral("sourceL1")).toInt() == 0
                    && voltage.value(QStringLiteral("sourceL2")).toInt() == 1
                    && voltage.value(QStringLiteral("sourceL3")).toInt() == 2,
                "voltage sequence reports its three recorded source channels");

        const QVariantMap current = analysis.sequenceComponentsAt(QStringLiteral("Current"), cursorTime);
        verify_balanced_snapshot(current, QStringLiteral("I1"), 0.5, 2.0e-5);
        require(current.value(QStringLiteral("unit")).toString() == QStringLiteral("A"),
                "current sequence preserves engineering unit");

        const QVariantMap unsupported = analysis.sequenceComponentsAt(QStringLiteral("Other"), cursorTime);
        require(!unsupported.value(QStringLiteral("valid")).toBool(),
                "unsupported roles do not fabricate sequence values");
        require(unsupported.value(QStringLiteral("provenance")).toString() == QStringLiteral("DERIVED"),
                "invalid derived snapshot still declares provenance");

        const QVariantMap incompleteWindow = analysis.sequenceComponentsAt(QStringLiteral("Voltage"),
                                                                            document.dataStartSeconds());
        require(!incompleteWindow.value(QStringLiteral("valid")).toBool(),
                "sequence snapshot requires a usable cursor analysis window");

        std::cout << "ardirec sequence analysis tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec sequence analysis tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
