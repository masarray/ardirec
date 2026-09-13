// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"
#include "sample_snapshot_key.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstddef>
#include <utility>

class TableSnapshotController final : public QObject {
    Q_OBJECT
public:
    explicit TableSnapshotController(DocumentController* document, QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap snapshotAt(int channelIndex, double absoluteTimeSeconds);
    Q_INVOKABLE QVariantList sortedChannels(const QVariantList& channelIndexes,
                                            double absoluteTimeSeconds,
                                            const QString& sortMode,
                                            bool abnormalOnly = false);
    Q_INVOKABLE QVariantMap summaryAt(const QVariantList& channelIndexes,
                                      double absoluteTimeSeconds);
    Q_INVOKABLE void clearCache();

private:
    struct CacheEntry final {
        QVariantMap value;
        double referenceTime{0.0};
        quint64 touch{0};
    };

    [[nodiscard]] std::pair<std::size_t, std::size_t> oneCycleWindow(double absoluteTimeSeconds) const;
    [[nodiscard]] SampleSnapshotKey cacheKey(int channelIndex, double absoluteTimeSeconds) const;
    [[nodiscard]] QString channelPhase(int channelIndex) const;
    [[nodiscard]] QVariantMap adjustedForReference(const CacheEntry& entry,
                                                   int channelIndex,
                                                   double absoluteTimeSeconds) const;
    void trimCache();

    QPointer<DocumentController> m_document;
    QHash<SampleSnapshotKey, CacheEntry> m_cache;
    quint64 m_touchCounter{0};
    int m_maxCacheEntries{512};
};
