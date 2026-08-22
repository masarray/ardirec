// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"

#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace {
constexpr std::size_t kViewerAlphaFrameLimit = 500'000;
}

double DocumentController::durationSeconds() const {
    if (m_timeSeconds.size() < 2) return 0.0;
    return std::max(0.0, m_timeSeconds.back() - m_timeSeconds.front());
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
        m_metadata = QStringLiteral("COMTRADE %1 · %2 · %3 analog · %4 digital · %5 Hz")
                         .arg(cfg.revision_year)
                         .arg(QString::fromLatin1(ardirec::comtrade::to_string(cfg.data_format)))
                         .arg(static_cast<qulonglong>(cfg.analog_channels.size()))
                         .arg(static_cast<qulonglong>(cfg.status_channels.size()))
                         .arg(cfg.nominal_frequency, 0, 'f', 1);

        m_channels.clear();
        m_analogCount = static_cast<int>(cfg.analog_channels.size());
        for (const auto& ch : cfg.analog_channels) {
            const auto unit = ch.units.empty() ? std::string{} : " · " + ch.units;
            m_channels << QStringLiteral("A  %1%2")
                              .arg(QString::fromStdString(ch.id), QString::fromStdString(unit));
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

        m_selectedAnalogIndex = m_analogCount > 0 ? 0 : -1;
        m_selectedSignal = m_selectedAnalogIndex >= 0
                               ? QString::fromStdString(cfg.analog_channels.front().id)
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
    const auto label = m_channels.value(index);
    m_selectedSignal = label.mid(3).section(QStringLiteral(" · "), 0, 0);
    rebuildSelectedSamples();
    emit waveformChanged();
}

void DocumentController::rebuildSelectedSamples() {
    if (m_selectedAnalogIndex < 0 || m_selectedAnalogIndex >= static_cast<int>(m_analogSamples.size())) {
        m_selectedSamples.clear();
        return;
    }
    m_selectedSamples = m_analogSamples[static_cast<std::size_t>(m_selectedAnalogIndex)];
}
