// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: root
    visible: false
    width: 0
    height: 0

    property var source
    property var document
    property string cachedKey: ""
    property var cachedLoci: ({})
    property int revision: 0

    function invalidate() {
        cachedKey = ""
        cachedLoci = ({})
        revision += 1
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
        if (!source || !document) return []

        // Reading revision here is intentional: bindings calling distanceLocus() subscribe to it.
        // When a new COMTRADE record arrives while Locus is already active, invalidate() bumps the
        // revision and forces those bindings to ask for fresh trajectories instead of keeping an
        // empty/stale list from the previous document state.
        const revisionDependency = revision
        const pointBudget = Math.max(16, Math.min(2048, maximumPoints))
        const representation = document.valueRepresentation || "secondary"
        const key = revisionDependency + "|" + representation + "|"
                  + startSeconds.toPrecision(16) + "|" + durationSeconds.toPrecision(16)
                  + "|" + pointBudget + "|" + Number(kLMagnitude).toPrecision(12)
                  + "|" + Number(kLAngle).toPrecision(12)
        if (key !== cachedKey) {
            cachedLoci = source.distanceLoci(startSeconds, durationSeconds, pointBudget, kLMagnitude, kLAngle)
            cachedKey = key
        }
        const list = cachedLoci ? cachedLoci[loopId] : null
        return list || []
    }

    onSourceChanged: invalidate()
    onDocumentChanged: invalidate()

    Connections {
        target: root.document
        function onDocumentChanged() { root.invalidate() }
        function onRepresentationChanged() { root.invalidate() }
    }
}
