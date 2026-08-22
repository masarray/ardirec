// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QColor>
#include <QPointer>
#include <QQuickItem>

#include <cstdint>
#include <vector>

class DigitalItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int channelIndex READ channelIndex WRITE setChannelIndex NOTIFY channelChanged)
    Q_PROPERTY(double zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY viewChanged)
    Q_PROPERTY(double panFraction READ panFraction WRITE setPanFraction NOTIFY viewChanged)
    Q_PROPERTY(QColor activeColor READ activeColor WRITE setActiveColor NOTIFY activeColorChanged)
public:
    explicit DigitalItem(QQuickItem* parent = nullptr);

    QObject* document() const { return m_document.data(); }
    void setDocument(QObject* document);

    int channelIndex() const { return m_channelIndex; }
    void setChannelIndex(int value);

    double zoomFactor() const { return m_zoomFactor; }
    void setZoomFactor(double value);
    double panFraction() const { return m_panFraction; }
    void setPanFraction(double value);

    QColor activeColor() const { return m_activeColor; }
    void setActiveColor(const QColor& value);

signals:
    void documentChanged();
    void channelChanged();
    void viewChanged();
    void activeColorChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    void reloadSamples();

    QPointer<DocumentController> m_document;
    std::vector<std::uint8_t> m_samples;
    int m_channelIndex{-1};
    double m_zoomFactor{1.0};
    double m_panFraction{0.0};
    QColor m_activeColor{QStringLiteral("#d5942b")};
};
