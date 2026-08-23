// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QColor>
#include <QPointer>
#include <QQuickItem>

#include <vector>

class RmsWaveformItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int channelIndex READ channelIndex WRITE setChannelIndex NOTIFY channelIndexChanged)
    Q_PROPERTY(QColor traceColor READ traceColor WRITE setTraceColor NOTIFY traceColorChanged)
    Q_PROPERTY(double zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY viewChanged)
    Q_PROPERTY(double panFraction READ panFraction WRITE setPanFraction NOTIFY viewChanged)
public:
    explicit RmsWaveformItem(QQuickItem* parent = nullptr);

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

private:
    void reloadSamples();
    void rebuildRms();
    void refreshRepresentation();

    QPointer<DocumentController> m_document;
    int m_channelIndex{0};
    QColor m_traceColor{QStringLiteral("#406a9b")};
    std::vector<double> m_samples;
    std::vector<double> m_times;
    std::vector<double> m_rmsSamples;
    double m_displayScale{1.0};
    double m_zoomFactor{1.0};
    double m_panFraction{0.0};
};
