// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickItem>
#include <QVariantList>

class PhasorVectorItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList vectors READ vectors WRITE setVectors NOTIFY vectorsChanged)
    Q_PROPERTY(double scaleMagnitude READ scaleMagnitude WRITE setScaleMagnitude NOTIFY scaleMagnitudeChanged)

public:
    explicit PhasorVectorItem(QQuickItem* parent = nullptr);

    QVariantList vectors() const { return m_vectors; }
    void setVectors(const QVariantList& vectors);

    double scaleMagnitude() const { return m_scaleMagnitude; }
    void setScaleMagnitude(double value);

signals:
    void vectorsChanged();
    void scaleMagnitudeChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    QVariantList m_vectors;
    double m_scaleMagnitude{0.0};
};
