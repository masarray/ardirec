// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QString>

class LocusSnapshotController;

class LocusTrajectoryItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool rawPhase READ rawPhase WRITE setRawPhase NOTIFY geometryChanged)
    Q_PROPERTY(int firstLoop READ firstLoop WRITE setFirstLoop NOTIFY geometryChanged)
    Q_PROPERTY(int loopCount READ loopCount WRITE setLoopCount NOTIFY geometryChanged)
    Q_PROPERTY(int visibilityMask READ visibilityMask WRITE setVisibilityMask NOTIFY geometryChanged)
    Q_PROPERTY(QString selectedLoop READ selectedLoop WRITE setSelectedLoop NOTIFY geometryChanged)
    Q_PROPERTY(double rHalf READ rHalf WRITE setRHalf NOTIFY geometryChanged)
    Q_PROPERTY(double xHalf READ xHalf WRITE setXHalf NOTIFY geometryChanged)

public:
    explicit LocusTrajectoryItem(QQuickItem* parent = nullptr);

    QObject* source() const { return m_source.data(); }
    void setSource(QObject* value);
    bool rawPhase() const { return m_rawPhase; }
    void setRawPhase(bool value);
    int firstLoop() const { return m_firstLoop; }
    void setFirstLoop(int value);
    int loopCount() const { return m_loopCount; }
    void setLoopCount(int value);
    int visibilityMask() const { return m_visibilityMask; }
    void setVisibilityMask(int value);
    QString selectedLoop() const { return m_selectedLoop; }
    void setSelectedLoop(const QString& value);
    double rHalf() const { return m_rHalf; }
    void setRHalf(double value);
    double xHalf() const { return m_xHalf; }
    void setXHalf(double value);

signals:
    void sourceChanged();
    void geometryChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    QPointer<LocusSnapshotController> m_source;
    bool m_rawPhase{false};
    int m_firstLoop{0};
    int m_loopCount{3};
    int m_visibilityMask{0x3f};
    QString m_selectedLoop;
    double m_rHalf{1.0};
    double m_xHalf{1.0};
};
