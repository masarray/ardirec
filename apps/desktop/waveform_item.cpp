// SPDX-License-Identifier: GPL-3.0-or-later
#include "waveform_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>
#include <limits>

WaveformItem::WaveformItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void WaveformItem::setDocument(QObject* document) {
    auto* controller = qobject_cast<DocumentController*>(document);
    if (m_document == controller) return;
    if (m_document) disconnect(m_document, nullptr, this, nullptr);
    m_document = controller;
    if (m_document) {
        connect(m_document, &DocumentController::documentChanged, this, &WaveformItem::reloadData);
        connect(m_document, &DocumentController::representationChanged, this, &WaveformItem::refreshRepresentation);
    }
    reloadData();
    emit documentChanged();
}

void WaveformItem::setChannelIndex(int value) {
    if (m_channelIndex == value) return;
    m_channelIndex = value;
    reloadData();
    emit channelIndexChanged();
}

void WaveformItem::setTraceColor(const QColor& value) {
    if (m_traceColor == value) return;
    m_traceColor = value;
    update();
    emit traceColorChanged();
}

void WaveformItem::setZoomFactor(double value) {
    value = std::clamp(value, 1.0, 500.0);
    if (qFuzzyCompare(m_zoomFactor, value)) return;
    m_zoomFactor = value;
    m_panFraction = std::clamp(m_panFraction, 0.0, 1.0);
    update();
    emit viewChanged();
}

void WaveformItem::setPanFraction(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_panFraction, value)) return;
    m_panFraction = value;
    update();
    emit viewChanged();
}

void WaveformItem::reloadData() {
    if (!m_document) {
        m_data.reset();
        m_times.reset();
        m_lod.reset();
        m_displayScale = 1.0;
    } else {
        m_data = m_document->dataStoreSnapshot();
        m_times = m_document->timeIndexSnapshot();
        m_lod = m_document->analogLodSnapshot();
        m_displayScale = m_document->channelDisplayScale(m_channelIndex);
    }
    update();
}

void WaveformItem::refreshRepresentation() {
    m_displayScale = m_document ? m_document->channelDisplayScale(m_channelIndex) : 1.0;
    update();
}

QSGNode* WaveformItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    const auto data = m_data;
    const auto times = m_times;
    const auto lod = m_lod;
    const std::size_t count = data && times ? std::min(data->frameCount(), times->size()) : 0;
    if (count < 2 || m_channelIndex < 0
        || static_cast<std::size_t>(m_channelIndex) >= (data ? data->analogCount() : 0)
        || width() <= 1.0 || height() <= 1.0) {
        delete node;
        return nullptr;
    }

    const double dataStart = times->front();
    const double dataEnd = (*times)[count - 1];
    const double fullDuration = std::max(0.0, dataEnd - dataStart);
    if (fullDuration <= 0.0) {
        delete node;
        return nullptr;
    }

    const double visibleDuration = fullDuration / std::clamp(m_zoomFactor, 1.0, 500.0);
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double startTime = dataStart + std::clamp(m_panFraction, 0.0, 1.0) * movable;
    const double endTime = startTime + visibleDuration;

    auto first = std::lower_bound(times->begin(), times->begin() + static_cast<std::ptrdiff_t>(count), startTime);
    auto last = std::upper_bound(times->begin(), times->begin() + static_cast<std::ptrdiff_t>(count), endTime);
    std::size_t start = static_cast<std::size_t>(std::distance(times->begin(), first));
    std::size_t end = static_cast<std::size_t>(std::distance(times->begin(), last));
    start = std::min(start, count - 1);
    end = std::min(end, count);
    if (end <= start + 1) end = std::min(count, start + 2);
    const std::size_t visibleCount = end - start;
    const std::size_t channel = static_cast<std::size_t>(m_channelIndex);

    const std::size_t pixelWidth = std::clamp<std::size_t>(static_cast<std::size_t>(width()), 64, 4096);
    const bool envelopeMode = visibleCount > pixelWidth * 8u;
    const std::size_t samplesPerPixel = std::max<std::size_t>(1u, visibleCount / pixelWidth);
    const bool lodCompatible = envelopeMode && lod && lod->block_size > 0
                               && channel < lod->channel_count
                               && lod->block_size <= samplesPerPixel * 2u;

    double peak = 0.0;
    std::size_t pointCount = 0;
    std::size_t stride = 1;

    if (envelopeMode) {
        pointCount = pixelWidth * 2u;
        m_bucketLows.assign(pixelWidth, std::numeric_limits<double>::infinity());
        m_bucketHighs.assign(pixelWidth, -std::numeric_limits<double>::infinity());

        const auto accumulate = [&](std::size_t representativeIndex, double rawLow, double rawHigh) {
            if (representativeIndex >= count || !std::isfinite(rawLow) || !std::isfinite(rawHigh)) return;
            double low = rawLow * m_displayScale;
            double high = rawHigh * m_displayScale;
            if (low > high) std::swap(low, high);
            if (!std::isfinite(low) || !std::isfinite(high)) return;
            const double fraction = std::clamp(((*times)[representativeIndex] - startTime) / visibleDuration, 0.0, 1.0);
            const std::size_t bucket = std::min(pixelWidth - 1u,
                                                static_cast<std::size_t>(fraction * static_cast<double>(pixelWidth - 1u)));
            m_bucketLows[bucket] = std::min(m_bucketLows[bucket], low);
            m_bucketHighs[bucket] = std::max(m_bucketHighs[bucket], high);
            peak = std::max(peak, std::max(std::abs(low), std::abs(high)));
        };

        const auto accumulateRaw = [&](std::size_t firstIndex, std::size_t lastIndex) {
            for (std::size_t i = firstIndex; i < lastIndex; ++i) {
                const double value = data->analogValue(i, channel);
                if (std::isfinite(value)) accumulate(i, value, value);
            }
        };

        if (lodCompatible) {
            const std::size_t blockSize = lod->block_size;
            const std::size_t firstFullBlock = (start + blockSize - 1u) / blockSize;
            const std::size_t lastFullBlockExclusive = end / blockSize;

            if (firstFullBlock < lastFullBlockExclusive) {
                const std::size_t prefixEnd = std::min(end, firstFullBlock * blockSize);
                accumulateRaw(start, prefixEnd);

                for (std::size_t block = firstFullBlock; block < lastFullBlockExclusive; ++block) {
                    double low = 0.0;
                    double high = 0.0;
                    if (!lod->blockExtrema(channel, block, low, high)) continue;
                    const std::size_t blockStart = block * blockSize;
                    const std::size_t representative = std::min(count - 1u,
                                                                 blockStart + (blockSize - 1u) / 2u);
                    accumulate(representative, low, high);
                }

                const std::size_t suffixStart = std::max(start, lastFullBlockExclusive * blockSize);
                accumulateRaw(suffixStart, end);
            } else {
                accumulateRaw(start, end);
            }
        } else {
            accumulateRaw(start, end);
        }
    } else {
        const std::size_t targetPoints = std::max<std::size_t>(64u, pixelWidth * 2u);
        stride = std::max<std::size_t>(1u, (visibleCount + targetPoints - 1u) / targetPoints);
        pointCount = (visibleCount + stride - 1u) / stride;
        pointCount = std::max<std::size_t>(2u, pointCount);
        for (std::size_t i = start; i < end; ++i) {
            const double value = data->analogValue(i, channel) * m_displayScale;
            if (std::isfinite(value)) peak = std::max(peak, std::abs(value));
        }
    }

    if (!std::isfinite(peak) || peak < 1.0e-12) peak = 1.0;
    peak *= 1.08;

    if (!node) {
        node = new QSGGeometryNode;
        auto* material = new QSGFlatColorMaterial;
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
    material->setColor(m_traceColor);
    node->markDirty(QSGNode::DirtyMaterial);

    auto* geometry = node->geometry();
    if (!geometry || static_cast<std::size_t>(geometry->vertexCount()) != pointCount) {
        auto* replacement = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(pointCount));
        node->setGeometry(replacement);
        node->setFlag(QSGNode::OwnsGeometry);
        geometry = replacement;
    }
    geometry->setDrawingMode(envelopeMode ? QSGGeometry::DrawLines : QSGGeometry::DrawLineStrip);

    auto* vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    const auto xForTime = [&](double value) {
        const double fraction = std::clamp((value - startTime) / visibleDuration, 0.0, 1.0);
        return static_cast<float>(fraction * static_cast<double>(w));
    };
    const auto yFor = [&](double value) {
        const double normalized = std::clamp(value / peak, -1.0, 1.0);
        return static_cast<float>(static_cast<double>(h) * (0.5 - normalized * 0.43));
    };

    if (!envelopeMode) {
        std::size_t out = 0;
        for (std::size_t i = start; i < end && out < pointCount; i += stride) {
            const double value = data->analogValue(i, channel) * m_displayScale;
            vertices[out++].set(xForTime((*times)[i]), std::isfinite(value) ? yFor(value) : h * 0.5F);
        }
        while (out < pointCount) {
            const std::size_t i = end - 1u;
            const double value = data->analogValue(i, channel) * m_displayScale;
            vertices[out++].set(xForTime((*times)[i]), std::isfinite(value) ? yFor(value) : h * 0.5F);
        }
    } else {
        double previous = 0.0;
        for (std::size_t bucket = 0; bucket < pixelWidth; ++bucket) {
            double low = m_bucketLows[bucket];
            double high = m_bucketHighs[bucket];
            if (!std::isfinite(low) || !std::isfinite(high)) {
                low = previous;
                high = previous;
            } else {
                previous = (low + high) * 0.5;
            }
            const float x = pixelWidth > 1u
                                ? w * static_cast<float>(bucket) / static_cast<float>(pixelWidth - 1u)
                                : 0.0F;
            vertices[bucket * 2u].set(x, yFor(low));
            vertices[bucket * 2u + 1u].set(x, yFor(high));
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
