// SPDX-License-Identifier: GPL-3.0-or-later
// R12 runtime density test. A real TimeSignalsView, seven analog lanes,
// two cursor value cells each, and four digital rows must be visible together.
// The old parent-owned 148px value made digital pickup/trip unreachable.
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QColor>
#include <QPointF>
#include <QVariant>
#include <QList>
#include <QString>
#include <QtQml/qqml.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

class StubRenderItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* document MEMBER m_document)
    Q_PROPERTY(int channelIndex MEMBER m_channelIndex)
    Q_PROPERTY(QColor traceColor MEMBER m_traceColor)
    Q_PROPERTY(QColor activeColor MEMBER m_activeColor)
    Q_PROPERTY(qreal zoomFactor MEMBER m_zoomFactor)
    Q_PROPERTY(qreal panFraction MEMBER m_panFraction)
public:
    using QQuickItem::QQuickItem;
private:
    QObject* m_document{nullptr};
    int m_channelIndex{-1};
    QColor m_traceColor;
    QColor m_activeColor;
    qreal m_zoomFactor{1.0};
    qreal m_panFraction{0.0};
};

class StubRecord final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int digitalCount READ digitalCount CONSTANT)
    Q_PROPERTY(int activeDigitalCount READ activeDigitalCount CONSTANT)
    Q_PROPERTY(double triggerOffsetSeconds READ triggerOffsetSeconds CONSTANT)
    Q_PROPERTY(double dataStartSeconds READ dataStartSeconds CONSTANT)
    Q_PROPERTY(double dataEndSeconds READ dataEndSeconds CONSTANT)
    Q_PROPERTY(double nominalFrequency READ nominalFrequency CONSTANT)
public:
    int digitalCount() const { return 4; }
    int activeDigitalCount() const { return 4; }
    double triggerOffsetSeconds() const { return 0.0; }
    double dataStartSeconds() const { return 0.0; }
    double dataEndSeconds() const { return 1.0; }
    double nominalFrequency() const { return 50.0; }
    Q_INVOKABLE QString channelName(int channel) const { return QStringLiteral("L%1").arg(channel); }
    Q_INVOKABLE QString channelUnit(int) const { return QStringLiteral("V"); }
    Q_INVOKABLE QString channelRatioText(int) const { return QStringLiteral("1:1"); }
    Q_INVOKABLE double channelPeak(int) const { return 100.0; }
    Q_INVOKABLE double sampleValue(int, double) const { return 12.0; }
    Q_INVOKABLE bool digitalIsActive(int) const { return true; }
    Q_INVOKABLE QString digitalName(int channel) const {
        return QStringLiteral("Pickup/Trip %1").arg(channel);
    }
    Q_INVOKABLE QString digitalStateText(int, double) const { return QStringLiteral("ON"); }
    Q_INVOKABLE QColor phaseColor(int channel) const {
        return channel % 3 == 0 ? QColor("#df2626")
             : channel % 3 == 1 ? QColor("#dfaa1b") : QColor("#246cdd");
    }
};

namespace {
void require(bool yes, const char* detail) {
    if (!yes) throw std::runtime_error(detail);
}
void near(qreal actual, qreal expected, const char* detail) {
    require(std::isfinite(actual) && std::abs(actual - expected) < 0.5, detail);
}
// Repeater delegates live in the Quick *visual* tree. QObject parentage may
// point at the Repeater instead, so findChildren(QObject) is not authoritative.
void collect_items(QQuickItem* parent, const QString& name, QList<QQuickItem*>& out) {
    if (!parent) return;
    if (parent->objectName() == name) out.append(parent);
    for (auto* child : parent->childItems()) collect_items(child, name, out);
}
QList<QQuickItem*> items(QQuickItem* root, const char* name) {
    QList<QQuickItem*> found;
    collect_items(root, QString::fromLatin1(name), found);
    return found;
}
QVariantList channels(std::initializer_list<int> indices) {
    QVariantList result;
    for (int index : indices) result << index;
    return result;
}
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    try {
        qmlRegisterType<StubRenderItem>("Ardirec.Render", 1, 0, "WaveformItem");
        qmlRegisterType<StubRenderItem>("Ardirec.Render", 1, 0, "RmsWaveformItem");
        qmlRegisterType<StubRenderItem>("Ardirec.Render", 1, 0, "DigitalItem");
        QQmlEngine engine;
        StubRecord record;
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(ARDIREC_QML_DIR) + "/TimeSignalsView.qml"));
        if (component.status() != QQmlComponent::Ready) {
            for (const auto& error : component.errors())
                std::cerr << error.toString().toStdString() << '\n';
            throw std::runtime_error("TimeSignalsView cannot load");
        }
        QVariantMap props{
            {"document", QVariant::fromValue(static_cast<QObject*>(&record))},
            {"analysis", QVariant::fromValue(static_cast<QObject*>(&record))},
            {"voltageChannels", channels({0, 1, 2})},
            {"currentChannels", channels({3, 4, 5, 6})},
            {"displayedDigitalChannels", channels({0, 1, 2, 3})},
            {"visibleDuration", 1.0}
        };
        std::unique_ptr<QObject> owner(component.createWithInitialProperties(props));
        require(owner != nullptr, "TimeSignalsView QML instantiates");
        auto* view = qobject_cast<QQuickItem*>(owner.get());
        require(view != nullptr, "TimeSignalsView is an item");
        view->setWidth(1580);
        view->setHeight(760);
        QCoreApplication::processEvents();
        near(view->property("analogTrackHeight").toDouble(), 72.0,
             "real QML track must use canonical compact analog height");
        near(view->property("digitalTrackHeight").toDouble(), 24.0,
             "real QML must use compact digital height");

        const auto analog = items(view, "engineeringAnalogLane");
        const auto digital = items(view, "engineeringDigitalLane");
        if (analog.size() != 7 || digital.size() != 4) {
            std::cerr << "Actual visual lanes: analog=" << analog.size()
                      << ", digital=" << digital.size()
                      << "; model voltage=" << view->property("voltageChannels").toList().size()
                      << ", current=" << view->property("currentChannels").toList().size()
                      << ", digital=" << view->property("displayedDigitalChannels").toList().size()
                      << '\n';
        }
        require(analog.size() == 7, "three voltage + four current channels instantiated");
        require(digital.size() == 4, "four digital Pickup/Trip channels instantiated");
        for (auto* lane : analog) {
            near(lane->height(), 72.0, "each analog lane is 72px, not overridden to 148");
            const auto cellAItems = items(lane, "engineeringCursorAValueCell");
            const auto cellBItems = items(lane, "engineeringCursorBValueCell");
            require(cellAItems.size() == 1 && cellBItems.size() == 1,
                    "both cursor values present in each visual lane");
            const auto* cellA = cellAItems.first();
            const auto* cellB = cellBItems.first();
            near(cellA->y(), 33.0, "C1 compact row positioned");
            near(cellB->y(), 51.0, "C2 compact row positioned");
            require(cellA->y() + cellA->height() <= cellB->y(),
                    "C1 and C2 cannot overlap");
            require(cellB->y() + cellB->height() <= lane->height() - 3.0,
                    "C2 value stays inside phase separator");
        }
        for (auto* lane : digital) near(lane->height(), 24.0,
                                      "digital row is compact and usable");
        const auto flickItems = items(view, "engineeringSignalFlick");
        require(flickItems.size() == 1, "actual Flickable exists");
        auto* flick = flickItems.first();
        // In Qt Quick, Flickable's contentHeight may include non-signal filler
        // and can be finalized asynchronously. The *visible* final digital
        // row, mapped to the actual viewport, is the engineering acceptance
        // criterion; don't conflate it with total scrollable content.
        const qreal contentHeight = flick->property("contentHeight").toDouble();
        const QPointF lastDigitalBottom = digital.last()->mapToItem(view, {0, digital.last()->height()});
        std::cout << "R12 visual geometry: contentHeight=" << contentHeight
                  << " viewport=" << view->height()
                  << " digitalBottom=" << lastDigitalBottom.y() << '\\n';
        require(flick->property("contentY").toDouble() == 0.0,
                "initial viewport starts at top without automatic scrolling");
        require(digital.last()->isVisible() && lastDigitalBottom.y() > 0.0
                && lastDigitalBottom.y() <= view->height(),
                "fourth digital Pickup/Trip signal visible on first screen");
        std::cout << "R12 compact density actual Qt runtime: PASS — seven analog + four digital\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "R12 compact density runtime: " << e.what() << '\n';
        return 1;
    }
}

#include "test_r12_compact_density.moc"
