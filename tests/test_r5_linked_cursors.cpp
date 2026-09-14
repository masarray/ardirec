// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QThread>
#include <QUrl>
#include <QVariant>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

void pump_events(int durationMs = 50) {
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

QVariant invoke(QObject* object, const char* method, const QVariant& a, const QVariant& b) {
    QVariant result;
    const bool ok = QMetaObject::invokeMethod(object, method,
                                               Q_RETURN_ARG(QVariant, result),
                                               Q_ARG(QVariant, a),
                                               Q_ARG(QVariant, b));
    require(ok, "QML two-argument method invocation succeeds");
    return result;
}

void invoke_void(QObject* object, const char* method, const QVariant& value) {
    const bool ok = QMetaObject::invokeMethod(object, method, Q_ARG(QVariant, value));
    require(ok, "QML one-argument method invocation succeeds");
}

QObject* find_window_item(QQuickItem* item, int windowId) {
    if (!item) return nullptr;
    const QVariant id = item->property("windowId");
    if (id.isValid() && id.toInt() == windowId && item->property("windowState").isValid())
        return item;
    for (QQuickItem* child : item->childItems()) {
        if (QObject* found = find_window_item(child, windowId)) return found;
    }
    return nullptr;
}

QObject* find_named_item(QQuickItem* item, const QString& objectName) {
    if (!item) return nullptr;
    if (item->objectName() == objectName) return item;
    for (QQuickItem* child : item->childItems()) {
        if (QObject* found = find_named_item(child, objectName)) return found;
    }
    return nullptr;
}

QObject* find_host(QObject* workspace, int windowId) {
    QObject* child = find_window_item(qobject_cast<QQuickItem*>(workspace), windowId);
    require(child != nullptr, "MDI child delegate exists");
    QObject* host = find_named_item(qobject_cast<QQuickItem*>(child), QStringLiteral("r4AnalysisHost"));
    require(host != nullptr, "MDI child contains an analysis host");
    return host;
}

class AnalysisHostInterceptor final : public QQmlAbstractUrlInterceptor {
public:
    explicit AnalysisHostInterceptor(QUrl stubUrl) : m_stubUrl(std::move(stubUrl)) {}

    QUrl intercept(const QUrl& url, DataType type) override {
        if (type == QQmlAbstractUrlInterceptor::QmlFile
            && url.fileName() == QStringLiteral("AnalysisViewHost.qml")) {
            return m_stubUrl;
        }
        return url;
    }

private:
    QUrl m_stubUrl;
};

void verify_linked_local_cursor_runtime() {
    QQmlEngine engine;
    const QString qmlDir = QStringLiteral(ARDIREC_QML_DIR);
    const QString testQmlDir = QStringLiteral(ARDIREC_TEST_QML_DIR);
    AnalysisHostInterceptor interceptor(QUrl::fromLocalFile(testQmlDir + QStringLiteral("/AnalysisViewHost.qml")));
    engine.addUrlInterceptor(&interceptor);

    const QUrl workspaceUrl = QUrl::fromLocalFile(qmlDir + QStringLiteral("/MdiWorkspace.qml"));
    const QString source = QStringLiteral(R"QML(
import QtQuick
Item {
    width: 1440
    height: 900
    Loader {
        id: workspaceLoader
        objectName: "r5WorkspaceLoader"
        anchors.fill: parent
        source: "%1"
        onLoaded: {
            item.width = width
            item.height = height
            item.cursorATime = 0.100
            item.cursorBTime = 0.200
            item.viewStart = 0.0
            item.visibleDuration = 1.0
            item.hasRecord = true
            item.resetForRecord()
        }
    }
    Connections {
        target: workspaceLoader.item
        function onCursorARequested(timeSeconds) { workspaceLoader.item.cursorATime = timeSeconds }
        function onCursorBRequested(timeSeconds) { workspaceLoader.item.cursorBTime = timeSeconds }
    }
}
)QML").arg(workspaceUrl.toString());

    QQmlComponent component(&engine);
    component.setData(source.toUtf8(), QUrl::fromLocalFile(qmlDir + QStringLiteral("/R5LinkedCursorHarness.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());
    std::unique_ptr<QObject> root(component.create());
    require(root != nullptr, "R5 linked-cursor harness is created");
    pump_events(100);

    QObject* loader = root->findChild<QObject*>(QStringLiteral("r5WorkspaceLoader"));
    require(loader != nullptr, "R5 workspace loader exists");
    QObject* workspace = loader->property("item").value<QObject*>();
    require(workspace != nullptr, "production MDI workspace is loaded");
    require(workspace->property("childCount").toInt() == 1,
            "new record starts with one Time Signals child");

    const int timeOne = workspace->property("activeWindowId").toInt();
    const int timeTwo = invoke(workspace, "openView", QStringLiteral("time"), true).toInt();
    const int phasor = invoke(workspace, "openView", QStringLiteral("phasor"), true).toInt();
    const int locus = invoke(workspace, "openView", QStringLiteral("locus"), true).toInt();
    const int harmonics = invoke(workspace, "openView", QStringLiteral("harmonics"), true).toInt();
    const int table = invoke(workspace, "openView", QStringLiteral("table"), true).toInt();
    pump_events(100);

    require(workspace->property("childCount").toInt() == 6,
            "duplicate Time plus all analysis view types coexist in MDI");

    QObject* timeHostOne = find_host(workspace, timeOne);
    QObject* timeHostTwo = find_host(workspace, timeTwo);
    QObject* phasorHost = find_host(workspace, phasor);
    QObject* locusHost = find_host(workspace, locus);
    QObject* harmonicHost = find_host(workspace, harmonics);
    QObject* tableHost = find_host(workspace, table);

    for (QObject* host : {timeHostOne, timeHostTwo, phasorHost, locusHost}) {
        require(host->property("localCursorCount").toInt() == 2,
                "Time/Phasor/Locus children present linked C1+C2 controls locally");
        require(host->property("presentsCursorA").toBool()
                && host->property("presentsCursorB").toBool(),
                "dual-cursor child presents both C1 and C2");
    }
    for (QObject* host : {harmonicHost, tableHost}) {
        require(host->property("localCursorCount").toInt() == 1,
                "Harmonics/Table children present only local C1 control");
        require(host->property("presentsCursorA").toBool()
                && !host->property("presentsCursorB").toBool(),
                "single-cursor child never presents C2");
    }

    for (QObject* host : {timeHostOne, timeHostTwo, phasorHost, locusHost, harmonicHost, tableHost}) {
        require_near(host->property("cursorATime").toDouble(), 0.100, 1.0e-12,
                     "all MDI children start from one shared C1 state");
        require_near(host->property("cursorBTime").toDouble(), 0.200, 1.0e-12,
                     "all MDI children receive the one shared C2 state");
    }

    invoke_void(timeHostTwo, "requestCursorA", 0.321);
    pump_events(40);
    require_near(workspace->property("cursorATime").toDouble(), 0.321, 1.0e-12,
                 "moving C1 in one child commits to the shared workspace state");
    for (QObject* host : {timeHostOne, timeHostTwo, phasorHost, locusHost, harmonicHost, tableHost})
        require_near(host->property("cursorATime").toDouble(), 0.321, 1.0e-12,
                     "shared C1 update fans out to every applicable MDI child");

    invoke_void(locusHost, "requestCursorB", 0.456);
    pump_events(40);
    require_near(workspace->property("cursorBTime").toDouble(), 0.456, 1.0e-12,
                 "moving C2 in a dual-cursor child commits to shared workspace state");
    for (QObject* host : {timeHostOne, timeHostTwo, phasorHost, locusHost})
        require_near(host->property("cursorBTime").toDouble(), 0.456, 1.0e-12,
                     "shared C2 update fans out to every dual-cursor MDI child");
    require(!harmonicHost->property("presentsCursorB").toBool()
            && !tableHost->property("presentsCursorB").toBool(),
            "Harmonics/Table remain C1-only after shared C2 changes");

    invoke_void(tableHost, "requestCursorA", 0.654);
    pump_events(40);
    for (QObject* host : {timeHostOne, timeHostTwo, phasorHost, locusHost, harmonicHost, tableHost})
        require_near(host->property("cursorATime").toDouble(), 0.654, 1.0e-12,
                     "C1 movement from Table links back to all other windows");

    QVariant ignored;
    require(QMetaObject::invokeMethod(workspace, "minimizeWindow",
                                      Q_RETURN_ARG(QVariant, ignored),
                                      Q_ARG(QVariant, locus)),
            "Locus minimize invocation succeeds");
    pump_events(40);
    require(find_host(workspace, locus)->property("presentedCursorCount").toInt() == 0,
            "minimized child presents no local cursor interaction surface");
}
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        verify_linked_local_cursor_runtime();
        std::cout << "ardirec R5.1 linked local cursor runtime qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R5.1 linked local cursor runtime qualification: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
