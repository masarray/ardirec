// SPDX-License-Identifier: GPL-3.0-or-later
#include "cursor_snapshot_controller.hpp"
#include "document_controller.hpp"
#include "phasor_vector_item.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void pump_events(int durationMs = 25) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < durationMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

bool wait_until(const std::function<bool()>& predicate, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return predicate();
}

void wait_for_document(DocumentController& document, int timeoutMs = 10000) {
    require(wait_until([&document]() { return !document.loading(); }, timeoutMs),
            "performance fixture load completes before timeout");
    require(document.error().isEmpty(), "performance fixture opens without error");
}

QString component_errors(const QQmlComponent& component) {
    QString result;
    for (const auto& error : component.errors()) {
        if (!result.isEmpty()) result += QLatin1Char('\n');
        result += error.toString();
    }
    return result;
}

QVariantMap make_snapshot(double timeSeconds, int channelCount, double magnitudeBase = 80.0) {
    QVariantList channels;
    channels.reserve(channelCount);
    static const char* phases[] = {"L1", "L2", "L3", "E"};
    for (int index = 0; index < channelCount; ++index) {
        const double angle = static_cast<double>((index % 4) * -90);
        channels.push_back(QVariantMap{
            {QStringLiteral("valid"), true},
            {QStringLiteral("index"), index},
            {QStringLiteral("name"), QStringLiteral("A%1").arg(index + 1)},
            {QStringLiteral("unit"), index < 20 ? QStringLiteral("V") : QStringLiteral("A")},
            {QStringLiteral("phase"), QString::fromLatin1(phases[index % 4])},
            {QStringLiteral("magnitude"), magnitudeBase + static_cast<double>(index % 7)},
            {QStringLiteral("angle"), angle},
            {QStringLiteral("real"), magnitudeBase},
            {QStringLiteral("imag"), 0.0},
        });
    }
    return QVariantMap{
        {QStringLiteral("valid"), true},
        {QStringLiteral("time"), timeSeconds},
        {QStringLiteral("channels"), channels},
        {QStringLiteral("voltageSequence"), QVariantMap{{QStringLiteral("valid"), false}}},
        {QStringLiteral("currentSequence"), QVariantMap{{QStringLiteral("valid"), false}}},
    };
}

class SnapshotProbe final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap cursorA READ cursorA NOTIFY cursorAChanged)
    Q_PROPERTY(QVariantMap cursorB READ cursorB NOTIFY cursorBChanged)
    Q_PROPERTY(bool busyA READ busyA NOTIFY busyAChanged)
    Q_PROPERTY(bool busyB READ busyB NOTIFY busyBChanged)

public:
    QVariantMap cursorA() const { return m_cursorA; }
    QVariantMap cursorB() const { return m_cursorB; }
    bool busyA() const { return m_busyA; }
    bool busyB() const { return m_busyB; }
    int requestsA() const { return m_requestsA; }

    void seedA(const QVariantMap& snapshot) { m_cursorA = snapshot; }
    void seedB(const QVariantMap& snapshot) { m_cursorB = snapshot; }

    Q_INVOKABLE void requestCursorA(double) {
        ++m_requestsA;
        if (!m_busyA) {
            m_busyA = true;
            emit busyAChanged();
        }
    }

    Q_INVOKABLE void requestCursorB(double) {
        if (!m_busyB) {
            m_busyB = true;
            emit busyBChanged();
        }
    }

    void commitA(const QVariantMap& snapshot) {
        m_cursorA = snapshot;
        if (m_busyA) {
            m_busyA = false;
            emit busyAChanged();
        }
        emit cursorAChanged();
    }

signals:
    void cursorAChanged();
    void cursorBChanged();
    void busyAChanged();
    void busyBChanged();

private:
    QVariantMap m_cursorA;
    QVariantMap m_cursorB;
    bool m_busyA{false};
    bool m_busyB{false};
    int m_requestsA{0};
};

class PerfDocument final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double triggerOffsetSeconds READ triggerOffsetSeconds CONSTANT)
    Q_PROPERTY(QString valueRepresentation READ valueRepresentation CONSTANT)

public:
    using QObject::QObject;
    double triggerOffsetSeconds() const { return 0.0; }
    QString valueRepresentation() const { return QStringLiteral("secondary"); }

    Q_INVOKABLE QString analogRole(int index) const {
        if (index < 10 || (index >= 20 && index < 30)) return QStringLiteral("Voltage");
        if ((index >= 10 && index < 20) || (index >= 30 && index < 40)) return QStringLiteral("Current");
        return QStringLiteral("Other");
    }

    Q_INVOKABLE double channelPeak(int index) const {
        return 100.0 + static_cast<double>(index % 9);
    }

    Q_INVOKABLE QString channelUnit(int index) const {
        return analogRole(index) == QStringLiteral("Voltage") ? QStringLiteral("V") : QStringLiteral("A");
    }

    Q_INVOKABLE QString channelName(int index) const {
        return QStringLiteral("A%1").arg(index + 1);
    }
};

class PerfAnalysis final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    Q_INVOKABLE int phaseChannel(const QString&, const QString&) const { return -1; }

    Q_INVOKABLE QString channelPhase(int index) const {
        switch (index % 4) {
        case 0: return QStringLiteral("L1");
        case 1: return QStringLiteral("L2");
        case 2: return QStringLiteral("L3");
        default: return QStringLiteral("E");
        }
    }

    Q_INVOKABLE QString phaseColor(int index) const {
        switch (index % 4) {
        case 0: return QStringLiteral("#244f9e");
        case 1: return QStringLiteral("#b77900");
        case 2: return QStringLiteral("#2c7b69");
        default: return QStringLiteral("#6f7780");
        }
    }
};

void collect_vector_items(QQuickItem* item, std::vector<PhasorVectorItem*>& output) {
    if (!item) return;
    if (auto* vectorItem = qobject_cast<PhasorVectorItem*>(item)) output.push_back(vectorItem);
    for (QQuickItem* child : item->childItems()) collect_vector_items(child, output);
}

std::vector<PhasorVectorItem*> vector_items(QObject* root) {
    std::vector<PhasorVectorItem*> result;
    collect_vector_items(qobject_cast<QQuickItem*>(root), result);
    return result;
}

QVariantList indices(int first, int count) {
    QVariantList result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) result.push_back(first + i);
    return result;
}

void verify_qml_first_open_and_scrub_budget() {
    qmlRegisterType<PhasorVectorItem>("Ardirec.Render", 1, 0, "PhasorVectorItem");

    constexpr int channelCount = 50;
    SnapshotProbe probe;
    probe.seedA(make_snapshot(0.020, channelCount));
    probe.seedB(make_snapshot(0.030, channelCount, 70.0));
    PerfDocument document;
    PerfAnalysis analysis;

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("cursorSnapshotController"), &probe);
    QQmlComponent component(
        &engine,
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/PhasorView.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());

    QVariantMap initial{
        {QStringLiteral("width"), 1000.0},
        {QStringLiteral("height"), 520.0},
        {QStringLiteral("document"), QVariant::fromValue(static_cast<QObject*>(&document))},
        {QStringLiteral("analysis"), QVariant::fromValue(static_cast<QObject*>(&analysis))},
        {QStringLiteral("requestOwner"), true},
        {QStringLiteral("visible"), true},
        {QStringLiteral("cursorATime"), 0.020},
        {QStringLiteral("cursorBTime"), 0.030},
        {QStringLiteral("voltageChannels"), indices(0, 10)},
        {QStringLiteral("currentChannels"), indices(10, 10)},
        {QStringLiteral("residualChannels"), indices(20, 30)},
    };

    const auto openStart = Clock::now();
    std::unique_ptr<QObject> view(component.createWithInitialProperties(initial, engine.rootContext()));
    const double synchronousOpenMs = elapsed_ms(openStart);
    require(view != nullptr, "PhasorView performance component is created");
    require(synchronousOpenMs < 500.0, "first-open latency budget keeps synchronous Phasor construction bounded");

    require(wait_until([&view]() {
        return vector_items(view.get()).size() >= 2u;
    }, 1500), "first-open latency budget reaches a visible vector group promptly");

    const auto initialItems = vector_items(view.get());
    require(initialItems.size() >= 2u && initialItems.size() <= 4u,
            "first-open latency budget virtualizes off-screen Phasor group bodies");

    const int baselineRequests = probe.requestsA();
    view->setProperty("cursorATime", 0.021);
    pump_events(5);
    require(probe.requestsA() == baselineRequests + 1,
            "scrub begins one bounded C1 calculation");
    require(probe.busyA(), "scrub probe is busy before coalescing");

    const auto scrubStart = Clock::now();
    constexpr int scrubUpdates = 250;
    double finalTime = 0.021;
    for (int i = 0; i < scrubUpdates; ++i) {
        finalTime = 0.021 + static_cast<double>(i + 1) * 0.00001;
        view->setProperty("cursorATime", finalTime);
    }
    const double scrubDispatchMs = elapsed_ms(scrubStart);
    require(scrubDispatchMs < 250.0, "cursor scrub latency budget keeps pointer-rate dispatch bounded");
    require(probe.requestsA() == baselineRequests + 1,
            "cursor scrub latency budget coalesces while one worker is in flight");

    const auto committedBefore = vector_items(view.get());
    require(!committedBefore.empty(), "visible vector geometry exists before final scrub commit");
    for (auto* item : committedBefore)
        require(item != nullptr && !item->vectors().isEmpty(),
                "committed-frame continuity remains valid while scrub work is pending");

    probe.commitA(make_snapshot(finalTime, channelCount, 84.0));
    require(wait_until([&view, finalTime]() {
        const QVariantMap displayed = view->property("displaySnapshotA").toMap();
        return displayed.value(QStringLiteral("valid")).toBool()
            && std::abs(displayed.value(QStringLiteral("time")).toDouble() - finalTime) <= 1.0e-10;
    }, 1000), "latest scrub target commits within the cursor scrub latency budget");

    const auto committedAfter = vector_items(view.get());
    for (auto* item : committedAfter)
        require(item != nullptr && !item->vectors().isEmpty(),
                "committed-frame continuity remains valid after latest scrub commit");

    std::cout << "R6.4 QML first-open synchronous: " << synchronousOpenMs
              << " ms; " << scrubUpdates << " scrub dispatches: "
              << scrubDispatchMs << " ms\n";
}

void verify_fundamental_worker_latency_and_dedup() {
    const std::filesystem::path cfgPath =
        std::filesystem::path(ARDIREC_TEST_DATA_DIR) / "distance_sigra_parity.cfg";

    DocumentController document;
    CursorSnapshotController snapshots(&document);
    document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
    wait_for_document(document);

    const auto firstStart = Clock::now();
    snapshots.requestCursorA(0.040);
    require(wait_until([&snapshots]() {
        return !snapshots.busyA() && snapshots.cursorA().value(QStringLiteral("valid")).toBool();
    }, 2000), "first-open latency budget produces the immutable fundamental frame");
    const double firstOpenMs = elapsed_ms(firstStart);
    require(firstOpenMs < 500.0, "first-open latency budget bounds real fixture snapshot completion");

    const quint64 committedLaunches = snapshots.launchedJobsA();
    snapshots.requestCursorA(0.040);
    snapshots.requestCursorA(0.040);
    pump_events(15);
    require(snapshots.launchedJobsA() == committedLaunches,
            "committed identical Phasor requests reuse the existing immutable frame");

    const quint64 beforeInFlightB = snapshots.launchedJobsB();
    snapshots.requestCursorB(0.045);
    snapshots.requestCursorB(0.045);
    snapshots.requestCursorB(0.045);
    require(snapshots.launchedJobsB() == beforeInFlightB + 1,
            "first-open duplicate consumers do not restart an identical in-flight DFT");
    require(wait_until([&snapshots]() { return !snapshots.busyB(); }, 2000),
            "deduplicated C2 fundamental worker completes");

    double maximumScrubMs = 0.0;
    for (int i = 0; i < 8; ++i) {
        const double time = 0.025 + static_cast<double>(i) * 0.004;
        const auto start = Clock::now();
        snapshots.requestCursorA(time);
        require(wait_until([&snapshots, time]() {
            const QVariantMap snapshot = snapshots.cursorA();
            return !snapshots.busyA()
                && snapshot.value(QStringLiteral("valid")).toBool()
                && std::abs(snapshot.value(QStringLiteral("time")).toDouble() - time) <= 1.0e-10;
        }, 2000), "cursor scrub latency budget completes a real fixture snapshot");
        maximumScrubMs = std::max(maximumScrubMs, elapsed_ms(start));
    }
    require(maximumScrubMs < 500.0, "cursor scrub latency budget bounds real fixture worker completion");

    const quint64 beforeRepresentationA = snapshots.launchedJobsA();
    document.setValueRepresentation(QStringLiteral("primary"));
    require(wait_until([&snapshots]() { return !snapshots.busyA() && !snapshots.busyB(); }, 2000),
            "representation change revalidates the immutable snapshot");
    require(snapshots.launchedJobsA() > beforeRepresentationA,
            "snapshot dedup never suppresses a required source-revision recalculation");

    std::cout << "R6.4 fundamental first-open: " << firstOpenMs
              << " ms; max sequential scrub: " << maximumScrubMs << " ms\n";
}
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        verify_qml_first_open_and_scrub_budget();
        verify_fundamental_worker_latency_and_dedup();
        std::cout << "ArDiRec R6.4 Phasor performance qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ArDiRec R6.4 Phasor performance qualification: FAIL: "
                  << ex.what() << '\n';
        return 1;
    }
}

#include "test_r6_phasor_performance.moc"
