// SPDX-License-Identifier: GPL-3.0-or-later
#include "cursor_snapshot_controller.hpp"
#include "document_controller.hpp"
#include "harmonic_snapshot_controller.hpp"
#include "locus_snapshot_controller.hpp"
#include "table_snapshot_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QUrl>

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void pump_events(int durationMs) {
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
    require(!document.loading(), "background COMTRADE load completes before timeout");
    require(document.error().isEmpty(), "COMTRADE fixture opens without error");
}

void require_closed_state(const DocumentController& document) {
    require(!document.loading(), "closed document is not loading");
    require(document.loadingStatus().isEmpty(), "closed document clears loading status");
    require(document.title() == QStringLiteral("No record open"), "closed document restores workstation title");
    require(document.metadata() == QStringLiteral("Open a COMTRADE CFG to begin"), "closed document restores empty metadata");
    require(document.sampleCount() == 0, "closed document releases the time index");
    require(document.analogCount() == 0, "closed document clears analog channels");
    require(document.digitalCount() == 0, "closed document clears digital channels");
    require(document.activeDigitalCount() == 0, "closed document clears active digital state");
    require(document.selectedAnalogIndex() == -1, "closed document clears signal selection");
    require(document.selectedSignal() == QStringLiteral("No signal"), "closed document restores no-signal state");
    require(document.diagnostics().isEmpty(), "closed document clears diagnostics");
    require(document.distanceZonePath().isEmpty(), "closed document clears distance-zone sidecar");
    require(document.headerSourceName().isEmpty() && document.headerText().isEmpty(), "closed document clears HDR sidecar state");
    require(document.valueRepresentation() == QStringLiteral("secondary"), "closed document restores secondary representation");
    require(!document.transformerRatiosAvailable(), "closed document clears transformer metadata");
    require(document.dataStoreSnapshot() == nullptr, "closed document releases DAT store/mapping ownership");
    require(document.analogLodPyramidSnapshot() == nullptr, "closed document releases analog LOD cache");
    require(document.rmsTileCacheSnapshot() == nullptr, "closed document releases RMS cache");
    require(document.timeSeconds().empty(), "closed document exposes no stale timestamps");
    require(document.digitalEdgeTimes().empty(), "closed document exposes no stale event edges");
    require(document.error().isEmpty(), "normal close is not reported as an error");
}

void require_analysis_invalidated(const CursorSnapshotController& cursor,
                                  const LocusSnapshotController& locus) {
    require(!cursor.busyA() && !cursor.busyB(), "close cancels active cursor snapshot workers");
    require(cursor.cursorA().isEmpty() && cursor.cursorB().isEmpty(), "close clears committed cursor snapshots");
    require(!locus.busy(), "close cancels active locus worker");
    require(locus.nativeSnapshot() == nullptr, "close clears native locus snapshot ownership");
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        const std::filesystem::path cfgPath = std::filesystem::path(ARDIREC_TEST_DATA_DIR) / "distance_sigra_parity.cfg";
        const QUrl cfgUrl = QUrl::fromLocalFile(QString::fromStdString(cfgPath.string()));

        DocumentController document;
        CursorSnapshotController cursor(&document);
        LocusSnapshotController locus(&document);
        HarmonicSnapshotController harmonic(&document);
        TableSnapshotController table(&document);

        document.openCfg(cfgUrl);
        wait_for_document(document);
        require(document.sampleCount() == 80, "lifecycle fixture opens before close");
        require(document.dataStoreSnapshot() != nullptr, "open document owns a DAT store");
        require(document.analogLodPyramidSnapshot() != nullptr, "open document owns an analog LOD cache");
        require(document.rmsTileCacheSnapshot() != nullptr, "open document owns an RMS cache");
        require(!document.distanceZonePath().isEmpty(), "lifecycle fixture discovers its RIO sidecar");

        // Warm the bounded scalar caches before close. They are synchronously tied to
        // documentChanged and must never preserve values from a closed record.
        require(!harmonic.spectrumAt(0, 0.025, 15).isEmpty(), "harmonic cache is populated before close");
        require(!table.snapshotAt(0, 0.025).isEmpty(), "engineering table cache is populated before close");

        document.closeDocument();
        require_closed_state(document);
        require_analysis_invalidated(cursor, locus);
        require(harmonic.spectrumAt(0, 0.025, 15).isEmpty(), "harmonic cache cannot serve a closed document");
        require(table.snapshotAt(0, 0.025).isEmpty(), "table cache cannot serve a closed document");

        // Generation invalidation is deterministic even if close happens before the
        // background load callback gets an event-loop turn. A stale loader result
        // must never resurrect the closed record.
        document.openCfg(cfgUrl);
        require(document.loading(), "second load enters background loading state");
        document.closeDocument();
        require_closed_state(document);
        pump_events(250);
        require_closed_state(document);
        require_analysis_invalidated(cursor, locus);

        // R4 worker-lifetime qualification: start real cursor and locus jobs, then
        // close immediately before either queued result can be published. Both
        // workers must retire and their old-generation results must stay discarded.
        document.openCfg(cfgUrl);
        wait_for_document(document);
        cursor.requestCursorA(0.040);
        cursor.requestCursorB(0.055);
        const int locusRevisionBefore = locus.revision();
        locus.request(document.dataStartSeconds(), document.durationSeconds(), 4000, 0.0, 0.0);
        require(cursor.busyA() || cursor.busyB() || locus.busy(),
                "at least one asynchronous analysis worker is active before close");
        document.closeDocument();
        require_closed_state(document);
        require_analysis_invalidated(cursor, locus);
        pump_events(350);
        require_closed_state(document);
        require_analysis_invalidated(cursor, locus);
        require(locus.revision() == locusRevisionBefore,
                "stale pre-close locus callback cannot publish a new revision after document close");

        std::cout << "ardirec document lifecycle tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec document lifecycle tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
