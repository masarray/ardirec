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
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        verify_shared_tile_boundaries();
        std::cout << "ArDiRec R6.2 workspace runtime qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ArDiRec R6.2 workspace runtime qualification: FAIL: "
                  << ex.what() << '\n';
        return 1;
    }
}
