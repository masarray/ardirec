// SPDX-License-Identifier: GPL-3.0-or-later
#include "phasor_vector_item.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSGNode>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

#include <cmath>
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

void pump_events(int durationMs = 40) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < durationMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

QString component_errors(const QQmlComponent& component) {
    QString result;
    for (const auto& error : component.errors()) {
        if (!result.isEmpty()) result += QLatin1Char('\n');
        result += error.toString();
    }
    return result;
}

QVariantMap make_snapshot(double timeSeconds, double magnitude, double angleDegrees) {
    QVariantList channels;
    channels.push_back(QVariantMap{
        {QStringLiteral("valid"), true},
        {QStringLiteral("index"), 0},
        {QStringLiteral("name"), QStringLiteral("UL1")},
        {QStringLiteral("unit"), QStringLiteral("V")},
        {QStringLiteral("phase"), QStringLiteral("L1")},
        {QStringLiteral("magnitude"), magnitude},
        {QStringLiteral("angle"), angleDegrees},
        {QStringLiteral("real"), magnitude},
        {QStringLiteral("imag"), 0.0},
    });
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
    int requestsB() const { return m_requestsB; }

    void seedA(const QVariantMap& value) { m_cursorA = value; }
    void seedB(const QVariantMap& value) { m_cursorB = value; }

    Q_INVOKABLE void requestCursorA(double) {
        ++m_requestsA;
        setBusyA(true);
    }
    Q_INVOKABLE void requestCursorB(double) {
        ++m_requestsB;
        setBusyB(true);
    }

    void commitA(const QVariantMap& value) {
        m_cursorA = value;
        setBusyA(false);
        emit cursorAChanged();
    }
    void commitB(const QVariantMap& value) {
        m_cursorB = value;
        setBusyB(false);
        emit cursorBChanged();
    }

signals:
    void cursorAChanged();
    void cursorBChanged();
    void busyAChanged();
    void busyBChanged();

private:
    void setBusyA(bool value) {
        if (m_busyA == value) return;
        m_busyA = value;
        emit busyAChanged();
    }
    void setBusyB(bool value) {
        if (m_busyB == value) return;
        m_busyB = value;
        emit busyBChanged();
    }

    QVariantMap m_cursorA;
    QVariantMap m_cursorB;
    bool m_busyA{false};
    bool m_busyB{false};
    int m_requestsA{0};
    int m_requestsB{0};
};

class MinimalDocument final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double triggerOffsetSeconds READ triggerOffsetSeconds CONSTANT)
    Q_PROPERTY(QString valueRepresentation READ valueRepresentation CONSTANT)
public:
    using QObject::QObject;
    double triggerOffsetSeconds() const { return 0.0; }
    QString valueRepresentation() const { return QStringLiteral("secondary"); }

    Q_INVOKABLE QString analogRole(int) const { return QStringLiteral("Voltage"); }
    Q_INVOKABLE double channelPeak(int) const { return 120.0; }
    Q_INVOKABLE QString channelUnit(int) const { return QStringLiteral("V"); }
    Q_INVOKABLE QString channelName(int) const { return QStringLiteral("UL1"); }
};

class MinimalAnalysis final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE int phaseChannel(const QString&, const QString&) const { return -1; }
    Q_INVOKABLE QString channelPhase(int) const { return QStringLiteral("L1"); }
    Q_INVOKABLE QString phaseColor(int) const { return QStringLiteral("#244f9e"); }
};

void collect_vector_items(QQuickItem* item, std::vector<PhasorVectorItem*>& output) {
    if (!item) return;
    if (auto* vectorItem = qobject_cast<PhasorVectorItem*>(item)) output.push_back(vectorItem);
    for (QQuickItem* child : item->childItems()) collect_vector_items(child, output);
}

std::vector<PhasorVectorItem*> active_vector_items(QObject* root) {
    std::vector<PhasorVectorItem*> all;
    collect_vector_items(qobject_cast<QQuickItem*>(root), all);
    std::vector<PhasorVectorItem*> active;
    for (auto* item : all) if (!item->vectors().isEmpty()) active.push_back(item);
    return active;
}

void require_active_vectors_stay_committed(const std::vector<PhasorVectorItem*>& items, const char* message) {
    require(!items.empty(), "at least one visible phasor vector item is active");
    for (auto* item : items) require(item && !item->vectors().isEmpty(), message);
}

void verify_qml_stale_while_revalidate() {
    qmlRegisterType<PhasorVectorItem>("Ardirec.Render", 1, 0, "PhasorVectorItem");

    SnapshotProbe probe;
    probe.seedA(make_snapshot(0.020, 80.0, 0.0));
    probe.seedB(make_snapshot(0.030, 75.0, -120.0));
    MinimalDocument document;
    MinimalAnalysis analysis;

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("cursorSnapshotController"), &probe);
    QQmlComponent component(&engine,
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/PhasorView.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());

    QVariantList voltageChannels;
    voltageChannels.push_back(0);
    QVariantMap initial;
    initial.insert(QStringLiteral("width"), 1000.0);
    initial.insert(QStringLiteral("height"), 700.0);
    initial.insert(QStringLiteral("document"), QVariant::fromValue(static_cast<QObject*>(&document)));
    initial.insert(QStringLiteral("analysis"), QVariant::fromValue(static_cast<QObject*>(&analysis)));
    initial.insert(QStringLiteral("requestOwner"), true);
    initial.insert(QStringLiteral("visible"), true);
    initial.insert(QStringLiteral("cursorATime"), 0.020);
    initial.insert(QStringLiteral("cursorBTime"), 0.030);
    initial.insert(QStringLiteral("voltageChannels"), voltageChannels);

    std::unique_ptr<QObject> view(component.createWithInitialProperties(initial, engine.rootContext()));
    require(view != nullptr, "PhasorView runtime component is created");
    pump_events(80);

    // Component completion may request the already-seeded times. Resolve that
    // startup work first and use the resulting counts as the scrub baseline.
    probe.commitA(make_snapshot(0.020, 80.0, 0.0));
    probe.commitB(make_snapshot(0.030, 75.0, -120.0));
    pump_events(60);
    const int baselineRequestsA = probe.requestsA();

    const QVariantMap initialDisplay = view->property("displaySnapshotA").toMap();
    require(initialDisplay.value(QStringLiteral("valid")).toBool(), "initial committed C1 frame is valid");
    require_near(initialDisplay.value(QStringLiteral("time")).toDouble(), 0.020, 1.0e-12,
                 "initial C1 display uses the committed frame timestamp");
    const auto activeVectors = active_vector_items(view.get());
    require(activeVectors.size() >= 2u, "C1 and C2 vector items are populated before scrub");

    view->setProperty("cursorATime", 0.025);
    pump_events(20);
    require(probe.requestsA() == baselineRequestsA + 1,
            "first scrub position launches exactly one C1 calculation");
    require(probe.busyA(), "C1 probe reports pending calculation");
    QVariantMap pendingDisplay = view->property("displaySnapshotA").toMap();
    require(pendingDisplay.value(QStringLiteral("valid")).toBool(),
            "committed C1 frame remains valid while a newer calculation is pending");
    require_near(pendingDisplay.value(QStringLiteral("time")).toDouble(), 0.020, 1.0e-12,
                 "pending scrub keeps the previous committed C1 frame visible");
    require(view->property("refreshingA").toBool(), "pending scrub is explicitly represented as refreshing");
    require_active_vectors_stay_committed(activeVectors,
        "continuous scrub never blanks already committed vector geometry");

    // Multiple mouse positions while the calculation is active are coalesced to
    // one latest pending target instead of spawning unbounded work.
    view->setProperty("cursorATime", 0.026);
    view->setProperty("cursorATime", 0.027);
    pump_events(20);
    require(probe.requestsA() == baselineRequestsA + 1,
            "continuous scrub coalesces intermediate C1 positions while one request is in flight");
    require(view->property("cursorARequestQueued").toBool(),
            "latest C1 scrub target is retained as one queued request");
    require_active_vectors_stay_committed(activeVectors,
        "coalesced scrub keeps vector and legend inputs committed");

    probe.commitA(make_snapshot(0.025, 82.0, 5.0));
    pump_events(60);
    require(probe.requestsA() == baselineRequestsA + 2,
            "completion launches exactly one coalesced latest C1 request");
    QVariantMap intermediateDisplay = view->property("displaySnapshotA").toMap();
    require(intermediateDisplay.value(QStringLiteral("valid")).toBool(),
            "intermediate atomic commit remains a valid rendered frame");
    require_near(intermediateDisplay.value(QStringLiteral("time")).toDouble(), 0.025, 1.0e-12,
                 "completed in-flight frame commits atomically before latest revalidation");
    require_active_vectors_stay_committed(activeVectors,
        "atomic commit does not blank retained vector items");

    probe.commitA(make_snapshot(0.027, 84.0, 9.0));
    pump_events(50);
    const QVariantMap finalDisplay = view->property("displaySnapshotA").toMap();
    require(finalDisplay.value(QStringLiteral("valid")).toBool(), "latest C1 committed frame is valid");
    require_near(finalDisplay.value(QStringLiteral("time")).toDouble(), 0.027, 1.0e-12,
                 "latest queued scrub target wins after bounded revalidation");
    require(!view->property("cursorARequestQueued").toBool(), "latest C1 queue drains after final commit");
    require_active_vectors_stay_committed(activeVectors,
        "final commit keeps phasor vectors continuously populated");
}

class TestPhasorVectorItem final : public PhasorVectorItem {
public:
    using PhasorVectorItem::updatePaintNode;
};

QVariantList render_vectors(double magnitude, double angleDegrees) {
    QVariantList result;
    result.push_back(QVariantMap{
        {QStringLiteral("valid"), true},
        {QStringLiteral("magnitude"), magnitude},
        {QStringLiteral("angle"), angleDegrees},
        {QStringLiteral("phase"), QStringLiteral("L1")},
        {QStringLiteral("color"), QStringLiteral("#244f9e")},
    });
    return result;
}

void verify_retained_qsg_nodes() {
    TestPhasorVectorItem item;
    item.setWidth(420.0);
    item.setHeight(320.0);
    item.setScaleMagnitude(100.0);
    item.setVectors(render_vectors(70.0, 15.0));

    QSGNode* firstRoot = item.updatePaintNode(nullptr, nullptr);
    require(firstRoot != nullptr, "initial phasor QSG root is created");
    QSGNode* firstVectorNode = firstRoot->firstChild();
    require(firstVectorNode != nullptr, "initial phasor QSG vector node is created");

    item.setVectors(render_vectors(82.0, 42.0));
    QSGNode* secondRoot = item.updatePaintNode(firstRoot, nullptr);
    require(secondRoot == firstRoot,
            "phasor update retains the existing QSG root instead of deleting the complete scene node");
    require(secondRoot->firstChild() == firstVectorNode,
            "same-cardinality phasor update retains vector geometry nodes in place");

    QVariantList twoVectors = render_vectors(82.0, 42.0);
    twoVectors.push_back(QVariantMap{
        {QStringLiteral("valid"), true},
        {QStringLiteral("magnitude"), 55.0},
        {QStringLiteral("angle"), -118.0},
        {QStringLiteral("phase"), QStringLiteral("L2")},
        {QStringLiteral("color"), QStringLiteral("#b77900")},
    });
    item.setVectors(twoVectors);
    QSGNode* thirdRoot = item.updatePaintNode(secondRoot, nullptr);
    require(thirdRoot == firstRoot, "growing vector count still retains the QSG root");
    require(thirdRoot->firstChild() == firstVectorNode,
            "growing vector count preserves already-existing vector geometry");

    delete thirdRoot;
}
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        verify_qml_stale_while_revalidate();
        verify_retained_qsg_nodes();
        std::cout << "ardirec R5.2 Phasor zero-flicker qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R5.2 Phasor zero-flicker qualification: FAIL: " << ex.what() << '\n';
        return 1;
    }
}

#include "test_r5_phasor_zero_flicker.moc"
