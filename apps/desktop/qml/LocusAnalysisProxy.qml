// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

QtObject {
    id: root
    property var source
    property var document
    property string cachedKey: ""
    property var cachedLoci: ({})

    function invalidate() {
        cachedKey = ""
        cachedLoci = ({})
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
        const pointBudget = Math.max(16, Math.min(2048, maximumPoints))
        const representation = document.valueRepresentation || "secondary"
        const key = representation + "|" + startSeconds.toPrecision(16) + "|" + durationSeconds.toPrecision(16)
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
}
