// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root
    visible: false
    width: 0
    height: 0

    property var source
    property var document
    readonly property int revision: locusSnapshotController.revision
    readonly property bool busy: locusSnapshotController.busy

    function invalidate() { locusSnapshotController.invalidate() }
    function distanceLoopAvailable(loopId) { return source ? source.distanceLoopAvailable(loopId) : false }
    function distanceCurrentFloor() { return source ? source.distanceCurrentFloor() : 0.0 }
    function phaseChannel(role, phase) { return source ? source.phaseChannel(role, phase) : -1 }
    function phaseColorForName(phase) { return source ? source.phaseColorForName(phase) : "#6f7780" }

    // Compatibility helpers intentionally stay scalar/snapshot-only. Full trajectory
    // geometry is consumed natively by LocusTrajectoryItem; QML never materializes
    // one QVariantMap per locus point in the production path.
    function distanceLoopsAt(timeSeconds, kLMagnitude, kLAngle) {
        return source ? source.distanceLoopsAt(timeSeconds, kLMagnitude, kLAngle) : ({})
    }
    function impedanceAt(voltageChannel, currentChannel, timeSeconds) {
        return source ? source.impedanceAt(voltageChannel, currentChannel, timeSeconds) : ({valid:false})
    }

    function requestDistanceLocus(startSeconds, durationSeconds, maximumPoints, kLMagnitude, kLAngle) {
        if (!document || !(durationSeconds > 0)) return
        const pointBudget = Math.max(16, Math.min(4096, maximumPoints))
        locusSnapshotController.request(startSeconds, durationSeconds, pointBudget,
                                        Number(kLMagnitude), Number(kLAngle))
    }

    onSourceChanged: invalidate()
    onDocumentChanged: invalidate()
}
