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

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

struct TableFrameSource;

class TableSnapshotController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap frame READ frame NOTIFY frameChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit TableSnapshotController(DocumentController* document, QObject* parent = nullptr);
    ~TableSnapshotController() override;

    QVariantMap frame() const { return m_frame; }
    bool busy() const { return m_busy; }

    // R5.3 table presentation consumes one immutable, asynchronously calculated
    // frame. Requests are coalesced to one in-flight calculation plus one latest
    // pending target, so cursor scrubbing never fans out unbounded work.
    Q_INVOKABLE void requestFrame(const QVariantList& channelIndexes,
                                  double absoluteTimeSeconds,
                                  const QString& sortMode,
                                  bool abnormalOnly = false);

    // Legacy synchronous APIs remain available for focused callers/tests, but
    // ValueTableView must not call them from QML bindings or delegates.
    Q_INVOKABLE QVariantMap snapshotAt(int channelIndex, double absoluteTimeSeconds);
    Q_INVOKABLE QVariantList sortedChannels(const QVariantList& channelIndexes,
                                            double absoluteTimeSeconds,
                                            const QString& sortMode,
                                            bool abnormalOnly = false);
    Q_INVOKABLE QVariantMap summaryAt(const QVariantList& channelIndexes,
                                      double absoluteTimeSeconds);
    Q_INVOKABLE void clearCache();

signals:
    void frameChanged();
    void busyChanged();

private:
    struct CacheEntry final {
        QVariantMap value;
        double referenceTime{0.0};
        quint64 touch{0};
    };

    struct FrameRequest final {
        std::vector<int> channels;
        double absoluteTimeSeconds{0.0};
        QString sortMode{QStringLiteral("record")};
        bool abnormalOnly{false};
    };

    [[nodiscard]] std::pair<std::size_t, std::size_t> oneCycleWindow(double absoluteTimeSeconds) const;
    [[nodiscard]] SampleSnapshotKey cacheKey(int channelIndex, double absoluteTimeSeconds) const;
    [[nodiscard]] QString channelPhase(int channelIndex) const;
    [[nodiscard]] QVariantMap adjustedForReference(const CacheEntry& entry,
                                                   int channelIndex,
                                                   double absoluteTimeSeconds) const;
    void trimCache();

    void rebuildFrameSource(bool preserveLastRequest);
    void cancelFrameWork(bool clearBusy = true) noexcept;
    void launchFrameRequest(const FrameRequest& request);
    void completeFrameRequest(quint64 generation,
                              const FrameRequest& request,
                              const std::shared_ptr<std::atomic_bool>& cancelToken,
                              const QVariantMap& result);
    [[nodiscard]] FrameRequest makeFrameRequest(const QVariantList& channelIndexes,
                                                double absoluteTimeSeconds,
                                                const QString& sortMode,
                                                bool abnormalOnly) const;
    [[nodiscard]] static bool sameFrameRequest(const FrameRequest& lhs,
                                               const FrameRequest& rhs) noexcept;

    QPointer<DocumentController> m_document;
    QHash<SampleSnapshotKey, CacheEntry> m_cache;
    quint64 m_touchCounter{0};
    int m_maxCacheEntries{512};

    std::shared_ptr<const TableFrameSource> m_frameSource;
    std::shared_ptr<std::atomic_bool> m_frameCancel;
    QVariantMap m_frame;
    std::optional<FrameRequest> m_activeFrameRequest;
    std::optional<FrameRequest> m_pendingFrameRequest;
    std::optional<FrameRequest> m_lastFrameRequest;
    std::optional<FrameRequest> m_committedFrameRequest;
    quint64 m_frameGeneration{0};
    bool m_busy{false};
};
