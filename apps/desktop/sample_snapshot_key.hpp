// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

struct SampleSnapshotKey final {
    quint64 sampleIndex{0};
    int channel{0};
    int variant{0};
    bool primary{false};

    bool operator==(const SampleSnapshotKey&) const noexcept = default;
};

[[nodiscard]] inline size_t qHash(const SampleSnapshotKey& key, size_t seed = 0) noexcept {
    seed = qHash(key.sampleIndex, seed);
    seed = qHash(key.channel, seed);
    seed = qHash(key.variant, seed);
    return qHash(key.primary, seed);
}
