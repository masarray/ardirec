// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec_bridge.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string utf8_string(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}
}

int main() {
    try {
        require(ardirec_bridge_abi_version() == ARDIREC_BRIDGE_ABI_VERSION, "ABI version");

        const auto cfg = std::filesystem::path(ARDIREC_BRIDGE_TEST_DATA_DIR) / "minimal_1999.cfg";
        const auto cfg_path = utf8_string(cfg);

        ardirec_record_handle handle = nullptr;
        char error[512]{};
        require(ardirec_record_open_utf8(cfg_path.c_str(), &handle, error, sizeof(error)) == 0, error);
        require(handle != nullptr, "record handle");

        ardirec_record_info info{};
        require(ardirec_record_get_info(handle, &info) == 0, "record info");
        require(info.abi_version == ARDIREC_BRIDGE_ABI_VERSION, "record ABI");
        require(std::string(info.station_name) == "ARDIREC TEST", "station");
        require(info.revision_year == 1999, "revision");
        require(info.analog_count == 3, "analog count");
        require(info.status_count == 2, "status count");
        require(info.frame_count == 4, "frame count");

        ardirec_analog_channel_info analog_info{};
        require(ardirec_record_get_analog_channel(handle, 0, &analog_info) == 0, "analog metadata");
        require(analog_info.has_primary == 1 && analog_info.has_secondary == 1, "ratio metadata");

        std::vector<double> analog(info.frame_count);
        require(ardirec_record_copy_analog(handle, 0, 0, info.frame_count, analog.data()) == 0, "analog samples");
        require(std::abs(analog[1] - 10.0) < 1e-12, "analog value");

        std::vector<uint8_t> status(info.frame_count);
        require(ardirec_record_copy_status(handle, 0, 0, info.frame_count, status.data()) == 0, "status samples");
        require(status[2] == 1u, "status value");

        std::vector<uint32_t> timestamps(info.frame_count);
        require(ardirec_record_copy_raw_timestamps(handle, 0, info.frame_count, timestamps.data()) == 0, "timestamps");

        require(ardirec_record_copy_analog(handle, 99, 0, 1, analog.data()) < 0, "channel bounds");
        require(ardirec_record_copy_status(handle, 0, info.frame_count, 1, status.data()) < 0, "frame bounds");
        ardirec_record_close(handle);
        handle = nullptr;

        // 1 kHz / 50 Hz fixture: VA is a clean 50 V RMS cosine. This validates
        // the complete bridge path through ArdIrec's harmonic DFT engine.
        const auto analysis_cfg = std::filesystem::path(ARDIREC_BRIDGE_TEST_DATA_DIR) / "distance_p1.cfg";
        const auto analysis_cfg_path = utf8_string(analysis_cfg);
        require(ardirec_record_open_utf8(analysis_cfg_path.c_str(), &handle, error, sizeof(error)) == 0, error);
        require(handle != nullptr, "analysis record handle");

        ardirec_phasor_info phasor{};
        require(ardirec_record_get_phasor(handle, 0, 20, &phasor) == 0, "phasor analysis");
        require(phasor.valid == 1, "phasor valid");
        require(std::abs(phasor.magnitude_rms - 50.0) < 1.0e-4, "phasor magnitude");
        require(std::abs(phasor.angle_degrees) < 1.0e-4, "phasor cosine-reference angle");
        require(phasor.window_end_exclusive > phasor.window_start_frame, "phasor window");

        ardirec_harmonic_spectrum_info spectrum{};
        require(ardirec_record_get_harmonic_spectrum(handle, 0, 20, 10, &spectrum, nullptr, 0) == 0,
                "harmonic query");
        require(spectrum.valid == 1, "harmonic spectrum valid");
        require(spectrum.bin_count == 10, "harmonic bin count");
        require(std::abs(spectrum.fundamental_rms - 50.0) < 1.0e-4, "harmonic fundamental");
        require(spectrum.thd_percent < 1.0e-4, "harmonic THD");

        std::vector<ardirec_harmonic_bin> bins(spectrum.bin_count);
        require(ardirec_record_get_harmonic_spectrum(
                    handle, 0, 20, 10, &spectrum, bins.data(), static_cast<uint32_t>(bins.size())) == 0,
                "harmonic copy");
        require(bins.front().order == 1, "fundamental order");
        require(std::abs(bins.front().magnitude_rms - 50.0) < 1.0e-4, "fundamental magnitude");
        require(std::abs(bins.front().percent_of_fundamental - 100.0) < 1.0e-6, "fundamental percent");
        require(std::abs(bins.front().angle_degrees - 90.0) < 1.0e-4, "sine-reference harmonic angle");

        ardirec_record_close(handle);
        std::cout << "ardirec native bridge smoke: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec native bridge smoke: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
