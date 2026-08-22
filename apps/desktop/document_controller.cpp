// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"

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
        m_analogCount = static_cast<int>(cfg.analog_channels.size());
        for (const auto& ch : cfg.analog_channels) {
            const auto unit = ch.units.empty() ? std::string{} : " · " + ch.units;
            m_channels << QStringLiteral("A  %1%2")
                              .arg(QString::fromStdString(ch.id), QString::fromStdString(unit));
            m_channelNames << QString::fromStdString(ch.id);
            m_channelUnits << QString::fromStdString(ch.units);
        }
        for (const auto& ch : cfg.status_channels) {
            m_channels << QStringLiteral("D  %1").arg(QString::fromStdString(ch.id));
        }

        m_analogSamples.assign(cfg.analog_channels.size(), {});
        for (auto& values : m_analogSamples) values.reserve(frames.size());
        m_timeSeconds.clear();
        m_timeSeconds.reserve(frames.size());

        const double timeScale = cfg.time_multiplier * 1.0e-6;
        for (const auto& frame : frames) {
            m_timeSeconds.push_back(static_cast<double>(frame.raw_timestamp) * timeScale);
            const auto count = std::min(frame.analog.size(), m_analogSamples.size());
            for (std::size_t i = 0; i < count; ++i) m_analogSamples[i].push_back(frame.analog[i]);
        }

        m_channelPeaks.assign(m_analogSamples.size(), 1.0);
        for (std::size_t channel = 0; channel < m_analogSamples.size(); ++channel) {
            double peak = 0.0;
            for (const double value : m_analogSamples[channel]) {
                if (std::isfinite(value)) peak = std::max(peak, std::abs(value));
            }
            m_channelPeaks[channel] = nice_peak(peak);
        }

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
                             : QStringLiteral("Loaded · CFG/DAT consistent enough to render");
        m_error.clear();
        emit documentChanged();
        emit waveformChanged();
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

QString DocumentController::channelName(int index) const {
    return index >= 0 && index < m_channelNames.size() ? m_channelNames.at(index) : QStringLiteral("—");
}

QString DocumentController::channelUnit(int index) const {
    return index >= 0 && index < m_channelUnits.size() ? m_channelUnits.at(index) : QString{};
}

double DocumentController::channelPeak(int index) const {
    if (index < 0 || index >= static_cast<int>(m_channelPeaks.size())) return 1.0;
    return m_channelPeaks[static_cast<std::size_t>(index)];
}

double DocumentController::sampleValue(int channelIndex, double absoluteTimeSeconds) const {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(m_analogSamples.size()) || m_timeSeconds.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto& samples = m_analogSamples[static_cast<std::size_t>(channelIndex)];
    if (samples.empty()) return std::numeric_limits<double>::quiet_NaN();

    absoluteTimeSeconds = std::clamp(absoluteTimeSeconds, dataStartSeconds(), dataEndSeconds());
    auto it = std::lower_bound(m_timeSeconds.begin(), m_timeSeconds.end(), absoluteTimeSeconds);
    std::size_t index = static_cast<std::size_t>(std::distance(m_timeSeconds.begin(), it));
    if (index >= m_timeSeconds.size()) index = m_timeSeconds.size() - 1;
    if (index > 0 && index < m_timeSeconds.size()) {
        const double before = std::abs(absoluteTimeSeconds - m_timeSeconds[index - 1]);
        const double after = std::abs(m_timeSeconds[index] - absoluteTimeSeconds);
        if (before <= after) --index;
    }
    index = std::min(index, samples.size() - 1);
    return samples[index];
}

QString DocumentController::sampleValueText(int channelIndex, double absoluteTimeSeconds) const {
    const double value = sampleValue(channelIndex, absoluteTimeSeconds);
    if (!std::isfinite(value)) return QStringLiteral("—");
    const double magnitude = std::abs(value);
    int decimals = 4;
    if (magnitude >= 1000.0) decimals = 1;
    else if (magnitude >= 100.0) decimals = 2;
    else if (magnitude >= 10.0) decimals = 3;
    const QString unit = channelUnit(channelIndex);
    return unit.isEmpty() ? QString::number(value, 'f', decimals)
                          : QStringLiteral("%1 %2").arg(QString::number(value, 'f', decimals), unit);
}

void DocumentController::rebuildSelectedSamples() {
    if (m_selectedAnalogIndex < 0 || m_selectedAnalogIndex >= static_cast<int>(m_analogSamples.size())) {
        m_selectedSamples.clear();
        return;
    }
    m_selectedSamples = m_analogSamples[static_cast<std::size_t>(m_selectedAnalogIndex)];
}
