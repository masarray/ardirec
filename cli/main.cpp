// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/bundle.hpp"
#include "ardirec/comtrade/dat_reader.hpp"
#include "ardirec/comtrade/parser.hpp"

#include <exception>
#include <iostream>

using namespace ardirec::comtrade;

namespace {
void usage() {
    std::cout << "ardirec-cli 0.1.0\n"
                 "Usage:\n"
                 "  ardirec-cli inspect <record.cfg>\n";
}
}

int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]) != "inspect") {
        usage();
        return argc == 1 ? 0 : 2;
    }
    try {
        const auto bundle = locate_bundle(argv[2]);
        if (bundle.cfg.empty()) throw std::runtime_error("CFG file not found");
        const auto cfg = ConfigParser{}.parse_file(bundle.cfg);
        std::cout << "Station:       " << cfg.station_name << '\n'
                  << "Recorder:      " << cfg.recorder_id << '\n'
                  << "Revision:      " << cfg.revision_year << '\n'
                  << "Channels:      " << cfg.total_channels << " ("
                  << cfg.analog_channels.size() << " analog, "
                  << cfg.status_channels.size() << " status)\n"
                  << "Frequency:     " << cfg.nominal_frequency << " Hz\n"
                  << "Data format:   " << to_string(cfg.data_format) << '\n'
                  << "Sample rates:  " << cfg.sample_rates.size() << '\n'
                  << "Start:         " << cfg.start_time.raw << '\n'
                  << "Trigger:       " << cfg.trigger_time.raw << '\n'
                  << "DAT:           " << (bundle.dat.empty() ? "not found" : bundle.dat.string()) << '\n';
        if (!bundle.dat.empty()) {
            const auto preview = DatReader{}.read(cfg, bundle.dat, 3);
            std::cout << "Preview rows:  " << preview.size() << '\n';
        }
        for (const auto& note : cfg.diagnostics) std::cout << "Note:          " << note << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec: " << ex.what() << '\n';
        return 1;
    }
}
