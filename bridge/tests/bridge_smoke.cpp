// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec_bridge.h"

#include <array>
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

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

std::string utf8_string(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}
}

int main() {
    try {
        require(ardirec_bridge_abi_version() == ARDIREC_BRIDGE_ABI_VERSION, "ABI version");
        const uint64_t capabilities = ardirec_bridge_capabilities();
        require((capabilities & ARDIREC_BRIDGE_CAP_CURSOR_MEASUREMENT) != 0, "cursor measurement capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_CHANNEL_SEMANTICS) != 0, "channel semantics capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_VALUE_REPRESENTATION) != 0, "representation capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_DISTANCE_LOCUS) != 0, "distance locus capability");

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

        ardirec_analog_semantics_info semantics{};
        require(ardirec_record_get_analog_semantics(handle, 0, &semantics) == 0, "analog semantics");
        require(semantics.role == ARDIREC_ANALOG_VOLTAGE, "voltage role");
        require(semantics.phase_role == ARDIREC_PHASE_L1, "L1 phase role");

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

        // 1 kHz / 50 Hz six-channel fixture also used by the desktop distance regression.
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

        // Distance cursor hot path: all six loops are returned from one shared phasor snapshot.
        double minimum_current = 0.0;
        require(ardirec_record_get_distance_current_floor(handle, ARDIREC_VALUE_SECONDARY, &minimum_current) == 0,
                "distance current floor");
        require_near(minimum_current, 0.001, 1.0e-12, "distance current floor value");

        std::array<ardirec_distance_point, 6> loops{};
        require(ardirec_record_get_distance_loops(
                    handle, 22, ARDIREC_VALUE_SECONDARY, 0.0, 0.0, minimum_current,
                    loops.data(), static_cast<uint32_t>(loops.size())) == 0,
                "distance loop cursor batch");
        for (int loop = ARDIREC_DISTANCE_L1_E; loop <= ARDIREC_DISTANCE_L3_L1; ++loop) {
            require(loops[static_cast<std::size_t>(loop)].loop == loop, "stable distance loop ordering");
            require(loops[static_cast<std::size_t>(loop)].valid == 1, "energized distance loop valid");
        }
        require_near(loops[ARDIREC_DISTANCE_L1_E].r, 100.0, 0.2, "L1-E resistance");
        require(std::abs(loops[ARDIREC_DISTANCE_L1_E].x) < 0.2, "L1-E reactance");

        // Static trajectory is source-frame aligned and bounded. Invalid post-open samples remain
        // present as gaps instead of being removed and visually connected across.
        uint32_t locus_count = 0;
        require(ardirec_record_get_distance_locus(
                    handle, ARDIREC_DISTANCE_L1_E, 0, 48, 4000, ARDIREC_VALUE_SECONDARY,
                    0.0, 0.0, minimum_current, nullptr, 0, &locus_count) == 0,
                "distance locus size query");
        require(locus_count == 48, "small locus preserves every source frame");
        std::vector<ardirec_distance_point> locus(locus_count);
        require(ardirec_record_get_distance_locus(
                    handle, ARDIREC_DISTANCE_L1_E, 0, 48, 4000, ARDIREC_VALUE_SECONDARY,
                    0.0, 0.0, minimum_current, locus.data(), locus_count, &locus_count) == 0,
                "distance locus copy");
        require(locus.size() == 48, "locus point count");
        require(locus[22].valid == 1, "energized locus point valid");
        require_near(locus[22].time_seconds, 0.022, 1.0e-12, "locus timestamp");
        require_near(locus[22].r, 100.0, 0.2, "locus resistance");
        require(locus.back().valid == 0, "post-open low-current locus gap retained");

        uint32_t decimated_count = 0;
        require(ardirec_record_get_distance_locus(
                    handle, ARDIREC_DISTANCE_L1_E, 0, 48, 16, ARDIREC_VALUE_SECONDARY,
                    0.0, 0.0, minimum_current, nullptr, 0, &decimated_count) == 0,
                "bounded locus size query");
        require(decimated_count <= 16 && decimated_count >= 2, "bounded locus point budget");
        std::vector<ardirec_distance_point> decimated(decimated_count);
        require(ardirec_record_get_distance_locus(
                    handle, ARDIREC_DISTANCE_L1_E, 0, 48, 16, ARDIREC_VALUE_SECONDARY,
                    0.0, 0.0, minimum_current, decimated.data(), decimated_count, &decimated_count) == 0,
                "bounded locus copy");
        require(decimated.front().reference_frame == 0, "bounded locus preserves first source frame");
        require(decimated.back().reference_frame == 47, "bounded locus preserves last source frame");

        ardirec_record_close(handle);
        std::cout << "ardirec native bridge smoke: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec native bridge smoke: FAIL: " << ex.what() << '\n';
        return 1;
    }
}