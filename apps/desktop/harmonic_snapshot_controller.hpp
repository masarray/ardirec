// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_controller.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>

#include <cstddef>
#include <utility>

class HarmonicSnapshotController final : public QObject {
    Q_OBJECT
public:
    explicit HarmonicSnapshotController(DocumentController* document, QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap spectrumAt(int channelIndex,
                                       double absoluteTimeSeconds,
                                       int maximumOrder);
    Q_INVOKABLE void clearCache();

private:
    [[nodiscard]] std::pair<std::size_t, std::size_t> oneCycleWindow(double absoluteTimeSeconds) const;
    [[nodiscard]] QString cacheKey(int channelIndex,
                                   double absoluteTimeSeconds,
                                   int maximumOrder) const;

    QPointer<DocumentController> m_document;
    QHash<QString, QVariantMap> m_cache;
};
