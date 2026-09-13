// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>

struct LocusSnapshotSource;

class LocusSnapshotController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY snapshotChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit LocusSnapshotController(DocumentController* document, QObject* parent = nullptr);
    ~LocusSnapshotController() override;

    int revision() const { return m_revision; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void request(double viewStartSeconds,
                             double visibleDurationSeconds,
                             int maximumPoints,
                             double groundingFactorMagnitude,
                             double groundingFactorAngleDegrees);
    Q_INVOKABLE QVariantList locus(const QString& loopId) const;
    Q_INVOKABLE QVariantMap snapshot() const { return m_snapshot; }
    Q_INVOKABLE void invalidate();

signals:
    void snapshotChanged();
    void busyChanged();

private:
    void rebuildSource();
    void cancel() noexcept;

    QPointer<DocumentController> m_document;
    std::shared_ptr<const LocusSnapshotSource> m_source;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QVariantMap m_snapshot;
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
