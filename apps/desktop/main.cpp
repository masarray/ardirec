// SPDX-License-Identifier: GPL-3.0-or-later
#include "analysis_controller.hpp"
#include "digital_item.hpp"
#include "distance_zone_controller.hpp"
#include "document_controller.hpp"
#include "harmonic_snapshot_controller.hpp"
#include "rms_waveform_item.hpp"
#include "table_snapshot_controller.hpp"
#include "waveform_item.hpp"

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <qqml.h>

namespace {
struct StartupRequest {
    QString cfgPath;
    bool hostedByArsas{false};
};

StartupRequest startup_request(int argc, char* argv[]) {
    if (argc == 2) {
        const QString candidate = QString::fromLocal8Bit(argv[1]).trimmed();
        if (!candidate.startsWith('-')) return {candidate, false};
    }

    if (argc >= 3) {
        const QString mode = QString::fromLatin1(argv[1]);
        const QString candidate = QString::fromLocal8Bit(argv[2]).trimmed();
        if (mode == QStringLiteral("--open")) return {candidate, false};
        if (mode == QStringLiteral("--arsas-open")) return {candidate, true};
    }

    return {};
}
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    const StartupRequest startup = startup_request(argc, argv);

    QGuiApplication::setApplicationName(startup.hostedByArsas
                                            ? QStringLiteral("ARSAS COMTRADE Viewer")
                                            : QStringLiteral("ardirec"));
    QGuiApplication::setOrganizationName(QStringLiteral("ardirec"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.2.0-alpha.14"));

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

    if (startup.hostedByArsas) {
        engine.rootObjects().first()->setProperty("title", QStringLiteral("ARSAS — COMTRADE Viewer"));
    }

    if (!startup.cfgPath.isEmpty()) {
        const QFileInfo cfgInfo(startup.cfgPath);
        document.openCfg(QUrl::fromLocalFile(cfgInfo.absoluteFilePath()));
    }

    return app.exec();
}
