// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QThread>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>
#include <QtQml/qqml.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

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

QObject* create_file_component(QQmlEngine& engine, const QString& path) {
    QQmlComponent component(&engine, QUrl::fromLocalFile(path));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());
    QObject* object = component.create();
    require(object != nullptr, "QML component instance is created");
    return object;
}

QVariant invoke(QObject* object, const char* method) {
    QVariant result;
    const bool ok = QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(QVariant, result));
    require(ok, "QML method invocation succeeds");
    return result;
}

QVariant invoke(QObject* object, const char* method, const QVariant& a) {
    QVariant result;
    const bool ok = QMetaObject::invokeMethod(object, method,
                                               Q_RETURN_ARG(QVariant, result),
                                               Q_ARG(QVariant, a));
    require(ok, "QML one-argument method invocation succeeds");
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

QVariant invoke(QObject* object, const char* method,
                const QVariant& a, const QVariant& b, const QVariant& c,
                const QVariant& d, const QVariant& e) {
    QVariant result;
    const bool ok = QMetaObject::invokeMethod(object, method,
                                               Q_RETURN_ARG(QVariant, result),
                                               Q_ARG(QVariant, a),
                                               Q_ARG(QVariant, b),
                                               Q_ARG(QVariant, c),
                                               Q_ARG(QVariant, d),
                                               Q_ARG(QVariant, e));
    require(ok, "QML five-argument method invocation succeeds");
    return result;
}

QObject* find_window(QObject* root, int windowId) {
    const auto objects = root->findChildren<QObject*>();
    for (QObject* object : objects) {
        const QVariant id = object->property("windowId");
        if (id.isValid() && id.toInt() == windowId && object->property("windowState").isValid())
            return object;
    }
    return nullptr;
}

QObject* find_host(QObject* childWindow) {
    if (!childWindow) return nullptr;
    const auto objects = childWindow->findChildren<QObject*>(QStringLiteral("r4AnalysisHost"));
    return objects.isEmpty() ? nullptr : objects.front();
}

QObject* find_open_popup(QObject* root) {
    const auto objects = root->findChildren<QObject*>();
    for (QObject* object : objects) {
        const QVariant opened = object->property("opened");
        const QVariant modal = object->property("modal");
        if (opened.isValid() && opened.toBool() && modal.isValid() && modal.toBool())
            return object;
    }
    return nullptr;
}

QObject* find_dimension_label(QObject* root) {
    const auto objects = root->findChildren<QObject*>();
    for (QObject* object : objects) {
        const QVariant text = object->property("text");
        if (!text.isValid()) continue;
        const QString value = text.toString();
        if (value.contains(QStringLiteral(" ms")) && !value.startsWith(QStringLiteral("Trigger")))
            return object;
    }
    return nullptr;
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

class StubPhasorVectorItem final : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QVariant vectors READ vectors WRITE setVectors)
    Q_PROPERTY(double scaleMagnitude READ scaleMagnitude WRITE setScaleMagnitude)
public:
    using QQuickItem::QQuickItem;
    QVariant vectors() const { return m_vectors; }
    void setVectors(const QVariant& value) { m_vectors = value; }
    double scaleMagnitude() const { return m_scaleMagnitude; }
    void setScaleMagnitude(double value) { m_scaleMagnitude = value; }
private:
    QVariant m_vectors;
    double m_scaleMagnitude{0.0};
};

class CursorRequestProbe final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap cursorA READ cursorA NOTIFY cursorAChanged)
    Q_PROPERTY(QVariantMap cursorB READ cursorB NOTIFY cursorBChanged)
    Q_PROPERTY(bool busyA READ busyA NOTIFY busyAChanged)
    Q_PROPERTY(bool busyB READ busyB NOTIFY busyBChanged)
public:
    QVariantMap cursorA() const { return {}; }
    QVariantMap cursorB() const { return {}; }
    bool busyA() const { return false; }
    bool busyB() const { return false; }
    int requestsA() const { return m_requestsA; }
    int requestsB() const { return m_requestsB; }

    Q_INVOKABLE void requestCursorA(double) { ++m_requestsA; }
    Q_INVOKABLE void requestCursorB(double) { ++m_requestsB; }

signals:
    void cursorAChanged();
    void cursorBChanged();
    void busyAChanged();
    void busyBChanged();

private:
    int m_requestsA{0};
    int m_requestsB{0};
};

class MinimalDocument final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double triggerOffsetSeconds READ triggerOffsetSeconds CONSTANT)
    Q_PROPERTY(double nominalFrequency READ nominalFrequency CONSTANT)
public:
    using QObject::QObject;
    double triggerOffsetSeconds() const { return 0.0; }
    double nominalFrequency() const { return 50.0; }
};

void verify_mdi_runtime() {
    QQmlEngine engine;
    const QString qmlDir = QStringLiteral(ARDIREC_QML_DIR);
    const QString testQmlDir = QStringLiteral(ARDIREC_TEST_QML_DIR);
    AnalysisHostInterceptor interceptor(QUrl::fromLocalFile(testQmlDir + QStringLiteral("/AnalysisViewHost.qml")));
    engine.setUrlInterceptor(&interceptor);

    QObject documentProbe;
    QObject analysisProbe;
    QObject locusProbe;
    QObject harmonicProbe;
    QObject tableProbe;

    std::unique_ptr<QObject> workspace(create_file_component(engine, qmlDir + QStringLiteral("/MdiWorkspace.qml")));
    workspace->setProperty("width", 1600.0);
    workspace->setProperty("height", 1200.0);
    workspace->setProperty("document", QVariant::fromValue(static_cast<QObject*>(&documentProbe)));
    workspace->setProperty("analysis", QVariant::fromValue(static_cast<QObject*>(&analysisProbe)));
    workspace->setProperty("locusAnalysis", QVariant::fromValue(static_cast<QObject*>(&locusProbe)));
    workspace->setProperty("harmonicSnapshot", QVariant::fromValue(static_cast<QObject*>(&harmonicProbe)));
    workspace->setProperty("tableSnapshot", QVariant::fromValue(static_cast<QObject*>(&tableProbe)));
    workspace->setProperty("hasRecord", true);
    invoke(workspace.get(), "resetForRecord");
    pump_events();

    require(workspace->property("childCount").toInt() == 1, "new record starts with exactly one Time Signals child");
    const int firstTime = workspace->property("activeWindowId").toInt();
    require(firstTime > 0, "initial Time Signals child is active");

    const int secondTime = invoke(workspace.get(), "openView", QStringLiteral("time"), true).toInt();
    const int phasorOne = invoke(workspace.get(), "openView", QStringLiteral("phasor"), true).toInt();
    const int locusOne = invoke(workspace.get(), "openView", QStringLiteral("locus"), true).toInt();
    pump_events();
    require(workspace->property("childCount").toInt() == 4, "four simultaneous MDI child windows are created");
    require(secondTime != firstTime, "duplicate Time Signals windows have distinct identities");

    for (int id : {firstTime, secondTime, phasorOne, locusOne}) {
        QObject* child = find_window(workspace.get(), id);
        require(child != nullptr, "runtime MDI delegate exists for every model child");
        QObject* host = find_host(child);
        require(host != nullptr, "every MDI child contains exactly one analysis host");
        require(host->property("document").value<QObject*>() == &documentProbe,
                "all children share one document engine instance");
        require(host->property("analysis").value<QObject*>() == &analysisProbe,
                "all children share one analysis engine instance");
        require(host->property("locusAnalysis").value<QObject*>() == &locusProbe,
                "all children share one locus engine instance");
        require(host->property("harmonicSnapshot").value<QObject*>() == &harmonicProbe,
                "all children share one harmonic snapshot engine instance");
        require(host->property("tableSnapshot").value<QObject*>() == &tableProbe,
                "all children share one table snapshot engine instance");
    }

    invoke(workspace.get(), "updateGeometry", secondTime, 180.0, 110.0, 520.0, 330.0);
    pump_events();
    QObject* moved = find_window(workspace.get(), secondTime);
    require(moved != nullptr, "moved child remains alive");
    require_near(moved->property("x").toDouble(), 180.0, 0.5, "manual MDI move updates x");
    require_near(moved->property("y").toDouble(), 110.0, 0.5, "manual MDI move updates y");
    require_near(moved->property("width").toDouble(), 520.0, 0.5, "manual MDI resize updates width");
    require_near(moved->property("height").toDouble(), 330.0, 0.5, "manual MDI resize updates height");

    invoke(workspace.get(), "activateWindow", phasorOne);
    require(workspace->property("activeWindowId").toInt() == phasorOne, "activation changes the active MDI child");

    const int phasorTwo = invoke(workspace.get(), "openView", QStringLiteral("phasor"), true).toInt();
    pump_events();
    require(workspace->property("childCount").toInt() == 5, "duplicate Phasor child is supported");
    invoke(workspace.get(), "activateWindow", phasorOne);
    pump_events();
    QObject* p1 = find_window(workspace.get(), phasorOne);
    QObject* p2 = find_window(workspace.get(), phasorTwo);
    require(p1 && p2, "both duplicate Phasor children exist");
    require(p1->property("requestOwner").toBool() != p2->property("requestOwner").toBool(),
            "exactly one visible duplicate Phasor child owns shared snapshot requests");

    invoke(workspace.get(), "minimizeWindow", phasorOne);
    pump_events();
    p1 = find_window(workspace.get(), phasorOne);
    p2 = find_window(workspace.get(), phasorTwo);
    require(p1->property("windowState").toString() == QStringLiteral("minimized"), "active child can be minimized");
    require(!p1->property("requestOwner").toBool(), "minimized child relinquishes request ownership");
    require(p2->property("requestOwner").toBool(), "visible duplicate inherits request ownership");
    QObject* minimizedHost = find_host(p1);
    require(minimizedHost && !minimizedHost->property("live").toBool(), "minimized child makes its analysis host non-live");
    require(!minimizedHost->property("heavyActive").toBool(), "minimized child has no heavy analysis activity");

    const QString firstType = find_window(workspace.get(), firstTime)->property("viewType").toString();
    const QString secondType = find_window(workspace.get(), secondTime)->property("viewType").toString();
    const QString locusType = find_window(workspace.get(), locusOne)->property("viewType").toString();
    const QString phasorType = p2->property("viewType").toString();

    invoke(workspace.get(), "cascade");
    pump_events();
    require(find_window(workspace.get(), phasorOne)->property("windowState").toString() == QStringLiteral("minimized"),
            "Cascade leaves minimized children minimized");
    require(find_window(workspace.get(), firstTime)->property("viewType").toString() == firstType
            && find_window(workspace.get(), secondTime)->property("viewType").toString() == secondType
            && find_window(workspace.get(), locusOne)->property("viewType").toString() == locusType
            && find_window(workspace.get(), phasorTwo)->property("viewType").toString() == phasorType,
            "Cascade preserves every child analysis type");

    invoke(workspace.get(), "restoreWindow", phasorOne);
    pump_events();
    invoke(workspace.get(), "tileHorizontal");
    pump_events();
    for (int id : {firstTime, secondTime, phasorOne, locusOne, phasorTwo}) {
        QObject* child = find_window(workspace.get(), id);
        require(child->property("viewType").isValid(), "horizontal tile keeps child model intact");
        require_near(child->property("width").toDouble(), 1600.0, 0.5,
                     "horizontal tile uses complete workspace width");
    }

    invoke(workspace.get(), "tileVertical");
    pump_events();
    for (int id : {firstTime, secondTime, phasorOne, locusOne, phasorTwo}) {
        QObject* child = find_window(workspace.get(), id);
        require_near(child->property("height").toDouble(), 1200.0, 0.5,
                     "vertical tile uses complete workspace height");
    }
    require(find_window(workspace.get(), firstTime)->property("viewType").toString() == firstType
            && find_window(workspace.get(), secondTime)->property("viewType").toString() == secondType
            && find_window(workspace.get(), locusOne)->property("viewType").toString() == locusType,
            "Tile operations never mutate analysis view type");

    invoke(workspace.get(), "closeWindow", secondTime);
    pump_events();
    require(workspace->property("childCount").toInt() == 4, "closing one duplicate removes only that child");
    require(find_window(workspace.get(), secondTime) == nullptr, "closed child delegate is destroyed");
}

void verify_cursor_dimension_runtime() {
    QQmlEngine engine;
    MinimalDocument document;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/EventStrip.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());

    QVariantMap initial;
    initial.insert(QStringLiteral("document"), QVariant::fromValue(static_cast<QObject*>(&document)));
    initial.insert(QStringLiteral("width"), 520.0);
    initial.insert(QStringLiteral("height"), 46.0);
    initial.insert(QStringLiteral("axisWidth"), 170.0);
    initial.insert(QStringLiteral("viewStart"), 0.0);
    initial.insert(QStringLiteral("visibleDuration"), 1.0);
    initial.insert(QStringLiteral("cursorATime"), 0.5000);
    initial.insert(QStringLiteral("cursorBTime"), 0.5005);
    std::unique_ptr<QObject> strip(component.createWithInitialProperties(initial, engine.rootContext()));
    require(strip != nullptr, "EventStrip runtime component is created");

    QQuickWindow window;
    window.resize(520, 46);
    auto* stripItem = qobject_cast<QQuickItem*>(strip.get());
    require(stripItem != nullptr, "EventStrip is a QQuickItem");
    stripItem->setParentItem(window.contentItem());
    window.show();
    pump_events(80);

    QObject* label = find_dimension_label(strip.get());
    require(label != nullptr, "cursor dimension always exposes a millisecond label");
    require(label->property("text").toString().contains(QStringLiteral("ms")),
            "cursor dimension primary text is milliseconds");
    auto* labelItem = qobject_cast<QQuickItem*>(label);
    require(labelItem != nullptr && labelItem->isVisible(), "narrow-span millisecond label remains visible");
    QQuickItem* labelBox = labelItem->parentItem();
    QQuickItem* dimension = labelBox ? labelBox->parentItem() : nullptr;
    require(labelBox && dimension, "dimension label has an in-plot placement container");
    require(!dimension->property("labelInside").toBool(), "narrow cursor span moves label outside dimension span");
    require(labelBox->x() >= -0.5 && labelBox->x() + labelBox->width() <= dimension->width() + 0.5,
            "narrow-span dimension label stays inside plot bounds");

    strip->setProperty("cursorATime", 0.9980);
    strip->setProperty("cursorBTime", 0.9990);
    pump_events(50);
    label = find_dimension_label(strip.get());
    labelItem = qobject_cast<QQuickItem*>(label);
    labelBox = labelItem ? labelItem->parentItem() : nullptr;
    dimension = labelBox ? labelBox->parentItem() : nullptr;
    require(labelBox && dimension, "edge-position dimension label remains instantiated");
    require(labelBox->x() >= -0.5 && labelBox->x() + labelBox->width() <= dimension->width() + 0.5,
            "dimension label is clamped in bounds near the right edge");
}

void verify_dialog_runtime() {
    QQmlEngine engine;
    const QUrl topBarUrl = QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/TopBar.qml"));
    const QString source = QStringLiteral(R"QML(
import QtQuick
import QtQuick.Controls
ApplicationWindow {
    id: host
    objectName: "r4DialogWindow"
    width: 800
    height: 600
    visible: true
    Loader {
        id: barLoader
        objectName: "r4TopBarLoader"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        source: "%1"
        onLoaded: {
            item.width = width
            item.height = height
            item.hasRecord = true
            item.recordTitle = "R4 runtime fixture"
        }
    }
}
)QML").arg(topBarUrl.toString());

    QQmlComponent component(&engine);
    component.setData(source.toUtf8(), QUrl());
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());
    std::unique_ptr<QObject> root(component.create());
    require(root != nullptr, "dialog host ApplicationWindow is created");
    pump_events(80);

    auto* window = qobject_cast<QQuickWindow*>(root.get());
    require(window != nullptr, "dialog runtime host is a window");
    QObject* loader = root->findChild<QObject*>(QStringLiteral("r4TopBarLoader"));
    require(loader != nullptr, "TopBar loader exists");
    QObject* topBar = loader->property("item").value<QObject*>();
    require(topBar != nullptr, "TopBar runtime instance is loaded");

    auto verifyCentered = [&](QObject* popup, const char* message) {
        require(popup != nullptr, message);
        const double expectedX = (window->width() - popup->property("width").toDouble()) * 0.5;
        const double expectedY = (window->height() - popup->property("height").toDouble()) * 0.5;
        require_near(popup->property("x").toDouble(), expectedX, 1.5, "application popup is horizontally centered on overlay");
        require_near(popup->property("y").toDouble(), expectedY, 1.5, "application popup is vertically centered on overlay");
        require(popup->property("x").toDouble() >= -0.5
                && popup->property("y").toDouble() >= -0.5
                && popup->property("x").toDouble() + popup->property("width").toDouble() <= window->width() + 0.5
                && popup->property("y").toDouble() + popup->property("height").toDouble() <= window->height() + 0.5,
                "application popup remains inside resized window bounds");
    };

    invoke(topBar, "showAbout");
    pump_events(60);
    QObject* about = find_open_popup(root.get());
    verifyCentered(about, "About popup opens on application overlay");
    for (const auto& size : {QSize(800, 600), QSize(1280, 720), QSize(1920, 1080)}) {
        window->resize(size);
        pump_events(50);
        verifyCentered(about, "About popup remains open while window resizes");
    }
    QMetaObject::invokeMethod(about, "close");
    pump_events(30);

    invoke(topBar, "showProperties");
    pump_events(60);
    QObject* properties = find_open_popup(root.get());
    verifyCentered(properties, "Properties popup opens on application overlay");
    for (const auto& size : {QSize(900, 650), QSize(1366, 768), QSize(1600, 900)}) {
        window->resize(size);
        pump_events(50);
        verifyCentered(properties, "Properties popup remains centered after resize");
    }
}

void verify_phasor_request_quiescence() {
    qmlRegisterType<StubPhasorVectorItem>("Ardirec.Render", 1, 0, "PhasorVectorItem");

    QQmlEngine engine;
    CursorRequestProbe cursorProbe;
    MinimalDocument document;
    engine.rootContext()->setContextProperty(QStringLiteral("cursorSnapshotController"), &cursorProbe);

    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(ARDIREC_QML_DIR) + QStringLiteral("/PhasorView.qml")));
    if (component.status() != QQmlComponent::Ready)
        throw std::runtime_error(component_errors(component).toStdString());

    QVariantMap initial;
    initial.insert(QStringLiteral("document"), QVariant::fromValue(static_cast<QObject*>(&document)));
    initial.insert(QStringLiteral("requestOwner"), false);
    initial.insert(QStringLiteral("visible"), true);
    initial.insert(QStringLiteral("cursorATime"), 0.020);
    initial.insert(QStringLiteral("cursorBTime"), 0.030);
    std::unique_ptr<QObject> view(component.createWithInitialProperties(initial, engine.rootContext()));
    require(view != nullptr, "PhasorView runtime component is created");
    pump_events(60);
    require(cursorProbe.requestsA() == 0 && cursorProbe.requestsB() == 0,
            "visible duplicate Phasor consumer does not launch heavy snapshot jobs without ownership");

    view->setProperty("cursorATime", 0.021);
    view->setProperty("cursorBTime", 0.031);
    pump_events(30);
    require(cursorProbe.requestsA() == 0 && cursorProbe.requestsB() == 0,
            "cursor changes remain quiescent for non-owner duplicate view");

    view->setProperty("requestOwner", true);
    pump_events(60);
    require(cursorProbe.requestsA() == 1 && cursorProbe.requestsB() == 1,
            "request ownership activates exactly one shared C1/C2 snapshot request pair");
    view->setProperty("cursorATime", 0.022);
    pump_events(30);
    require(cursorProbe.requestsA() == 2 && cursorProbe.requestsB() == 1,
            "owner cursor change issues only the matching cursor snapshot request");

    view->setProperty("visible", false);
    view->setProperty("cursorBTime", 0.032);
    pump_events(30);
    require(cursorProbe.requestsB() == 1,
            "hidden/minimized Phasor view issues no heavy cursor snapshot jobs");
}
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("r4-test"));
    try {
        verify_mdi_runtime();
        verify_cursor_dimension_runtime();
        verify_dialog_runtime();
        verify_phasor_request_quiescence();
        std::cout << "ardirec R4 QML runtime qualification: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec R4 QML runtime qualification: FAIL: " << ex.what() << '\n';
        return 1;
    }
}

#include "test_r4_qml_runtime.moc"
