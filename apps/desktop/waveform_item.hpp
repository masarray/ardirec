// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickItem>

class WaveformItem final : public QQuickItem {
    Q_OBJECT
public:
    explicit WaveformItem(QQuickItem* parent = nullptr);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
};
