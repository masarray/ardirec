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
    double earthMaxAbsR{0.0};
    double earthMaxAbsX{0.0};
    double phaseMaxAbsR{0.0};
    double phaseMaxAbsX{0.0};
    double earthRelevantMaxAbsR{0.0};
    double earthRelevantMaxAbsX{0.0};
    double phaseRelevantMaxAbsR{0.0};
    double phaseRelevantMaxAbsX{0.0};
    double rawMaxAbsR{0.0};
    double rawMaxAbsX{0.0};
    int pointBudget{0};
    int analyzedPointCount{0};
    int statusRejectedCount{0};
};

class LocusSnapshotController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY snapshotChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double maxAbsR READ maxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double maxAbsX READ maxAbsX NOTIFY snapshotChanged)
    Q_PROPERTY(double earthMaxAbsR READ earthMaxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double earthMaxAbsX READ earthMaxAbsX NOTIFY snapshotChanged)
    Q_PROPERTY(double phaseMaxAbsR READ phaseMaxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double phaseMaxAbsX READ phaseMaxAbsX NOTIFY snapshotChanged)
    Q_PROPERTY(double earthRelevantMaxAbsR READ earthRelevantMaxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double earthRelevantMaxAbsX READ earthRelevantMaxAbsX NOTIFY snapshotChanged)
    Q_PROPERTY(double phaseRelevantMaxAbsR READ phaseRelevantMaxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double phaseRelevantMaxAbsX READ phaseRelevantMaxAbsX NOTIFY snapshotChanged)
    Q_PROPERTY(double rawMaxAbsR READ rawMaxAbsR NOTIFY snapshotChanged)
    Q_PROPERTY(double rawMaxAbsX READ rawMaxAbsX NOTIFY snapshotChanged)
    Q_PROPERTY(int analyzedPointCount READ analyzedPointCount NOTIFY snapshotChanged)
    Q_PROPERTY(int statusRejectedCount READ statusRejectedCount NOTIFY snapshotChanged)
    Q_PROPERTY(bool classicalGroundingValid READ classicalGroundingValid NOTIFY snapshotChanged)
    Q_PROPERTY(double reOverRl READ reOverRl NOTIFY snapshotChanged)
    Q_PROPERTY(double xeOverXl READ xeOverXl NOTIFY snapshotChanged)

public:
    explicit LocusSnapshotController(DocumentController* document, QObject* parent = nullptr);
    ~LocusSnapshotController() override;

    int revision() const { return m_revision; }
    bool busy() const { return m_busy; }
    double maxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->maxAbsR : 0.0; }
    double maxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->maxAbsX : 0.0; }
    double earthMaxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->earthMaxAbsR : 0.0; }
    double earthMaxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->earthMaxAbsX : 0.0; }
    double phaseMaxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->phaseMaxAbsR : 0.0; }
    double phaseMaxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->phaseMaxAbsX : 0.0; }
    double earthRelevantMaxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->earthRelevantMaxAbsR : 0.0; }
    double earthRelevantMaxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->earthRelevantMaxAbsX : 0.0; }
    double phaseRelevantMaxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->phaseRelevantMaxAbsR : 0.0; }
    double phaseRelevantMaxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->phaseRelevantMaxAbsX : 0.0; }
    double rawMaxAbsR() const { return m_nativeSnapshot ? m_nativeSnapshot->rawMaxAbsR : 0.0; }
    double rawMaxAbsX() const { return m_nativeSnapshot ? m_nativeSnapshot->rawMaxAbsX : 0.0; }
    int analyzedPointCount() const { return m_nativeSnapshot ? m_nativeSnapshot->analyzedPointCount : 0; }
    int statusRejectedCount() const { return m_nativeSnapshot ? m_nativeSnapshot->statusRejectedCount : 0; }
    bool classicalGroundingValid() const { return m_classicalGroundingValid; }
    double reOverRl() const { return m_reOverRl; }
    double xeOverXl() const { return m_xeOverXl; }

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
    bool m_classicalGroundingValid{false};
    double m_reOverRl{0.0};
    double m_xeOverXl{0.0};

    double m_lastStart{0.0};
    double m_lastDuration{0.0};
    double m_lastKMagnitude{0.0};
    double m_lastKAngle{0.0};
    int m_lastMaximumPoints{0};
    bool m_haveRequest{false};
};
