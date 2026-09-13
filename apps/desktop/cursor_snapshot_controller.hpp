// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <atomic>
#include <memory>

struct CursorSnapshotSource;

class CursorSnapshotController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap cursorA READ cursorA NOTIFY cursorAChanged)
    Q_PROPERTY(QVariantMap cursorB READ cursorB NOTIFY cursorBChanged)
    Q_PROPERTY(bool busyA READ busyA NOTIFY busyAChanged)
    Q_PROPERTY(bool busyB READ busyB NOTIFY busyBChanged)

public:
    explicit CursorSnapshotController(DocumentController* document, QObject* parent = nullptr);
    ~CursorSnapshotController() override;

    QVariantMap cursorA() const { return m_cursorA; }
    QVariantMap cursorB() const { return m_cursorB; }
    bool busyA() const { return m_busyA; }
    bool busyB() const { return m_busyB; }

    Q_INVOKABLE void requestCursorA(double absoluteTimeSeconds);
    Q_INVOKABLE void requestCursorB(double absoluteTimeSeconds);

    // Distance math consumes the already-computed fundamental V/I phasors from a
    // cursor snapshot. No DAT traversal or DFT is performed by this call.
    Q_INVOKABLE QVariantMap distanceLoopsForSnapshot(const QVariantMap& snapshot,
                                                     double groundingFactorMagnitude,
                                                     double groundingFactorAngleDegrees) const;

signals:
    void cursorAChanged();
    void cursorBChanged();
    void busyAChanged();
    void busyBChanged();

private:
    void rebuildSource();
    void request(int cursor, double absoluteTimeSeconds);
    void cancel(int cursor) noexcept;
    void publish(int cursor, quint64 generation, const QVariantMap& snapshot);

    QPointer<DocumentController> m_document;
    std::shared_ptr<const CursorSnapshotSource> m_source;
    std::shared_ptr<std::atomic_bool> m_cancelA;
    std::shared_ptr<std::atomic_bool> m_cancelB;
    QVariantMap m_cursorA;
    QVariantMap m_cursorB;
    quint64 m_generationA{0};
    quint64 m_generationB{0};
    double m_lastTimeA{0.0};
    double m_lastTimeB{0.0};
    bool m_haveLastA{false};
    bool m_haveLastB{false};
    bool m_busyA{false};
    bool m_busyB{false};
};
