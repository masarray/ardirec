// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_controller.hpp"

#include "ardirec/comtrade/parser.hpp"

#include <filesystem>

void DocumentController::openCfg(const QUrl& url) {
    try {
        const auto path = std::filesystem::path(url.toLocalFile().toStdString());
        const auto cfg = ardirec::comtrade::ConfigParser{}.parse_file(path);
        m_title = QString::fromStdString(cfg.station_name);
        m_metadata = QStringLiteral("%1 · %2 · %3 analog · %4 digital · %5 Hz")
                         .arg(cfg.revision_year)
                         .arg(QString::fromLatin1(ardirec::comtrade::to_string(cfg.data_format)))
                         .arg(static_cast<qulonglong>(cfg.analog_channels.size()))
                         .arg(static_cast<qulonglong>(cfg.status_channels.size()))
                         .arg(cfg.nominal_frequency, 0, 'f', 1);
        m_channels.clear();
        for (const auto& ch : cfg.analog_channels) m_channels << QString::fromStdString(ch.id);
        for (const auto& ch : cfg.status_channels) m_channels << QString::fromStdString(ch.id);
        m_error.clear();
        emit documentChanged();
        emit errorChanged();
    } catch (const std::exception& ex) {
        m_error = QString::fromUtf8(ex.what());
        emit errorChanged();
    }
}
