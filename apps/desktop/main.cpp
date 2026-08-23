// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"
#include "digital_item.hpp"
#include "distance_zone_controller.hpp"
#include "document_controller.hpp"
#include "harmonic_snapshot_controller.hpp"
#include "rms_waveform_item.hpp"
#include "table_snapshot_controller.hpp"
#include "waveform_item.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ardirec"));
    QGuiApplication::setOrganizationName(QStringLiteral("ardirec"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.2.0-alpha.13"));

    qmlRegisterType<WaveformItem>("Ardirec.Render", 1, 0, "WaveformItem");
    qmlRegisterType<RmsWaveformItem>("Ardirec.Render", 1, 0, "RmsWaveformItem");
    qmlRegisterType<DigitalItem>("Ardirec.Render", 1, 0, "DigitalItem");

    DocumentController document;
    AnalysisController analysis(&document);
    HarmonicSnapshotController harmonicSnapshots(&document);
    TableSnapshotController tableSnapshots(&document);
    DistanceZoneController distanceZones;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("documentController"), &document);
    engine.rootContext()->setContextProperty(QStringLiteral("analysisController"), &analysis);
    engine.rootContext()->setContextProperty(QStringLiteral("harmonicSnapshotController"), &harmonicSnapshots);
    engine.rootContext()->setContextProperty(QStringLiteral("tableSnapshotController"), &tableSnapshots);
    engine.rootContext()->setContextProperty(QStringLiteral("distanceZoneController"), &distanceZones);
    engine.loadFromModule("Ardirec", "Main");
    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}
