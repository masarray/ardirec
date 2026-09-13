// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root
    visible: false
    width: 0
    height: 0

    property var source
    property var document
    // LocusSnapshotController publishes only completed latest-generation snapshots.
    // Bindings subscribe to revision; no heavy locus calculation runs in this QML object.
    readonly property int revision: locusSnapshotController.revision
    readonly property bool busy: locusSnapshotController.busy

    function invalidate() {
        locusSnapshotController.invalidate()
    }

    function distanceLoopAvailable(loopId) {
        return source ? source.distanceLoopAvailable(loopId) : false
    }
    function distanceCurrentFloor() {
        return source ? source.distanceCurrentFloor() : 0.0
    }
    function distanceLoopsAt(timeSeconds, kLMagnitude, kLAngle) {
        return source ? source.distanceLoopsAt(timeSeconds, kLMagnitude, kLAngle) : ({})
    }
    function phaseChannel(role, phase) {
        return source ? source.phaseChannel(role, phase) : -1
    }
    function impedanceAt(voltageChannel, currentChannel, timeSeconds) {
        return source ? source.impedanceAt(voltageChannel, currentChannel, timeSeconds) : ({valid:false})
    }
    function phaseColorForName(phase) {
        return source ? source.phaseColorForName(phase) : "#6f7780"
    }

    function distanceLocus(loopId, startSeconds, durationSeconds, maximumPoints, kLMagnitude, kLAngle) {
        if (!source || !document || !(durationSeconds > 0)) return []
        // Geometry budget is tied to display needs, never to record sample count.
        const pointBudget = Math.max(16, Math.min(2048, maximumPoints))
        locusSnapshotController.request(startSeconds, durationSeconds, pointBudget,
                                        Number(kLMagnitude), Number(kLAngle))
        const revisionDependency = root.revision
        return locusSnapshotController.locus(loopId) || []
    }

    onSourceChanged: invalidate()
    onDocumentChanged: invalidate()
}
