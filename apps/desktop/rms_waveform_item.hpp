// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>

#include <atomic>
#include <memory>
#include <vector>

struct RmsGeometrySnapshot;

class RmsWaveformItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int channelIndex READ channelIndex WRITE setChannelIndex NOTIFY channelIndexChanged)
    Q_PROPERTY(QColor traceColor READ traceColor WRITE setTraceColor NOTIFY traceColorChanged)
    Q_PROPERTY(double zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY viewChanged)
    Q_PROPERTY(double panFraction READ panFraction WRITE setPanFraction NOTIFY viewChanged)
public:
    explicit RmsWaveformItem(QQuickItem* parent = nullptr);
    ~RmsWaveformItem() override;

    QObject* document() const { return m_document.data(); }
    void setDocument(QObject* document);

    int channelIndex() const { return m_channelIndex; }
    void setChannelIndex(int value);
    QColor traceColor() const { return m_traceColor; }
    void setTraceColor(const QColor& value);

    double zoomFactor() const { return m_zoomFactor; }
    void setZoomFactor(double value);
    double panFraction() const { return m_panFraction; }
    void setPanFraction(double value);

signals:
    void documentChanged();
    void channelIndexChanged();
    void traceColorChanged();
    void viewChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void reloadData();
    void refreshRepresentation();
    void scheduleRebuild();
    void startRebuild();
    void clearPreparedGeometry();

    QPointer<DocumentController> m_document;
    std::shared_ptr<const ardirec::comtrade::IndexedDatFile> m_data;
    std::shared_ptr<const std::vector<double>> m_times;
    std::shared_ptr<const RmsGeometrySnapshot> m_renderSnapshot;
    std::shared_ptr<std::atomic_bool> m_activeCancel;
    QTimer m_rebuildTimer;
    quint64 m_rebuildGeneration{0};
    int m_channelIndex{0};
    QColor m_traceColor{QStringLiteral("#406a9b")};
    double m_displayScale{1.0};
    double m_scalePeak{1.0};
    double m_nominalFrequency{50.0};
    double m_zoomFactor{1.0};
    double m_panFraction{0.0};
};
