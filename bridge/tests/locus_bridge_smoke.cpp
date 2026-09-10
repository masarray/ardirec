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

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

std::string utf8_string(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}
}

int main() {
    ardirec_record_handle handle = nullptr;
    try {
        const auto cfg = std::filesystem::path(ARDIREC_BRIDGE_TEST_DATA_DIR) / "distance_p1.cfg";
        const auto cfg_path = utf8_string(cfg);
        char error[512]{};
        require(ardirec_record_open_utf8(cfg_path.c_str(), &handle, error, sizeof(error)) == 0, error);
        require(handle != nullptr, "distance fixture handle");

        ardirec_record_info info{};
        require(ardirec_record_get_info(handle, &info) == 0, "distance record info");
        require(info.frame_count == 48, "distance fixture frame count");

        double floor = 0.0;
        require(ardirec_record_get_distance_current_floor(
                    handle, ARDIREC_VALUE_RECORDED, &floor) == 0,
                "distance current floor");
        require_near(floor, 0.001, 1.0e-12,
                     "distance current floor matches established ArdIrec 0.1 percent rule");

        ardirec_distance_point loops[6]{};
        require(ardirec_record_get_distance_loops(
                    handle,
                    22,
                    ARDIREC_VALUE_RECORDED,
                    0.0,
                    0.0,
                    floor,
                    loops,
                    6) == 0,
                "batched distance cursor loops");

        require(loops[ARDIREC_DISTANCE_L1_E].valid == 1, "L1-E cursor valid");
        require_near(loops[ARDIREC_DISTANCE_L1_E].r, 100.0, 0.2, "L1-E R parity");
        require(std::abs(loops[ARDIREC_DISTANCE_L1_E].x) < 0.2, "L1-E X parity");
        require(loops[ARDIREC_DISTANCE_L1_L2].valid == 1, "L1-L2 phase locus cursor valid");
        require(loops[ARDIREC_DISTANCE_L2_L3].valid == 1, "L2-L3 phase locus cursor valid");
        require(loops[ARDIREC_DISTANCE_L3_L1].valid == 1, "L3-L1 phase locus cursor valid");
        require_near(loops[ARDIREC_DISTANCE_L1_E].time_seconds, 0.022, 1.0e-12,
                     "cursor point retains COMTRADE time");

        uint32_t required = 0;
        require(ardirec_record_get_distance_locus(
                    handle,
                    ARDIREC_DISTANCE_L1_L2,
                    0,
                    info.frame_count,
                    16,
                    ARDIREC_VALUE_RECORDED,
                    0.0,
                    0.0,
                    floor,
                    nullptr,
                    0,
                    &required) == 0,
                "phase locus bounded query");
        require(required > 1 && required <= 16, "phase locus respects point budget");

        std::vector<ardirec_distance_point> locus(required);
        uint32_t copied = 0;
        require(ardirec_record_get_distance_locus(
                    handle,
                    ARDIREC_DISTANCE_L1_L2,
                    0,
                    info.frame_count,
                    16,
                    ARDIREC_VALUE_RECORDED,
                    0.0,
                    0.0,
                    floor,
                    locus.data(),
                    static_cast<uint32_t>(locus.size()),
                    &copied) == 0,
                "phase locus copy");
        require(copied == required, "phase locus copied count");
        require(locus.front().reference_frame == 0, "phase locus keeps first frame");
        require(locus.back().reference_frame == info.frame_count - 1, "phase locus keeps final frame");

        bool saw_valid_phase_point = false;
        for (const auto& point : locus) {
            require(point.loop == ARDIREC_DISTANCE_L1_L2, "phase locus loop identity");
            if (point.valid != 0) saw_valid_phase_point = true;
        }
        require(saw_valid_phase_point, "phase locus contains valid impedance points");

        ardirec_record_close(handle);
        handle = nullptr;
        std::cout << "ardirec locus bridge smoke: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        if (handle != nullptr) ardirec_record_close(handle);
        std::cerr << "ardirec locus bridge smoke: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
