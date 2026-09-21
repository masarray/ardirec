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

void pump_events(int durationMs = 35) {
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

class AnalysisHostInterceptor final : public QQmlAbstractUrlInterceptor {
public:
    explicit AnalysisHostInterceptor(QUrl stubUrl) : m_stubUrl(std::move(stubUrl)) {}

    QUrl intercept(const QUrl& url, DataType type) override {
        if (type == QQmlAbstractUrlInterceptor::QmlFile
            && url.fileName() == QStringLiteral("AnalysisViewHost.qml"))
            return m_stubUrl;
        return url;
    }

private:
    QUrl m_stubUrl;
};

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

QObject* find_window(QObject* root, int windowId) {
    return find_window_item(qobject_cast<QQuickItem*>(root), windowId);
}

QVariant invoke(QObject* object, const char* method) {
    QVariant result;
    require(QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(QVariant, result)),
            "QML method invocation succeeds");
    return result;
}

QVariant invoke(QObject* object, const char* method, const QVariant& a, const QVariant& b) {
    QVariant result;
    require(QMetaObject::invokeMethod(object, method,
                                      Q_RETURN_ARG(QVariant, result),
                                      Q_ARG(QVariant, a), Q_ARG(QVariant, b)),
            "QML two-argument invocation succeeds");
    return result;
}

QVariant invoke(QObject* object, const char* method,
                const QVariant& a, const QVariant& b, const QVariant& c,
                const QVariant& d, const QVariant& e, const QVariant& f) {
    QVariant result;
    require(QMetaObject::invokeMethod(object, method,
                                      Q_RETURN_ARG(QVariant, result),
                                      Q_ARG(QVariant, a), Q_ARG(QVariant, b),
                                      Q_ARG(QVariant, c), Q_ARG(QVariant, d),
                                      Q_ARG(QVariant, e), Q_ARG(QVariant, f)),
            "QML six-argument invocation succeeds");
    return result;
}

void verify_shared_tile_boundaries() {
    QQmlEngine engine;
    AnalysisHostInterceptor interceptor(
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_TEST_QML_DIR)
                            + QStringLiteral("/AnalysisViewHost.qml")));
    engine.addUrlInterceptor(&interceptor);

    QQmlComponent component(
        &engine,
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR)
                            + QStringLiteral("/MdiWorkspace.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());

    std::unique_ptr<QObject> workspace(component.create());
    require(workspace != nullptr, "R6 workspace runtime component is created");
    workspace->setProperty("width", 1800.0);
    workspace->setProperty("height", 1200.0);
    workspace->setProperty("hasRecord", true);
    invoke(workspace.get(), "resetForRecord");
    pump_events();

    const int first = workspace->property("activeWindowId").toInt();
    const int second = invoke(workspace.get(), "openView",
                              QStringLiteral("time"), true).toInt();
    const int third = invoke(workspace.get(), "openView",
                             QStringLiteral("phasor"), true).toInt();
    pump_events();
    require(first > 0 && second > 0 && third > 0, "three tile children exist");

    invoke(workspace.get(), "tileVertical");
    pump_events();
    require(workspace->property("arrangementMode").toString()
                == QStringLiteral("tile-vertical"),
            "vertical Tile enters managed topology");

    QObject* a = find_window(workspace.get(), first);
    QObject* b = find_window(workspace.get(), second);
    QObject* c = find_window(workspace.get(), third);
    require(a && b && c, "vertical tile delegates remain alive");
    require(a->property("tileManaged").toBool()
            && b->property("tileManaged").toBool()
            && c->property("tileManaged").toBool(),
            "tiled children expose managed geometry state");

    const double thirdXBefore = c->property("x").toDouble();
    const double thirdWidthBefore = c->property("width").toDouble();

    // Edge mask 2 = right edge. The requested right edge is x + width = 700.
    invoke(workspace.get(), "resizeTileBoundary",
           first, 2, 0.0, 0.0, 700.0, 1200.0);
    pump_events();

    a = find_window(workspace.get(), first);
    b = find_window(workspace.get(), second);
    c = find_window(workspace.get(), third);
    require_near(a->property("width").toDouble(), 700.0, 0.75,
                 "shared vertical tile boundary resizes both neighbors");
    require_near(b->property("x").toDouble(), 700.0, 0.75,
                 "shared vertical tile boundary resizes both neighbors");
    require_near(a->property("x").toDouble() + a->property("width").toDouble(),
                 b->property("x").toDouble(), 0.75,
                 "vertical Tile keeps adjacent panes gap-free");
    require_near(b->property("x").toDouble() + b->property("width").toDouble(),
                 thirdXBefore, 0.75,
                 "shared boundary preserves the neighbor pair outer edge");
    require_near(c->property("x").toDouble(), thirdXBefore, 0.75,
                 "multi-pane Tile leaves non-neighbor position unchanged");
    require_near(c->property("width").toDouble(), thirdWidthBefore, 0.75,
                 "multi-pane Tile leaves non-neighbor size unchanged");
    require_near(c->property("x").toDouble() + c->property("width").toDouble(),
                 1800.0, 0.75,
                 "tile resize preserves the occupied workspace extent");

    // A direct free-geometry request cannot silently break the managed topology.
    QMetaObject::invokeMethod(workspace.get(), "updateGeometry",
                              Q_ARG(QVariant, first), Q_ARG(QVariant, 90.0),
                              Q_ARG(QVariant, 70.0), Q_ARG(QVariant, 800.0),
                              Q_ARG(QVariant, 900.0));
    pump_events();
    a = find_window(workspace.get(), first);
    require_near(a->property("x").toDouble(), 0.0, 0.75,
                 "managed Tile rejects independent free movement");

    invoke(workspace.get(), "tileHorizontal");
    pump_events();
    require(workspace->property("arrangementMode").toString()
                == QStringLiteral("tile-horizontal"),
            "horizontal Tile enters managed topology");

    a = find_window(workspace.get(), first);
    b = find_window(workspace.get(), second);
    c = find_window(workspace.get(), third);
    const double thirdYBefore = c->property("y").toDouble();
    const double thirdHeightBefore = c->property("height").toDouble();

    // Edge mask 8 = bottom edge. The requested bottom edge is y + height = 500.
    invoke(workspace.get(), "resizeTileBoundary",
           first, 8, 0.0, 0.0, 1800.0, 500.0);
    pump_events();

    a = find_window(workspace.get(), first);
    b = find_window(workspace.get(), second);
    c = find_window(workspace.get(), third);
    require_near(a->property("height").toDouble(), 500.0, 0.75,
                 "shared horizontal tile boundary resizes both neighbors");
    require_near(b->property("y").toDouble(), 500.0, 0.75,
                 "shared horizontal tile boundary resizes both neighbors");
    require_near(a->property("y").toDouble() + a->property("height").toDouble(),
                 b->property("y").toDouble(), 0.75,
                 "horizontal Tile keeps adjacent panes gap-free");
    require_near(b->property("y").toDouble() + b->property("height").toDouble(),
                 thirdYBefore, 0.75,
                 "horizontal shared boundary preserves the pair outer edge");
    require_near(c->property("y").toDouble(), thirdYBefore, 0.75,
                 "horizontal multi-pane Tile leaves non-neighbor position unchanged");
    require_near(c->property("height").toDouble(), thirdHeightBefore, 0.75,
                 "horizontal multi-pane Tile leaves non-neighbor size unchanged");
    require_near(c->property("y").toDouble() + c->property("height").toDouble(),
                 1200.0, 0.75,
                 "tile resize preserves the occupied workspace extent");
}

void verify_virtual_workspace_and_direct_drag() {
    QQmlEngine engine;
    AnalysisHostInterceptor interceptor(
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_TEST_QML_DIR)
                            + QStringLiteral("/AnalysisViewHost.qml")));
    engine.addUrlInterceptor(&interceptor);

    QQmlComponent component(
        &engine,
        QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR)
                            + QStringLiteral("/MdiWorkspace.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());

    std::unique_ptr<QObject> workspace(component.create());
    require(workspace != nullptr, "R6.3 workspace runtime component is created");
    workspace->setProperty("width", 1000.0);
    workspace->setProperty("height", 700.0);
    workspace->setProperty("hasRecord", true);
    invoke(workspace.get(), "resetForRecord");
    pump_events();

    const int windowId = workspace->property("activeWindowId").toInt();
    require(windowId > 0, "free workspace has an active child");

    // Free geometry is logical desktop geometry: right/bottom motion must not
    // be clamped merely because the current viewport ends.
    require(QMetaObject::invokeMethod(
                workspace.get(), "updateGeometry",
                Q_ARG(QVariant, windowId), Q_ARG(QVariant, 1250.0),
                Q_ARG(QVariant, 920.0), Q_ARG(QVariant, 520.0),
                Q_ARG(QVariant, 360.0)),
            "free geometry update succeeds");
    pump_events();

    QObject* child = find_window(workspace.get(), windowId);
    require(child != nullptr, "free child delegate remains alive");
    require_near(child->property("x").toDouble(), 1250.0, 0.75,
                 "free drag follows every pointer update without viewport clamp");
    require_near(child->property("y").toDouble(), 920.0, 0.75,
                 "free drag follows every pointer update without viewport clamp");
    require(workspace->property("workspaceContentWidth").toDouble() > 1770.0,
            "moving a child beyond the viewport expands logical workspace width");
    require(workspace->property("workspaceContentHeight").toDouble() > 1280.0,
            "moving a child beyond the viewport expands logical workspace height");
    require(workspace->property("horizontalScrollRange").toDouble() > 0.0
            && workspace->property("verticalScrollRange").toDouble() > 0.0,
            "workspace exposes scroll range instead of clamping the child");

    // Pointer updates continue to write their exact requested logical position.
    require(QMetaObject::invokeMethod(
                workspace.get(), "updateGeometry",
                Q_ARG(QVariant, windowId), Q_ARG(QVariant, 1315.0),
                Q_ARG(QVariant, 955.0), Q_ARG(QVariant, 520.0),
                Q_ARG(QVariant, 360.0)),
            "second direct free geometry update succeeds");
    child = find_window(workspace.get(), windowId);
    require_near(child->property("x").toDouble(), 1315.0, 0.75,
                 "free drag follows every pointer update without viewport clamp");
    require_near(child->property("y").toDouble(), 955.0, 0.75,
                 "free drag follows every pointer update without viewport clamp");

    const double screenXBefore =
        child->property("x").toDouble() - workspace->property("workspaceScrollX").toDouble();
    const double screenYBefore =
        child->property("y").toDouble() - workspace->property("workspaceScrollY").toDouble();

    require(QMetaObject::invokeMethod(
                workspace.get(), "beginFreeDrag",
                Q_ARG(QVariant, windowId), Q_ARG(QVariant, 998.0),
                Q_ARG(QVariant, 698.0)),
            "edge auto-scroll drag begins");
    const bool advanced = invoke(workspace.get(), "stepAutoScroll").toBool();
    require(QMetaObject::invokeMethod(
                workspace.get(), "endFreeDrag", Q_ARG(QVariant, windowId)),
            "edge auto-scroll drag ends");

    require(advanced, "edge drag advances workspace scroll while preserving pointer-relative position");
    require(workspace->property("workspaceScrollX").toDouble() > 0.0
            && workspace->property("workspaceScrollY").toDouble() > 0.0,
            "edge drag advances workspace scroll while preserving pointer-relative position");

    child = find_window(workspace.get(), windowId);
    require(child != nullptr, "auto-scrolled child remains alive");
    const double screenXAfter =
        child->property("x").toDouble() - workspace->property("workspaceScrollX").toDouble();
    const double screenYAfter =
        child->property("y").toDouble() - workspace->property("workspaceScrollY").toDouble();
    require_near(screenXAfter, screenXBefore, 0.75,
                 "edge drag advances workspace scroll while preserving pointer-relative position");
    require_near(screenYAfter, screenYBefore, 0.75,
                 "edge drag advances workspace scroll while preserving pointer-relative position");
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        verify_shared_tile_boundaries();
        verify_virtual_workspace_and_direct_drag();
        std::cout << "ArDiRec R6.3 workspace runtime qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ArDiRec R6.3 workspace runtime qualification: FAIL: "
                  << ex.what() << '\n';
        return 1;
    }
}
