// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

// Compact exact key for a sampled engineering window. Using discrete first/end
// sample indices avoids floating-point QString keys while preserving irregular
// timestamp/window-boundary behavior.
struct SampleSnapshotKey final {
    quint64 sampleIndex{0};
    quint64 windowFirst{0};
    int channel{0};
    int variant{0};
    bool primary{false};

    bool operator==(const SampleSnapshotKey&) const noexcept = default;
};

[[nodiscard]] inline size_t qHash(const SampleSnapshotKey& key, size_t seed = 0) noexcept {
    seed = qHash(key.sampleIndex, seed);
    seed = qHash(key.windowFirst, seed);
    seed = qHash(key.channel, seed);
    seed = qHash(key.variant, seed);
    return qHash(key.primary, seed);
}
