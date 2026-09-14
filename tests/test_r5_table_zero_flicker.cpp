// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"
#include "table_snapshot_controller.hpp"

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

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || !std::isfinite(expected) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

void pump_events(int durationMs = 30) {
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
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(!document.loading(), "R5.3 fixture load completes before timeout");
    require(document.error().isEmpty(), "R5.3 fixture opens without error");
}

void wait_for_table(TableSnapshotController& table, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (table.busy() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(!table.busy(), "async Table frame completes before timeout");
}

QVariantMap row_for_channel(const QVariantMap& frame, int channelIndex) {
    const QVariantList rows = frame.value(QStringLiteral("rows")).toList();
    for (const QVariant& value : rows) {
        const QVariantMap row = value.toMap();
        if (row.value(QStringLiteral("channelIndex"), -1).toInt() == channelIndex) return row;
    }
    return {};
}

void verify_async_atomic_frame() {
    const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR)
                                              / "distance_sigra_parity.cfg";
    DocumentController document;
    document.openCfg(QUrl::fromLocalFile(QString::fromStdString(cfgPath.string())));
    wait_for_document(document);
    require(document.analogCount() > 0, "R5.3 fixture contains analog channels");

    TableSnapshotController table(&document);
    int frameChanges = 0;
    QObject::connect(&table, &TableSnapshotController::frameChanged,
                     [&frameChanges]() { ++frameChanges; });

    QVariantList channels;
    for (int channel = 0; channel < document.analogCount(); ++channel) channels.push_back(channel);

    constexpr double initialTime = 0.025;
    table.requestFrame(channels, initialTime, QStringLiteral("record"), false);
    require(table.busy(), "initial Table frame runs asynchronously");
    wait_for_table(table);

    const QVariantMap initial = table.frame();
    require(initial.value(QStringLiteral("valid")).toBool(), "initial committed Table frame is valid");
    require_near(initial.value(QStringLiteral("time")).toDouble(), initialTime, 1.0e-12,
                 "initial committed Table frame carries C1 time");
    const QVariantList initialRows = initial.value(QStringLiteral("rows")).toList();
    require(!initialRows.isEmpty(), "initial committed Table frame contains rows");
    require(frameChanges == 1, "initial Table frame is one atomic publication");

    // The async worker must preserve the existing Table numerical semantics.
    // This cross-check is intentionally local to R5.3 and does not alter DFT or
    // the R5.4 full-cycle qualification policy.
    const int firstChannel = initialRows.front().toMap().value(QStringLiteral("channelIndex"), -1).toInt();
    require(firstChannel >= 0, "initial Table row exposes its channel index");
    const QVariantMap asyncRow = row_for_channel(initial, firstChannel);
    const QVariantMap legacyRow = table.snapshotAt(firstChannel, initialTime);
    require(asyncRow.value(QStringLiteral("valid")).toBool()
                && legacyRow.value(QStringLiteral("valid")).toBool(),
            "async and legacy Table rows are both valid for parity check");
    require_near(asyncRow.value(QStringLiteral("fundamental")).toDouble(),
                 legacyRow.value(QStringLiteral("fundamental")).toDouble(), 1.0e-9,
                 "async Table H1 preserves legacy numerical value");
    require_near(asyncRow.value(QStringLiteral("rms")).toDouble(),
                 legacyRow.value(QStringLiteral("rms")).toDouble(), 1.0e-9,
                 "async Table RMS preserves legacy numerical value");
    require_near(asyncRow.value(QStringLiteral("angle")).toDouble(),
                 legacyRow.value(QStringLiteral("angle")).toDouble(), 1.0e-8,
                 "async Table phase angle preserves legacy numerical value");
    require_near(asyncRow.value(QStringLiteral("thd")).toDouble(),
                 legacyRow.value(QStringLiteral("thd")).toDouble(), 1.0e-9,
                 "async Table THD preserves legacy numerical value");

    const QVariantMap frozen = table.frame();
    const int frozenRows = frozen.value(QStringLiteral("rows")).toList().size();

    // A burst of cursor positions is intentionally issued without pumping the
    // event loop. One job is in flight and all later positions collapse to the
    // single latest pending target.
    table.requestFrame(channels, 0.030, QStringLiteral("record"), false);
    table.requestFrame(channels, 0.035, QStringLiteral("record"), false);
    table.requestFrame(channels, 0.040, QStringLiteral("record"), false);
    table.requestFrame(channels, 0.045, QStringLiteral("record"), false);
    require(table.busy(), "continuous scrub leaves one Table calculation active");
    require(table.frame() == frozen,
            "previous committed table frame remains visible while scrub calculation is pending");
    require(table.frame().value(QStringLiteral("rows")).toList().size() == frozenRows,
            "pending Table work cannot collapse the committed row model");
    require(frameChanges == 1,
            "pending and intermediate Table work cannot publish partial frames");

    wait_for_table(table);
    const QVariantMap finalFrame = table.frame();
    require(finalFrame.value(QStringLiteral("valid")).toBool(), "latest Table frame is valid");
    require_near(finalFrame.value(QStringLiteral("time")).toDouble(), 0.045, 1.0e-12,
                 "latest queued table request wins after bounded revalidation");
    require(frameChanges == 2,
            "scrub burst produces exactly one new atomic Table publication");
    require(finalFrame.value(QStringLiteral("rows")).toList().size() == frozenRows,
            "same-scope scrub keeps Table row count stable between commits");

    table.requestFrame(channels, 0.050, QStringLiteral("record"), false);
    require(table.busy(), "Table worker can be active before document close");
    document.closeDocument();
    pump_events(30);
    require(!table.busy(), "document close synchronously quiesces Table worker state");
    require(table.frame().isEmpty(), "document close invalidates committed Table frame");
}

QString component_errors(const QQmlComponent& component) {
    QString result;
    for (const auto& error : component.errors()) {
        if (!result.isEmpty()) result += QLatin1Char('\n');
        result += error.toString();
    }
    return result;
}

QVariantMap make_summary(int count) {
    return {{QStringLiteral("count"), count},
            {QStringLiteral("abnormalCount"), 0},
            {QStringLiteral("maxThdChannel"), count > 0 ? 0 : -1},
            {QStringLiteral("maxThd"), 1.0},
            {QStringLiteral("maxDcChannel"), count > 0 ? 0 : -1},
            {QStringLiteral("maxDcPercent"), 0.5},
            {QStringLiteral("maxCrestChannel"), count > 0 ? 0 : -1},
            {QStringLiteral("maxCrestFactor"), 1.4},
            {QStringLiteral("maxVoltageRmsChannel"), count > 0 ? 0 : -1},
            {QStringLiteral("maxVoltageRms"), 110.0},
            {QStringLiteral("maxCurrentRmsChannel"), -1},
            {QStringLiteral("maxCurrentRms"), 0.0},
            {QStringLiteral("maxHarmonicOrder"), 25},
            {QStringLiteral("sampleRate"), 10000.0}};
}

QVariantMap make_qml_frame(double timeSeconds, int rowCount, double valueOffset = 0.0) {
    QVariantList rows;
    QVariantList channels;
    for (int channel = 0; channel < rowCount; ++channel) {
        const double base = 100.0 + valueOffset + channel;
        rows.push_back(QVariantMap{
            {QStringLiteral("valid"), true},
            {QStringLiteral("channelIndex"), channel},
            {QStringLiteral("name"), QStringLiteral("U%1").arg(channel + 1)},
            {QStringLiteral("role"), QStringLiteral("Voltage")},
            {QStringLiteral("phase"), channel % 3 == 0 ? QStringLiteral("L1")
                                                       : channel % 3 == 1 ? QStringLiteral("L2")
                                                                          : QStringLiteral("L3")},
            {QStringLiteral("unit"), QStringLiteral("V")},
            {QStringLiteral("instant"), base},
            {QStringLiteral("rms"), base},
            {QStringLiteral("fundamental"), base},
            {QStringLiteral("angle"), static_cast<double>((channel % 3) * -120)},
            {QStringLiteral("extremum"), base * 1.4},
            {QStringLiteral("crestFactor"), 1.4},
            {QStringLiteral("dc"), 0.1},
            {QStringLiteral("dcPercent"), 0.1},
            {QStringLiteral("thd"), 1.0},
            {QStringLiteral("h2"), 0.2},
            {QStringLiteral("h3"), 0.3},
            {QStringLiteral("h5"), 0.1},
            {QStringLiteral("abnormal"), false},
        });
        channels.push_back(channel);
    }
    return {{QStringLiteral("valid"), true},
            {QStringLiteral("time"), timeSeconds},
            {QStringLiteral("rows"), rows},
            {QStringLiteral("displayedChannels"), channels},
            {QStringLiteral("summary"), make_summary(rowCount)},
            {QStringLiteral("sortMode"), QStringLiteral("record")},
            {QStringLiteral("abnormalOnly"), false}};
}

class TableFrameProbe final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap frame READ frame NOTIFY frameChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    using QObject::QObject;
    QVariantMap frame() const { return m_frame; }
    bool busy() const { return m_busy; }
    int requestCount() const { return m_requestCount; }
    double requestedTime() const { return m_requestedTime; }

    void seed(const QVariantMap& value) { m_frame = value; }

    Q_INVOKABLE void requestFrame(const QVariantList&, double timeSeconds,
                                  const QString&, bool) {
        ++m_requestCount;
        m_requestedTime = timeSeconds;
        if (!m_busy) {
            m_busy = true;
            emit busyChanged();
        }
    }

    void commit(const QVariantMap& value) {
        m_frame = value;
        emit frameChanged();
        if (m_busy) {
            m_busy = false;
            emit busyChanged();
        }
    }

signals:
    void frameChanged();
    void busyChanged();

private:
    QVariantMap m_frame;
    bool m_busy{false};
    int m_requestCount{0};
    double m_requestedTime{0.0};
};

class MinimalTableDocument final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int analogCount READ analogCount CONSTANT)
    Q_PROPERTY(int selectedAnalogIndex READ selectedAnalogIndex NOTIFY selectedAnalogIndexChanged)
    Q_PROPERTY(double triggerOffsetSeconds READ triggerOffsetSeconds CONSTANT)
    Q_PROPERTY(QString valueRepresentation READ valueRepresentation CONSTANT)
public:
    using QObject::QObject;
    int analogCount() const { return 30; }
    int selectedAnalogIndex() const { return m_selected; }
    double triggerOffsetSeconds() const { return 0.0; }
    QString valueRepresentation() const { return QStringLiteral("secondary"); }

    Q_INVOKABLE QString analogRole(int) const { return QStringLiteral("Voltage"); }
    Q_INVOKABLE QString channelName(int index) const { return QStringLiteral("U%1").arg(index + 1); }
    Q_INVOKABLE QString formatChannelValue(int, double value) const {
        return QString::number(value, 'f', 2) + QStringLiteral(" V");
    }
    Q_INVOKABLE void selectChannel(int index) {
        if (m_selected == index) return;
        m_selected = index;
        emit selectedAnalogIndexChanged();
    }

signals:
    void selectedAnalogIndexChanged();

private:
    int m_selected{-1};
};

class MinimalTableAnalysis final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE QString phaseColor(int) const { return QStringLiteral("#244f9e"); }
};

QQuickItem* find_item(QQuickItem* item, const QString& objectName) {
    if (!item) return nullptr;
    if (item->objectName() == objectName) return item;
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* result = find_item(child, objectName)) return result;
    }
    return nullptr;
}

void verify_qml_geometry_and_scroll_stability() {
    TableFrameProbe probe;
    probe.seed(make_qml_frame(0.020, 30));
    MinimalTableDocument document;
    MinimalTableAnalysis analysis;

    QQmlEngine engine;
    QQmlComponent component(&engine,
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/ValueTableView.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());

    QVariantList visibleChannels;
    for (int channel = 0; channel < 30; ++channel) visibleChannels.push_back(channel);
    QVariantMap initial;
    initial.insert(QStringLiteral("width"), 1100.0);
    initial.insert(QStringLiteral("height"), 640.0);
    initial.insert(QStringLiteral("visible"), true);
    initial.insert(QStringLiteral("document"), QVariant::fromValue(static_cast<QObject*>(&document)));
    initial.insert(QStringLiteral("analysis"), QVariant::fromValue(static_cast<QObject*>(&analysis)));
    initial.insert(QStringLiteral("snapshot"), QVariant::fromValue(static_cast<QObject*>(&probe)));
    initial.insert(QStringLiteral("visibleChannels"), visibleChannels);
    initial.insert(QStringLiteral("cursorTime"), 0.020);

    std::unique_ptr<QObject> view(component.createWithInitialProperties(initial, engine.rootContext()));
    require(view != nullptr, "ValueTableView runtime component is created");
    pump_events(100);

    // Resolve the harmless onCompleted request and establish one committed GUI frame.
    probe.commit(make_qml_frame(0.020, 30));
    pump_events(80);

    auto* rootItem = qobject_cast<QQuickItem*>(view.get());
    QQuickItem* rows = find_item(rootItem, QStringLiteral("engineeringTableRows"));
    require(rows != nullptr, "Table ListView exposes stable regression objectName");
    require(view->property("displayedRows").toList().size() == 30,
            "Table starts with all committed rows visible");
    const double rootHeight = rootItem->height();

    rows->setProperty("contentY", 260.0);
    pump_events(30);
    const double scrolledY = rows->property("contentY").toDouble();
    require(scrolledY > 100.0, "Table regression establishes a non-zero scroll position");

    view->setProperty("cursorTime", 0.030);
    pump_events(30);
    require(probe.busy(), "Table reports pending work during cursor scrub");
    require(view->property("displayedRows").toList().size() == 30,
            "pending scrub never clears the committed Table rows");
    require_near(view->property("committedFrame").toMap().value(QStringLiteral("time")).toDouble(),
                 0.020, 1.0e-12,
                 "pending scrub keeps the previous committed Table timestamp");
    require_near(rootItem->height(), rootHeight, 1.0e-9,
                 "pending Table work cannot change the view vertical geometry");
    require_near(rows->property("contentY").toDouble(), scrolledY, 1.0,
                 "pending Table work preserves scroll position");

    // The next complete frame has the same row count but different values/time.
    // ValueTableView must swap it once without collapsing geometry or jumping to top.
    probe.commit(make_qml_frame(0.030, 30, 10.0));
    pump_events(100);
    require(view->property("displayedRows").toList().size() == 30,
            "atomic Table commit preserves stable row count");
    require_near(view->property("committedFrame").toMap().value(QStringLiteral("time")).toDouble(),
                 0.030, 1.0e-12,
                 "Table GUI atomically adopts the completed frame");
    require_near(rootItem->height(), rootHeight, 1.0e-9,
                 "Table commit does not collapse or expand vertical geometry");
    require_near(rows->property("contentY").toDouble(), scrolledY, 2.0,
                 "Table commit restores the user's scroll position");
}
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        verify_async_atomic_frame();
        verify_qml_geometry_and_scroll_stability();
        std::cout << "ardirec R5.3 Table zero-flicker qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R5.3 Table zero-flicker qualification: FAIL: " << ex.what() << '\n';
        return 1;
    }
}

#include "test_r5_table_zero_flicker.moc"
