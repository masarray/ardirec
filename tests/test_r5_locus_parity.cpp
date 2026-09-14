// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/power/timestamped_dft.hpp"
#include "distance_zone_controller.hpp"
#include "document_controller.hpp"
#include "locus_snapshot_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QThread>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
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

void pump_events(int durationMs = 20) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < durationMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

void wait_for_document(DocumentController& document, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (document.loading() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    pump_events();
    require(!document.loading(), "R5.4 COMTRADE load completes before timeout");
    require(document.error().isEmpty(), "R5.4 COMTRADE fixture opens without error");
}

void wait_for_locus(LocusSnapshotController& locus, int previousRevision, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while ((locus.busy() || locus.revision() == previousRevision) && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    pump_events();
    require(!locus.busy(), "R5.4 Locus calculation completes before timeout");
    require(locus.revision() != previousRevision, "R5.4 Locus publishes a new revision");
}

QVariant invoke_policy(QObject* object, const char* method,
                       const QVariant& a, const QVariant& b, const QVariant& c,
                       const QVariant& d, const QVariant& e, const QVariant& f) {
    QVariant result;
    const bool ok = QMetaObject::invokeMethod(object, method,
                                               Q_RETURN_ARG(QVariant, result),
                                               Q_ARG(QVariant, a), Q_ARG(QVariant, b),
                                               Q_ARG(QVariant, c), Q_ARG(QVariant, d),
                                               Q_ARG(QVariant, e), Q_ARG(QVariant, f));
    require(ok, "R5.4 fit-policy invocation succeeds");
    return result;
}

QVariant invoke_policy(QObject* object, const char* method,
                       const QVariant& a, const QVariant& b,
                       const QVariant& c, const QVariant& d) {
    QVariant result;
    const bool ok = QMetaObject::invokeMethod(object, method,
                                               Q_RETURN_ARG(QVariant, result),
                                               Q_ARG(QVariant, a), Q_ARG(QVariant, b),
                                               Q_ARG(QVariant, c), Q_ARG(QVariant, d));
    require(ok, "R5.4 scale-policy invocation succeeds");
    return result;
}

void verify_complete_backward_cycle() {
    std::vector<double> uniform;
    for (int i = 0; i <= 60; ++i) uniform.push_back(static_cast<double>(i) / 1000.0);

    const auto partial = ardirec::power::trailing_cycle_window(uniform, 0.019, 50.0);
    require(!partial.valid(), "beginning-of-record partial cycle is invalid");

    const auto exact = ardirec::power::trailing_cycle_window(uniform, 0.020, 50.0);
    require(exact.valid(), "exactly one full backward 50-Hz cycle is valid");
    require_near(exact.start_seconds, 0.0, 1.0e-12,
                 "complete-cycle window starts exactly one period behind cursor");
    require_near(exact.duration(), 0.020, 1.0e-12,
                 "complete-cycle duration is exactly one 50-Hz period");

    std::vector<double> multiRate;
    for (int i = 0; i <= 10; ++i) multiRate.push_back(static_cast<double>(i) / 1000.0);
    for (int i = 21; i <= 80; ++i) multiRate.push_back(static_cast<double>(i) / 2000.0);
    const auto crossed = ardirec::power::trailing_cycle_window(multiRate, 0.030, 50.0);
    require(crossed.valid(), "full cycle remains valid across a sample-rate section boundary");
    require_near(crossed.start_seconds, 0.010, 1.0e-12,
                 "multi-rate full cycle preserves exact start time");
    require_near(crossed.duration(), 0.020, 1.0e-12,
                 "multi-rate full cycle preserves exact duration");

    long double weightSum = 0.0L;
    for (std::size_t sample = crossed.first; sample < crossed.end; ++sample)
        weightSum += ardirec::power::timestamp_cell_weight(multiRate, crossed, sample);
    require_near(static_cast<double>(weightSum), 0.020, 1.0e-12,
                 "timestamp quadrature weights still integrate to one full period");
}

void verify_production_locus_window_validity() {
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
    require(snapshot != nullptr, "production Locus publishes native snapshot");
    require(snapshot->loops[0].size() == 48,
            "production Locus preserves one output position per short-fixture timestamp");

    for (std::size_t index = 0; index < 20u; ++index) {
        require(snapshot->loops[0][index].valid == 0,
                "Locus rejects every point before a complete backward 50-Hz cycle exists");
    }
    require(snapshot->loops[0][20].valid != 0,
            "Locus accepts the first timestamp with one complete backward cycle");
    require(snapshot->loops[0][22].valid != 0,
            "qualified 22-ms golden point remains valid");
    require_near(static_cast<double>(snapshot->loops[0][22].r), 100.0, 0.2,
                 "full-cycle qualification does not alter qualified distance resistance");
    require(std::abs(static_cast<double>(snapshot->loops[0][22].x)) < 0.2,
            "full-cycle qualification does not alter qualified distance reactance");
}

void verify_sigra_fit_policy() {
    DistanceZoneController zones;
    const std::filesystem::path rioPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR)
                                          / "ligne_1_sigra.rio";
    require(zones.openFile(QUrl::fromLocalFile(QString::fromStdString(rioPath.string()))),
            "canonical ligne_1 SIGRA RIO opens");
    require(zones.hasZones(), "canonical ligne_1 SIGRA RIO exposes protection zones");
    const QVariantList earthZones = zones.zonesForLoop(QStringLiteral("L1-E"),
                                                        QStringLiteral("secondary"));
    require(!earthZones.isEmpty(), "canonical ligne_1 RIO exposes L1-E zones");

    QQmlEngine engine;
    QQmlComponent component(&engine,
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/LocusFitPolicy.qml")));
    require(component.status() == QQmlComponent::Ready,
            "production LocusFitPolicy QML compiles in qualification test");
    std::unique_ptr<QObject> policy(component.create());
    require(policy != nullptr, "production LocusFitPolicy instance is created");

    // Deliberately feed the policy trajectory magnitudes comparable to the remote
    // finite pole seen in ligne_1. Protection-context fit must remain zone-sized.
    const QVariantMap relevant = invoke_policy(policy.get(), "distanceTarget",
                                                earthZones, 507.0, 393.0,
                                                100.0, 61.0,
                                                QStringLiteral("relevant")).toMap();
    require_near(relevant.value(QStringLiteral("r")).toDouble(), 15.0, 1.0e-9,
                 "SIGRA protection-context relevant target is 15 ohm R seed");
    require_near(relevant.value(QStringLiteral("x")).toDouble(), 15.0, 1.0e-9,
                 "SIGRA protection-context relevant target is 15 ohm X seed");

    const QVariantMap relevantWithDifferentTrajectory = invoke_policy(
        policy.get(), "distanceTarget", earthZones, 5000.0, 4000.0,
        900.0, 800.0, QStringLiteral("relevant")).toMap();
    require(relevantWithDifferentTrajectory == relevant,
            "Fit Relevant is independent of trajectory outliers whenever protection zones exist");

    const QVariantMap fitAll = invoke_policy(policy.get(), "distanceTarget",
                                             earthZones, 507.0, 393.0,
                                             100.0, 61.0,
                                             QStringLiteral("all")).toMap();
    require(fitAll.value(QStringLiteral("r")).toDouble() > relevant.value(QStringLiteral("r")).toDouble()
            && fitAll.value(QStringLiteral("x")).toDouble() > relevant.value(QStringLiteral("x")).toDouble(),
            "Fit All still exposes the remote finite forensic trajectory");

    const double scale = invoke_policy(policy.get(), "scaleFor",
                                       1516.0, 224.0, 15.0, 15.0).toDouble();
    require_near(scale, 224.0 / 30.0, 1.0e-9,
                 "equal-ohm Locus transform is constrained by the SIGRA-like vertical range");
    const double rHalf = 1516.0 / (2.0 * scale);
    const double xHalf = 224.0 / (2.0 * scale);
    require(rHalf > 95.0 && rHalf < 110.0,
            "wide SIGRA-like panel naturally exposes about +/-100 ohm R");
    require_near(xHalf, 15.0, 1.0e-9,
                 "wide SIGRA-like panel exposes +/-15 ohm X");

    QFile locusSource(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/LocusView.qml"));
    require(locusSource.open(QIODevice::ReadOnly | QIODevice::Text),
            "production LocusView source is readable by qualification test");
    const QString qml = QString::fromUtf8(locusSource.readAll());
    require(!qml.contains(QStringLiteral("cursorExtent(")),
            "production Locus auto-fit has no cursor-derived extent path");
    require(qml.contains(QStringLiteral("fitPolicy.distanceTarget")),
            "production Locus delegates viewport selection to the qualified policy");
    require(qml.contains(QStringLiteral("Fit All")),
            "production Locus preserves explicit forensic Fit All path");
}
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        verify_complete_backward_cycle();
        verify_production_locus_window_validity();
        verify_sigra_fit_policy();
        std::cout << "ardirec R5.4 SIGRA Locus parity qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R5.4 SIGRA Locus parity qualification: FAIL: "
                  << ex.what() << '\n';
        return 1;
    }
}
