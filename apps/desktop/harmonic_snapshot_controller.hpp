// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"
#include "sample_snapshot_key.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <cstddef>
#include <utility>

class HarmonicSnapshotController final : public QObject {
    Q_OBJECT
public:
    explicit HarmonicSnapshotController(DocumentController* document, QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap spectrumAt(int channelIndex,
                                       double absoluteTimeSeconds,
                                       int maximumOrder);
    Q_INVOKABLE void clearCache();

private:
    struct CacheEntry final {
        QVariantMap value;
        double referenceTime{0.0};
        quint64 touch{0};
    };

    [[nodiscard]] std::pair<std::size_t, std::size_t> oneCycleWindow(double absoluteTimeSeconds) const;
    [[nodiscard]] SampleSnapshotKey cacheKey(int channelIndex,
                                             double absoluteTimeSeconds,
                                             int maximumOrder) const;
    [[nodiscard]] QVariantMap adjustedForReference(const CacheEntry& entry,
                                                   double absoluteTimeSeconds) const;
    void trimCache();

    QPointer<DocumentController> m_document;
    QHash<SampleSnapshotKey, CacheEntry> m_cache;
    quint64 m_touchCounter{0};
    int m_maxCacheEntries{256};
};
