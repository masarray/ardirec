// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"
#include "ardirec/comtrade/value_representation.hpp"

#include <QDate>
#include <QDateTime>
#include <QTime>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>

namespace {
constexpr std::size_t kViewerAlphaFrameLimit = 500'000;

std::optional<double> parse_comtrade_timestamp(const std::string& raw) {
    const QString text = QString::fromStdString(raw).trimmed();
    const QStringList halves = text.split(',');
    if (halves.size() < 2) return std::nullopt;

    const QStringList date = halves.at(0).trimmed().split('/');
    const QStringList time = halves.at(1).trimmed().split(':');
    if (date.size() != 3 || time.size() != 3) return std::nullopt;

    bool okDay = false;
    bool okMonth = false;
    bool okYear = false;
    bool okHour = false;
    bool okMinute = false;
    bool okSecond = false;
    int day = date.at(0).toInt(&okDay);
    int month = date.at(1).toInt(&okMonth);
    int year = date.at(2).toInt(&okYear);
    const int hour = time.at(0).toInt(&okHour);
    const int minute = time.at(1).toInt(&okMinute);
    const double seconds = time.at(2).toDouble(&okSecond);
    if (!okDay || !okMonth || !okYear || !okHour || !okMinute || !okSecond) return std::nullopt;

    if (year < 100) year += year >= 70 ? 1900 : 2000;
    const int wholeSecond = std::clamp(static_cast<int>(std::floor(seconds)), 0, 59);
    const double fraction = std::max(0.0, seconds - static_cast<double>(wholeSecond));
    const QDate qDate(year, month, day);
    const QTime qTime(hour, minute, wholeSecond);
    if (!qDate.isValid() || !qTime.isValid()) return std::nullopt;

    const QDateTime dateTime(qDate, qTime, Qt::UTC);
    return static_cast<double>(dateTime.toMSecsSinceEpoch()) / 1000.0 + fraction;
}

double nice_peak(double peak) {
    if (!std::isfinite(peak) || peak <= 1.0e-12) return 1.0;
    const double exponent = std::floor(std::log10(peak));
    const double base = std::pow(10.0, exponent);
    const double normalized = peak / base;
    double step = 10.0;
    if (normalized <= 1.0) step = 1.0;
    else if (normalized <= 2.0) step = 2.0;
    else if (normalized <= 5.0) step = 5.0;
    return step * base;
}

QString normalized_unit(QString unit) {
    unit = unit.trimmed().toUpper();
    unit.remove(' ');
    return unit;
}

QString compact_ratio_value(double value) {
    if (!std::isfinite(value)) return QStringLiteral("—");
    return QString::number(value, 'g', 7);
}
} // namespace

double DocumentController::durationSeconds() const {
    if (m_timeSeconds.size() < 2) return 0.0;
    return std::max(0.0, m_timeSeconds.back() - m_timeSeconds.front());
}

double DocumentController::dataStartSeconds() const {
    return m_timeSeconds.empty() ? 0.0 : m_timeSeconds.front();
}

double DocumentController::dataEndSeconds() const {
    return m_timeSeconds.empty() ? 0.0 : m_timeSeconds.back();
}

const std::vector<double>& DocumentController::analogSamples(int index) const {
    static const std::vector<double> empty;
    if (index < 0 || index >= static_cast<int>(m_analogSamples.size())) return empty;
    return m_analogSamples[static_cast<std::size_t>(index)];
}

const std::vector<std::uint8_t>& DocumentController::statusSamples(int index) const {
    static const std::vector<std::uint8_t> empty;
    if (index < 0 || index >= static_cast<int>(m_statusSamples.size())) return empty;
    return m_statusSamples[static_cast<std::size_t>(index)];
}

std::pair<std::size_t, std::size_t> DocumentController::visibleSampleRange(double zoomFactor,
                                                                           double panFraction) const {
    if (m_timeSeconds.empty()) return {0, 0};
    if (m_timeSeconds.size() == 1) return {0, 1};

    zoomFactor = std::clamp(zoomFactor, 1.0, 500.0);
    panFraction = std::clamp(panFraction, 0.0, 1.0);
    const double fullDuration = durationSeconds();
    if (fullDuration <= 0.0) return {0, m_timeSeconds.size()};

    const double visibleDuration = fullDuration / zoomFactor;
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double startTime = dataStartSeconds() + panFraction * movable;
    const double endTime = startTime + visibleDuration;

    auto first = std::lower_bound(m_timeSeconds.begin(), m_timeSeconds.end(), startTime);
    auto last = std::upper_bound(m_timeSeconds.begin(), m_timeSeconds.end(), endTime);
    std::size_t start = static_cast<std::size_t>(std::distance(m_timeSeconds.begin(), first));
    std::size_t end = static_cast<std::size_t>(std::distance(m_timeSeconds.begin(), last));
    if (start >= m_timeSeconds.size()) start = m_timeSeconds.size() - 1;
    end = std::min(end, m_timeSeconds.size());
    if (end <= start + 1) end = std::min(m_timeSeconds.size(), start + 2);
    return {start, end};
}

void DocumentController::openCfg(const QUrl& url) {
    try {
        const auto path = std::filesystem::path(url.toLocalFile().toStdString());
        const auto bundle = ardirec::comtrade::locate_bundle(path);
        if (bundle.cfg.empty()) throw std::runtime_error("Cannot locate CFG file");
        if (bundle.dat.empty()) throw std::runtime_error("Matching DAT file was not found next to CFG");

        const auto cfg = ardirec::comtrade::ConfigParser{}.parse_file(bundle.cfg);
        const auto frames = ardirec::comtrade::DatReader{}.read(cfg, bundle.dat, kViewerAlphaFrameLimit);
        if (frames.empty()) throw std::runtime_error("DAT contains no readable sample frames");

        m_title = QString::fromStdString(cfg.station_name.empty() ? bundle.cfg.stem().string() : cfg.station_name);
        m_recorderId = cfg.recorder_id.empty() ? QStringLiteral("—") : QString::fromStdString(cfg.recorder_id);
        m_revisionText = QString::number(cfg.revision_year);
        m_dataFormatText = QString::fromLatin1(ardirec::comtrade::to_string(cfg.data_format));
        m_nominalFrequency = cfg.nominal_frequency;
        m_startTimeText = cfg.start_time.raw.empty() ? QStringLiteral("—") : QString::fromStdString(cfg.start_time.raw);
        m_triggerTimeText = cfg.trigger_time.raw.empty() ? QStringLiteral("—") : QString::fromStdString(cfg.trigger_time.raw);
        m_metadata = QStringLiteral("COMTRADE %1 · %2 · %3 analog · %4 digital · %5 Hz")
                         .arg(cfg.revision_year)
                         .arg(m_dataFormatText)
                         .arg(static_cast<qulonglong>(cfg.analog_channels.size()))
                         .arg(static_cast<qulonglong>(cfg.status_channels.size()))
                         .arg(cfg.nominal_frequency, 0, 'f', 1);

        m_channels.clear();
        m_channelNames.clear();
        m_channelUnits.clear();
        m_statusNames.clear();
        m_channelConfigs = cfg.analog_channels;
        m_valueRepresentation = QStringLiteral("secondary");
        m_analogCount = static_cast<int>(cfg.analog_channels.size());
        m_digitalCount = static_cast<int>(cfg.status_channels.size());
        for (const auto& ch : cfg.analog_channels) {
            const auto unit = ch.units.empty() ? std::string{} : " · " + ch.units;
            m_channels << QStringLiteral("A  %1%2")
                              .arg(QString::fromStdString(ch.id), QString::fromStdString(unit));
            m_channelNames << QString::fromStdString(ch.id);
            m_channelUnits << QString::fromStdString(ch.units);
        }
        for (const auto& ch : cfg.status_channels) {
            const QString name = QString::fromStdString(ch.id);
            m_channels << QStringLiteral("D  %1").arg(name);
            m_statusNames << name;
        }
        rebuildTransformerSummary();

        m_analogSamples.assign(cfg.analog_channels.size(), {});
        for (auto& values : m_analogSamples) values.reserve(frames.size());
        m_statusSamples.assign(cfg.status_channels.size(), {});
        for (auto& values : m_statusSamples) values.reserve(frames.size());
        m_statusNormalState.clear();
        m_statusNormalState.reserve(cfg.status_channels.size());
        for (const auto& channel : cfg.status_channels) m_statusNormalState.push_back(channel.normal_state);
        m_timeSeconds.clear();
        m_timeSeconds.reserve(frames.size());

        const double timeScale = cfg.time_multiplier * 1.0e-6;
        for (const auto& frame : frames) {
            m_timeSeconds.push_back(static_cast<double>(frame.raw_timestamp) * timeScale);
            const auto analogCount = std::min(frame.analog.size(), m_analogSamples.size());
            for (std::size_t i = 0; i < analogCount; ++i) m_analogSamples[i].push_back(frame.analog[i]);
            const auto statusCount = std::min(frame.status.size(), m_statusSamples.size());
            for (std::size_t i = 0; i < statusCount; ++i) {
                m_statusSamples[i].push_back(frame.status[i] ? std::uint8_t{1} : std::uint8_t{0});
            }
        }

        m_channelPeaks.assign(m_analogSamples.size(), 1.0);
        for (std::size_t channel = 0; channel < m_analogSamples.size(); ++channel) {
            double peak = 0.0;
            for (const double value : m_analogSamples[channel]) {
                if (std::isfinite(value)) peak = std::max(peak, std::abs(value));
            }
            m_channelPeaks[channel] = peak;
        }

        m_statusActive.assign(m_statusSamples.size(), false);
        m_activeDigitalCount = 0;
        for (std::size_t channel = 0; channel < m_statusSamples.size(); ++channel) {
            const bool active = std::any_of(m_statusSamples[channel].begin(), m_statusSamples[channel].end(),
                                            [](std::uint8_t value) { return value != 0; });
            m_statusActive[channel] = active;
            if (active) ++m_activeDigitalCount;
        }
        rebuildDigitalEdges();

        m_triggerOffsetSeconds = dataStartSeconds();
        const auto startStamp = parse_comtrade_timestamp(cfg.start_time.raw);
        const auto triggerStamp = parse_comtrade_timestamp(cfg.trigger_time.raw);
        if (startStamp && triggerStamp) {
            const double delta = *triggerStamp - *startStamp;
            if (delta >= -1.0e-6 && delta <= durationSeconds() + 1.0e-3) {
                m_triggerOffsetSeconds = dataStartSeconds() + std::max(0.0, delta);
            }
        }

        m_selectedAnalogIndex = m_analogCount > 0 ? 0 : -1;
        m_selectedSignal = m_selectedAnalogIndex >= 0
                               ? m_channelNames.value(0)
                               : QStringLiteral("No analog signal");
        rebuildSelectedSamples();

        const bool hitLimit = frames.size() >= kViewerAlphaFrameLimit;
        m_recordHealth = hitLimit
                             ? QStringLiteral("Loaded · preview capped at %1 samples for alpha")
                                   .arg(static_cast<qulonglong>(kViewerAlphaFrameLimit))
                             : QStringLiteral("Loaded · %1 analog · %2 digital (%3 active)")
                                   .arg(m_analogCount)
                                   .arg(m_digitalCount)
                                   .arg(m_activeDigitalCount);
        m_error.clear();
        emit documentChanged();
        emit waveformChanged();
        emit representationChanged();
        emit errorChanged();
    } catch (const std::exception& ex) {
        m_error = QString::fromUtf8(ex.what());
        emit errorChanged();
    }
}

void DocumentController::selectChannel(int index) {
    if (index < 0 || index >= m_analogCount || index >= static_cast<int>(m_analogSamples.size())) return;
    if (m_selectedAnalogIndex == index) return;
    m_selectedAnalogIndex = index;
    m_selectedSignal = m_channelNames.value(index);
    rebuildSelectedSamples();
    emit waveformChanged();
}

void DocumentController::setValueRepresentation(const QString& representation) {
    const QString normalized = representation.trimmed().toLower();
    if (normalized != QStringLiteral("primary") && normalized != QStringLiteral("secondary")) return;
    if (m_valueRepresentation == normalized) return;
    m_valueRepresentation = normalized;
    rebuildSelectedSamples();
    emit representationChanged();
    emit waveformChanged();
}

QString DocumentController::channelName(int index) const {
    return index >= 0 && index < m_channelNames.size() ? m_channelNames.at(index) : QStringLiteral("—");
}

QString DocumentController::channelUnit(int index) const {
    return index >= 0 && index < m_channelUnits.size() ? m_channelUnits.at(index) : QString{};
}

QString DocumentController::analogRole(int index) const {
    if (index < 0 || index >= m_channelNames.size()) return QStringLiteral("Other");
    const QString unit = normalized_unit(channelUnit(index));
    const QString name = channelName(index).trimmed().toUpper();

    if (unit == QStringLiteral("V") || unit == QStringLiteral("KV") || unit == QStringLiteral("MV")
        || unit.contains(QStringLiteral("VOLT"))) {
        return QStringLiteral("Voltage");
    }
    if (unit == QStringLiteral("A") || unit == QStringLiteral("KA") || unit == QStringLiteral("MA")
        || unit.contains(QStringLiteral("AMP"))) {
        return QStringLiteral("Current");
    }

    if (name.startsWith('V') || name.startsWith('U') || name.contains(QStringLiteral(":V"))
        || name.contains(QStringLiteral("UL1")) || name.contains(QStringLiteral("UL2"))
        || name.contains(QStringLiteral("UL3"))) {
        return QStringLiteral("Voltage");
    }
    if (name.startsWith('I') || name.contains(QStringLiteral(":I")) || name.contains(QStringLiteral("IL1"))
        || name.contains(QStringLiteral("IL2")) || name.contains(QStringLiteral("IL3"))) {
        return QStringLiteral("Current");
    }
    return QStringLiteral("Other");
}

double DocumentController::channelDisplayScale(int index) const {
    if (index < 0 || index >= static_cast<int>(m_channelConfigs.size())) return 1.0;
    const auto target = m_valueRepresentation == QStringLiteral("primary")
                            ? ardirec::comtrade::ValueRepresentation::Primary
                            : ardirec::comtrade::ValueRepresentation::Secondary;
    return ardirec::comtrade::representation_scale(m_channelConfigs[static_cast<std::size_t>(index)], target);
}

double DocumentController::channelPeak(int index) const {
    if (index < 0 || index >= static_cast<int>(m_channelPeaks.size())) return 1.0;
    return nice_peak(m_channelPeaks[static_cast<std::size_t>(index)] * std::abs(channelDisplayScale(index)));
}

QString DocumentController::channelRatioText(int index) const {
    if (index < 0 || index >= static_cast<int>(m_channelConfigs.size())) return QStringLiteral("—");
    const auto& channel = m_channelConfigs[static_cast<std::size_t>(index)];
    if (!ardirec::comtrade::has_valid_transformer_ratio(channel)) return QStringLiteral("1:1 / unavailable");
    const QString unit = channelUnit(index);
    const QString recorded = ardirec::comtrade::recorded_representation(channel)
                                     == ardirec::comtrade::ValueRepresentation::Primary
                                 ? QStringLiteral("P")
                                 : QStringLiteral("S");
    return QStringLiteral("Pri %1 / Sec %2%3 · recorded %4")
        .arg(compact_ratio_value(*channel.primary))
        .arg(compact_ratio_value(*channel.secondary))
        .arg(unit.isEmpty() ? QString{} : QStringLiteral(" ") + unit)
        .arg(recorded);
}

std::size_t DocumentController::nearestSampleIndex(double absoluteTimeSeconds) const {
    if (m_timeSeconds.empty()) return 0;
    absoluteTimeSeconds = std::clamp(absoluteTimeSeconds, dataStartSeconds(), dataEndSeconds());
    auto it = std::lower_bound(m_timeSeconds.begin(), m_timeSeconds.end(), absoluteTimeSeconds);
    std::size_t index = static_cast<std::size_t>(std::distance(m_timeSeconds.begin(), it));
    if (index >= m_timeSeconds.size()) index = m_timeSeconds.size() - 1;
    if (index > 0 && index < m_timeSeconds.size()) {
        const double before = std::abs(absoluteTimeSeconds - m_timeSeconds[index - 1]);
        const double after = std::abs(m_timeSeconds[index] - absoluteTimeSeconds);
        if (before <= after) --index;
    }
    return index;
}

double DocumentController::sampleValue(int channelIndex, double absoluteTimeSeconds) const {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(m_analogSamples.size()) || m_timeSeconds.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto& samples = m_analogSamples[static_cast<std::size_t>(channelIndex)];
    if (samples.empty()) return std::numeric_limits<double>::quiet_NaN();
    const std::size_t index = std::min(nearestSampleIndex(absoluteTimeSeconds), samples.size() - 1);
    return samples[index] * channelDisplayScale(channelIndex);
}

QString DocumentController::formatChannelValue(int channelIndex, double value) const {
    if (!std::isfinite(value)) return QStringLiteral("—");
    const double magnitude = std::abs(value);
    int decimals = 4;
    if (magnitude >= 100000.0) decimals = 0;
    else if (magnitude >= 10000.0) decimals = 1;
    else if (magnitude >= 1000.0) decimals = 1;
    else if (magnitude >= 100.0) decimals = 2;
    else if (magnitude >= 10.0) decimals = 3;
    const QString unit = channelUnit(channelIndex);
    return unit.isEmpty() ? QString::number(value, 'f', decimals)
                          : QStringLiteral("%1 %2").arg(QString::number(value, 'f', decimals), unit);
}

QString DocumentController::sampleValueText(int channelIndex, double absoluteTimeSeconds) const {
    return formatChannelValue(channelIndex, sampleValue(channelIndex, absoluteTimeSeconds));
}

QString DocumentController::digitalName(int index) const {
    return index >= 0 && index < m_statusNames.size() ? m_statusNames.at(index) : QStringLiteral("—");
}

bool DocumentController::digitalIsActive(int index) const {
    return index >= 0 && index < static_cast<int>(m_statusActive.size())
           && m_statusActive[static_cast<std::size_t>(index)];
}

bool DocumentController::digitalStateAt(int index, double absoluteTimeSeconds) const {
    if (index < 0 || index >= static_cast<int>(m_statusSamples.size()) || m_timeSeconds.empty()) return false;
    const auto& samples = m_statusSamples[static_cast<std::size_t>(index)];
    if (samples.empty()) return false;
    const std::size_t sample = std::min(nearestSampleIndex(absoluteTimeSeconds), samples.size() - 1);
    return samples[sample] != 0;
}

QString DocumentController::digitalStateText(int index, double absoluteTimeSeconds) const {
    return digitalStateAt(index, absoluteTimeSeconds) ? QStringLiteral("1") : QStringLiteral("0");
}

double DocumentController::snapToDigitalEdge(double absoluteTimeSeconds, double maxDistanceSeconds) const {
    if (m_digitalEdgeTimes.empty() || !std::isfinite(absoluteTimeSeconds)
        || !std::isfinite(maxDistanceSeconds) || maxDistanceSeconds <= 0.0) {
        return absoluteTimeSeconds;
    }

    const auto it = std::lower_bound(m_digitalEdgeTimes.begin(), m_digitalEdgeTimes.end(), absoluteTimeSeconds);
    double nearest = absoluteTimeSeconds;
    double distance = std::numeric_limits<double>::infinity();

    if (it != m_digitalEdgeTimes.end()) {
        nearest = *it;
        distance = std::abs(*it - absoluteTimeSeconds);
    }
    if (it != m_digitalEdgeTimes.begin()) {
        const double candidate = *std::prev(it);
        const double candidateDistance = std::abs(candidate - absoluteTimeSeconds);
        if (candidateDistance < distance) {
            nearest = candidate;
            distance = candidateDistance;
        }
    }

    return distance <= maxDistanceSeconds ? nearest : absoluteTimeSeconds;
}

void DocumentController::rebuildDigitalEdges() {
    m_digitalEdgeTimes.clear();
    if (m_timeSeconds.size() < 2 || m_statusSamples.empty()) return;

    for (const auto& samples : m_statusSamples) {
        const std::size_t count = std::min(samples.size(), m_timeSeconds.size());
        for (std::size_t i = 1; i < count; ++i) {
            if (samples[i] != samples[i - 1]) m_digitalEdgeTimes.push_back(m_timeSeconds[i]);
        }
    }

    std::sort(m_digitalEdgeTimes.begin(), m_digitalEdgeTimes.end());
    m_digitalEdgeTimes.erase(std::unique(m_digitalEdgeTimes.begin(), m_digitalEdgeTimes.end()),
                             m_digitalEdgeTimes.end());
}

void DocumentController::rebuildTransformerSummary() {
    QString voltageRatio;
    QString currentRatio;
    for (int index = 0; index < static_cast<int>(m_channelConfigs.size()); ++index) {
        const auto& channel = m_channelConfigs[static_cast<std::size_t>(index)];
        if (!ardirec::comtrade::has_valid_transformer_ratio(channel)) continue;
        const QString ratio = QStringLiteral("%1/%2")
                                  .arg(compact_ratio_value(*channel.primary),
                                       compact_ratio_value(*channel.secondary));
        const QString role = analogRole(index);
        if (role == QStringLiteral("Voltage") && voltageRatio.isEmpty()) voltageRatio = ratio;
        if (role == QStringLiteral("Current") && currentRatio.isEmpty()) currentRatio = ratio;
    }

    QStringList parts;
    if (!voltageRatio.isEmpty()) parts << QStringLiteral("PT %1").arg(voltageRatio);
    if (!currentRatio.isEmpty()) parts << QStringLiteral("CT %1").arg(currentRatio);
    m_transformerRatiosAvailable = !parts.isEmpty();
    m_transformerRatioSummary = parts.isEmpty() ? QStringLiteral("No CT/PT ratio metadata")
                                                : parts.join(QStringLiteral(" · "));
}

void DocumentController::rebuildSelectedSamples() {
    if (m_selectedAnalogIndex < 0 || m_selectedAnalogIndex >= static_cast<int>(m_analogSamples.size())) {
        m_selectedSamples.clear();
        return;
    }
    m_selectedSamples = m_analogSamples[static_cast<std::size_t>(m_selectedAnalogIndex)];
    const double scale = channelDisplayScale(m_selectedAnalogIndex);
    for (double& value : m_selectedSamples) {
        if (std::isfinite(value)) value *= scale;
    }
}
