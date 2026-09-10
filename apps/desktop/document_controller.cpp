// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"

#include "document_loader.hpp"
#include "ardirec/comtrade/value_representation.hpp"

#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTime>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>

namespace {

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

QString filesystem_path_to_qstring(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::filesystem::path qstring_to_filesystem_path(const QString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toStdString());
#endif
}

QString decoded_text(const std::string& bytes) {
    if (bytes.empty()) return {};
    QString decoded = QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size()));
    if (decoded.contains(QChar(0xfffd))) {
        decoded = QString::fromLatin1(bytes.data(), static_cast<qsizetype>(bytes.size()));
    }
    return decoded;
}

} // namespace

DocumentController::DocumentController(QObject* parent) : QObject(parent) {}

DocumentController::~DocumentController() {
    if (m_activeLoadCancel) m_activeLoadCancel->store(true, std::memory_order_relaxed);
}

const std::vector<double>& DocumentController::timeSeconds() const {
    static const std::vector<double> empty;
    return m_timeSeconds ? *m_timeSeconds : empty;
}

double DocumentController::durationSeconds() const {
    const auto& times = timeSeconds();
    if (times.size() < 2) return 0.0;
    return std::max(0.0, times.back() - times.front());
}

double DocumentController::dataStartSeconds() const {
    const auto& times = timeSeconds();
    return times.empty() ? 0.0 : times.front();
}

double DocumentController::dataEndSeconds() const {
    const auto& times = timeSeconds();
    return times.empty() ? 0.0 : times.back();
}

double DocumentController::recordedAnalogSampleAt(int channelIndex, std::size_t frameIndex) const noexcept {
    if (!m_datStore || channelIndex < 0 || channelIndex >= m_analogCount
        || frameIndex >= timeSeconds().size()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return m_datStore->analogValue(frameIndex, static_cast<std::size_t>(channelIndex));
}

bool DocumentController::recordedDigitalSampleAt(int channelIndex, std::size_t frameIndex) const noexcept {
    if (!m_datStore || channelIndex < 0 || channelIndex >= m_digitalCount
        || frameIndex >= timeSeconds().size()) {
        return false;
    }
    return m_datStore->statusValue(frameIndex, static_cast<std::size_t>(channelIndex));
}

void DocumentController::copyRecordedAnalogRange(int channelIndex,
                                                 std::size_t first,
                                                 std::size_t end,
                                                 std::vector<double>& destination) const {
    destination.clear();
    if (!m_datStore || channelIndex < 0 || channelIndex >= m_analogCount) return;
    end = std::min(end, timeSeconds().size());
    if (first >= end) return;
    m_datStore->copyAnalogRange(static_cast<std::size_t>(channelIndex), first, end, destination);
}

std::pair<std::size_t, std::size_t> DocumentController::visibleSampleRange(double zoomFactor,
                                                                           double panFraction) const {
    const auto& times = timeSeconds();
    if (times.empty()) return {0, 0};
    if (times.size() == 1) return {0, 1};

    zoomFactor = std::clamp(zoomFactor, 1.0, 500.0);
    panFraction = std::clamp(panFraction, 0.0, 1.0);
    const double fullDuration = durationSeconds();
    if (fullDuration <= 0.0) return {0, times.size()};

    const double visibleDuration = fullDuration / zoomFactor;
    const double movable = std::max(0.0, fullDuration - visibleDuration);
    const double startTime = dataStartSeconds() + panFraction * movable;
    const double endTime = startTime + visibleDuration;

    auto first = std::lower_bound(times.begin(), times.end(), startTime);
    auto last = std::upper_bound(times.begin(), times.end(), endTime);
    std::size_t start = static_cast<std::size_t>(std::distance(times.begin(), first));
    std::size_t end = static_cast<std::size_t>(std::distance(times.begin(), last));
    if (start >= times.size()) start = times.size() - 1;
    end = std::min(end, times.size());
    if (end <= start + 1) end = std::min(times.size(), start + 2);
    return {start, end};
}

void DocumentController::openCfg(const QUrl& url) {
    const auto path = qstring_to_filesystem_path(url.toLocalFile());
    if (path.empty()) {
        m_error = QStringLiteral("Invalid CFG path");
        emit errorChanged();
        return;
    }

    if (m_activeLoadCancel) m_activeLoadCancel->store(true, std::memory_order_relaxed);
    const quint64 generation = ++m_loadGeneration;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_activeLoadCancel = cancel;
    m_loading = true;
    m_loadingStatus = QStringLiteral("Indexing COMTRADE in background · %1")
                          .arg(QFileInfo(url.toLocalFile()).fileName());
    m_error.clear();
    emit loadingChanged();
    emit errorChanged();

    auto* watcher = new QFutureWatcher<std::shared_ptr<LoadedDocumentData>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<LoadedDocumentData>>::finished,
            this, [this, watcher, generation, cancel]() {
                const auto loaded = watcher->result();
                watcher->deleteLater();
                if (generation != m_loadGeneration) return;
                if (m_activeLoadCancel == cancel) m_activeLoadCancel.reset();
                applyLoadedDocument(loaded, generation);
            });

    watcher->setFuture(QtConcurrent::run([path, cancel]() {
        return loadDocumentData(path, cancel);
    }));
}

void DocumentController::applyLoadedDocument(const std::shared_ptr<LoadedDocumentData>& loaded,
                                             quint64 generation) {
    if (generation != m_loadGeneration) return;
    m_loading = false;
    m_loadingStatus.clear();

    if (!loaded || loaded->cancelled) {
        emit loadingChanged();
        return;
    }
    if (!loaded->error.empty() || !loaded->dat || !loaded->time_seconds || loaded->time_seconds->empty()) {
        m_error = QString::fromStdString(loaded ? loaded->error : std::string("COMTRADE load failed"));
        if (m_error.isEmpty()) m_error = QStringLiteral("COMTRADE load failed");
        emit loadingChanged();
        emit errorChanged();
        return;
    }

    const auto& cfg = loaded->config;
    const auto& bundle = loaded->bundle;
    m_datStore = loaded->dat;
    m_timeSeconds = loaded->time_seconds;
    m_channelPeaks = loaded->channel_peaks;
    m_statusActive = loaded->status_active;
    m_digitalEdgeTimes = loaded->digital_edge_times;
    m_diagnostics.clear();
    m_diagnostics.reserve(static_cast<qsizetype>(loaded->diagnostics.size()));
    for (const auto& diagnostic : loaded->diagnostics) {
        m_diagnostics.push_back(QString::fromStdString(diagnostic));
    }

    m_distanceZonePath.clear();
    m_headerSourceName.clear();
    m_headerText.clear();
    const auto distanceSidecar = !bundle.rio.empty() ? bundle.rio : bundle.xrio;
    if (!distanceSidecar.empty()) {
        m_distanceZonePath = QFileInfo(filesystem_path_to_qstring(distanceSidecar)).absoluteFilePath();
    }
    if (!bundle.hdr.empty()) {
        m_headerSourceName = QFileInfo(filesystem_path_to_qstring(bundle.hdr)).fileName();
        m_headerText = decoded_text(loaded->header_text);
    }

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
    for (const auto& channel : cfg.analog_channels) {
        const auto unit = channel.units.empty() ? std::string{} : " · " + channel.units;
        m_channels << QStringLiteral("A  %1%2")
                          .arg(QString::fromStdString(channel.id), QString::fromStdString(unit));
        m_channelNames << QString::fromStdString(channel.id);
        m_channelUnits << QString::fromStdString(channel.units);
    }
    for (const auto& channel : cfg.status_channels) {
        const QString name = QString::fromStdString(channel.id);
        m_channels << QStringLiteral("D  %1").arg(name);
        m_statusNames << name;
    }
    rebuildTransformerSummary();

    m_statusNormalState.clear();
    m_statusNormalState.reserve(cfg.status_channels.size());
    for (const auto& channel : cfg.status_channels) m_statusNormalState.push_back(channel.normal_state);

    m_activeDigitalCount = static_cast<int>(std::count_if(m_statusActive.begin(), m_statusActive.end(),
                                                          [](std::uint8_t value) { return value != 0; }));

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

    const QString accessMode = m_datStore->memoryMapped()
                                   ? QStringLiteral("mmap lazy")
                                   : QStringLiteral("stream-indexed lazy");
    m_recordHealth = QStringLiteral("Loaded · %1 samples · %2 · %3 analog · %4 digital (%5 active)")
                         .arg(static_cast<qulonglong>(sampleCount()))
                         .arg(accessMode)
                         .arg(m_analogCount)
                         .arg(m_digitalCount)
                         .arg(m_activeDigitalCount);
    QStringList sidecars;
    if (!m_distanceZonePath.isEmpty()) sidecars << QFileInfo(m_distanceZonePath).suffix().toUpper();
    if (!m_headerSourceName.isEmpty()) sidecars << QStringLiteral("HDR");
    if (!sidecars.isEmpty()) {
        m_recordHealth += QStringLiteral(" · sidecars %1").arg(sidecars.join(QStringLiteral(" + ")));
    }
    if (!m_diagnostics.isEmpty()) {
        m_recordHealth += QStringLiteral(" · %1 diagnostic(s)").arg(m_diagnostics.size());
    }

    m_error.clear();
    emit loadingChanged();
    emit documentChanged();
    emit waveformChanged();
    emit representationChanged();
    emit errorChanged();
}

void DocumentController::selectChannel(int index) {
    if (index < 0 || index >= m_analogCount) return;
    if (m_selectedAnalogIndex == index) return;
    m_selectedAnalogIndex = index;
    m_selectedSignal = m_channelNames.value(index);
    emit waveformChanged();
}

void DocumentController::setValueRepresentation(const QString& representation) {
    const QString normalized = representation.trimmed().toLower();
    if (normalized != QStringLiteral("primary") && normalized != QStringLiteral("secondary")) return;
    if (m_valueRepresentation == normalized) return;
    m_valueRepresentation = normalized;
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
    const auto& times = timeSeconds();
    if (times.empty()) return 0;
    absoluteTimeSeconds = std::clamp(absoluteTimeSeconds, dataStartSeconds(), dataEndSeconds());
    auto it = std::lower_bound(times.begin(), times.end(), absoluteTimeSeconds);
    std::size_t index = static_cast<std::size_t>(std::distance(times.begin(), it));
    if (index >= times.size()) index = times.size() - 1;
    if (index > 0 && index < times.size()) {
        const double before = std::abs(absoluteTimeSeconds - times[index - 1]);
        const double after = std::abs(times[index] - absoluteTimeSeconds);
        if (before <= after) --index;
    }
    return index;
}

double DocumentController::sampleValue(int channelIndex, double absoluteTimeSeconds) const {
    if (channelIndex < 0 || channelIndex >= m_analogCount || timeSeconds().empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::size_t index = nearestSampleIndex(absoluteTimeSeconds);
    const double value = recordedAnalogSampleAt(channelIndex, index);
    return std::isfinite(value) ? value * channelDisplayScale(channelIndex)
                                : std::numeric_limits<double>::quiet_NaN();
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
           && m_statusActive[static_cast<std::size_t>(index)] != 0;
}

bool DocumentController::digitalStateAt(int index, double absoluteTimeSeconds) const {
    if (index < 0 || index >= m_digitalCount || timeSeconds().empty()) return false;
    return recordedDigitalSampleAt(index, nearestSampleIndex(absoluteTimeSeconds));
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
