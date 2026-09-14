// SPDX-License-Identifier: GPL-3.0-or-later
import QtQml

QtObject {
    // Protection-zone context is the default investigation frame, matching SIGRA's
    // Circle Diagrams behavior: remote finite trajectory excursions may continue
    // offscreen while the protection characteristic remains readable. When no
    // zones are loaded, fall back to the controller's robust trajectory extent.
    function zoneMagnitude(zones) {
        let r = 0.0
        let x = 0.0
        for (let zone of (zones || [])) {
            if (zone.kind === "circle") {
                r = Math.max(r, Math.abs(Number(zone.centerR)) + Math.abs(Number(zone.radius)))
                x = Math.max(x, Math.abs(Number(zone.centerX)) + Math.abs(Number(zone.radius)))
            } else if (zone.kind === "polygon" && zone.points) {
                for (let point of zone.points) {
                    if (!Number.isFinite(point.r) || !Number.isFinite(point.x)) continue
                    r = Math.max(r, Math.abs(point.r))
                    x = Math.max(x, Math.abs(point.x))
                }
            }
        }
        return {r:r, x:x}
    }

    function niceCeil(value) {
        if (!Number.isFinite(value) || value <= 0) return 1.0
        const exponent = Math.floor(Math.log(value) / Math.LN10)
        const base = Math.pow(10, exponent)
        const normalized = value / base
        const choices = [1, 1.5, 2, 2.5, 3, 4, 5, 7.5, 10]
        for (let choice of choices) if (normalized <= choice + 1e-12) return choice * base
        return 10 * base
    }

    function distanceTarget(zones, fullR, fullX, relevantR, relevantX, fitMode) {
        const zone = zoneMagnitude(zones)
        if (fitMode === "all") {
            return {
                r: niceCeil(Math.max(3.0, zone.r, Number(fullR)) * 1.08),
                x: niceCeil(Math.max(3.0, zone.x, Number(fullX)) * 1.08)
            }
        }

        const hasProtectionContext = zone.r > 0.0 || zone.x > 0.0
        const baseR = hasProtectionContext ? zone.r : Number(relevantR)
        const baseX = hasProtectionContext ? zone.x : Number(relevantX)
        return {
            r: niceCeil(Math.max(3.0, baseR) * 1.12),
            x: niceCeil(Math.max(3.0, baseX) * 1.12)
        }
    }

    function scaleFor(plotWidth, plotHeight, targetR, targetX) {
        const safeR = Math.max(1.0e-9, Number(targetR))
        const safeX = Math.max(1.0e-9, Number(targetX))
        return Math.max(1.0e-9,
                        Math.min(Number(plotWidth) / (2 * safeR),
                                 Number(plotHeight) / (2 * safeX)))
    }
}
