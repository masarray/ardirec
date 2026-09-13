// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

struct LocusSnapshotSource;

struct LocusNativePoint final {
    float r{0.0f};
    float x{0.0f};
    std::uint8_t valid{0};
};

struct LocusNativeSnapshot final {
    std::array<std::vector<LocusNativePoint>, 6> loops;
    std::array<std::vector<LocusNativePoint>, 3> rawPhase;
    double maxAbsR{0.0};
    double maxAbsX{0.0};
    double rawMaxAbsR{0.0};
    double rawMaxAbsX{0.0};
    int pointBudget{0};
};

class LocusSnapshotController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY snapshotChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double maxAbsR READ maxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double maxAbsX READ maxAbsX NOTIFY snapshotChanged)
    Q_PROPERTY(double rawMaxAbsR READ rawMaxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double rawMaxAbsX READ rawMaxAbsX NOTIFY snapshotChanged)

public:
    explicit LocusSnapshotController(DocumentController* document, QObject* parent = nullptr);
    ~LocusSnapshotController() override;

    int revision() const { return m_revision; }
    bool busy() const { return m_busy; }
    double maxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->maxAbsR : 0.0; }
    double maxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->maxAbsX : 0.0; }
    double rawMaxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->rawMaxAbsR : 0.0; }
    double rawMaxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->rawMaxAbsX : 0.0; }

    Q_INVOKABLE void request(double viewStartSeconds,
                             double visibleDurationSeconds,
                             int maximumPoints,
                             double groundingFactorMagnitude,
                             double groundingFactorAngleDegrees);
    // Compatibility/debug materialization only. Production locus rendering reads
    // nativeSnapshot() directly and never allocates QVariantMap per trajectory point.
    Q_INVOKABLE QVariantList locus(const QString& loopId) const;
    Q_INVOKABLE QVariantMap snapshot() const;
    Q_INVOKABLE void invalidate();

    [[nodiscard]] std::shared_ptr<const LocusNativeSnapshot> nativeSnapshot() const noexcept {
        return m_nativeSnapshot;
    }

signals:
    void snapshotChanged();
    void busyChanged();

private:
    void rebuildSource();
    void cancel() noexcept;

    QPointer<DocumentController> m_document;
    std::shared_ptr<const LocusSnapshotSource> m_source;
    std::shared_ptr<const LocusNativeSnapshot> m_nativeSnapshot;
    std::shared_ptr<std::atomic_bool> m_cancel;
    quint64 m_generation{0};
    int m_revision{0};
    bool m_busy{false};

    double m_lastStart{0.0};
    double m_lastDuration{0.0};
    double m_lastKMagnitude{0.0};
    double m_lastKAngle{0.0};
    int m_lastMaximumPoints{0};
    bool m_haveRequest{false};
};
