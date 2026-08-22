// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QPointer>
#include <QQuickItem>

#include <vector>

class WaveformItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(double zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY viewChanged)
    Q_PROPERTY(double panFraction READ panFraction WRITE setPanFraction NOTIFY viewChanged)
public:
    explicit WaveformItem(QQuickItem* parent = nullptr);

    QObject* document() const { return m_document.data(); }
    void setDocument(QObject* document);

    double zoomFactor() const { return m_zoomFactor; }
    void setZoomFactor(double value);
    double panFraction() const { return m_panFraction; }
    void setPanFraction(double value);

signals:
    void documentChanged();
    void viewChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    void reloadSamples();

    QPointer<DocumentController> m_document;
    std::vector<double> m_samples;
    double m_zoomFactor{1.0};
    double m_panFraction{0.0};
};
