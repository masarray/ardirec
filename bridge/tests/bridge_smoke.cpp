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
        const auto capabilities = ardirec_bridge_capabilities();
        require((capabilities & ARDIREC_BRIDGE_CAP_CURSOR_MEASUREMENT) != 0, "cursor capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_CHANNEL_SEMANTICS) != 0, "semantics capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_VALUE_REPRESENTATION) != 0, "representation capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_STATUS_STATE) != 0, "status-state capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_DIGITAL_EDGE_SNAP) != 0, "edge-snap capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_PHASOR) != 0, "phasor capability");
        require((capabilities & ARDIREC_BRIDGE_CAP_HARMONICS) != 0, "harmonics capability");

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
        require(semantics.role == ARDIREC_ANALOG_CURRENT, "current role");
        require(semantics.phase_role == ARDIREC_PHASE_L1, "L1 phase role");
        require(semantics.recorded_representation == ARDIREC_VALUE_PRIMARY, "recorded primary");
        require(semantics.has_valid_transformer_ratio == 1, "valid ratio semantics");
        require(std::abs(semantics.scale_to_primary - 1.0) < 1.0e-12, "primary scale");
        require(std::abs(semantics.scale_to_secondary - 0.0005) < 1.0e-12, "secondary scale");

        double secondary_scale = 0.0;
        require(ardirec_record_get_representation_scale(
                    handle, 0, ARDIREC_VALUE_SECONDARY, &secondary_scale) == 0,
                "secondary representation scale");
        require(std::abs(secondary_scale - 0.0005) < 1.0e-12, "secondary scale API");

        std::vector<double> analog(info.frame_count);
        require(ardirec_record_copy_analog(handle, 0, 0, info.frame_count, analog.data()) == 0, "analog samples");
        require(std::abs(analog[1] - 10.0) < 1e-12, "analog value");

        ardirec_cursor_measurement_info measurement{};
        require(ardirec_record_get_cursor_measurement(
                    handle, 0, 3, ARDIREC_VALUE_SECONDARY, &measurement) == 0,
                "cursor measurement");
        require(measurement.valid == 1, "cursor measurement valid");
        require(measurement.reference_frame == 3, "cursor reference frame");
        require(measurement.raw_timestamp == 3000, "cursor raw timestamp");
        require(std::abs(measurement.time_seconds - 0.003) < 1.0e-12, "cursor time");
        require(std::abs(measurement.instantaneous - 0.0025) < 1.0e-12, "cursor instantaneous secondary");
        const double expected_rms_secondary = std::sqrt(1025.0 / 4.0) / 2000.0;
        require(std::abs(measurement.rms - expected_rms_secondary) < 1.0e-12, "cursor RMS secondary");
        require(measurement.window_start_frame == 0 && measurement.window_end_exclusive == 4,
                "cursor measurement window");
        require(measurement.window_sample_count == 4, "cursor measurement sample count");

        std::vector<uint8_t> status(info.frame_count);
        require(ardirec_record_copy_status(handle, 0, 0, info.frame_count, status.data()) == 0, "status samples");
        require(status[2] == 1u, "status value");

        ardirec_status_state_info state{};
        require(ardirec_record_get_status_state(handle, 0, 2, &state) == 0, "status state");
        require(state.raw_state == 1 && state.normal_state == 0 && state.is_active == 1,
                "active-high state semantics");

        ardirec_status_edge_info edge{};
        require(ardirec_record_find_nearest_status_edge(handle, 1, 0.002, &edge) == 0,
                "nearest status edge");
        require(edge.valid == 1, "nearest edge valid");
        require(edge.frame_index == 2 && edge.channel_index == 0, "nearest edge identity");
        require(edge.before_state == 0 && edge.after_state == 1 && edge.became_active == 1,
                "nearest edge active-high semantics");
        require(std::abs(edge.distance_seconds - 0.001) < 1.0e-12, "nearest edge distance");

        std::vector<uint32_t> timestamps(info.frame_count);
        require(ardirec_record_copy_raw_timestamps(handle, 0, info.frame_count, timestamps.data()) == 0, "timestamps");

        require(ardirec_record_copy_analog(handle, 99, 0, 1, analog.data()) < 0, "channel bounds");
        require(ardirec_record_copy_status(handle, 0, info.frame_count, 1, status.data()) < 0, "frame bounds");
        require(ardirec_record_get_representation_scale(handle, 0, 99, &secondary_scale) < 0,
                "representation enum bounds");
        ardirec_record_close(handle);
        handle = nullptr;

        // Explicit active-low fixture: normal=1, asserted state=0. The bridge must
        // never infer Pickup/Trip activity from raw bit direction alone.
        const auto active_low_cfg = std::filesystem::path(ARDIREC_BRIDGE_TEST_DATA_DIR) / "active_low_1999.cfg";
        const auto active_low_path = utf8_string(active_low_cfg);
        require(ardirec_record_open_utf8(active_low_path.c_str(), &handle, error, sizeof(error)) == 0, error);
        require(handle != nullptr, "active-low record handle");

        require(ardirec_record_get_status_state(handle, 0, 2, &state) == 0, "active-low asserted state");
        require(state.raw_state == 0 && state.normal_state == 1 && state.is_active == 1,
                "active-low assert semantics");
        require(ardirec_record_find_nearest_status_edge(handle, 1, 0.002, &edge) == 0,
                "active-low assert edge");
        require(edge.valid == 1 && edge.frame_index == 2 && edge.became_active == 1,
                "active-low assert edge semantics");
        require(ardirec_record_find_nearest_status_edge(handle, 3, 0.0001, &edge) == 0,
                "active-low reset edge");
        require(edge.valid == 1 && edge.frame_index == 3 && edge.became_active == 0,
                "active-low reset edge semantics");
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

        require(ardirec_record_get_cursor_measurement(
                    handle, 0, 20, ARDIREC_VALUE_RECORDED, &measurement) == 0,
                "clean sine cursor measurement");
        require(measurement.valid == 1, "clean sine measurement valid");
        require(std::abs(measurement.instantaneous - 70.710678) < 1.0e-5, "clean sine instantaneous");
        require(std::abs(measurement.rms - 50.0) < 1.0e-4, "clean sine true RMS");

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
