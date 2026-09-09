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
}

int main() {
    try {
        require(ardirec_bridge_abi_version() == ARDIREC_BRIDGE_ABI_VERSION, "ABI version");

        const auto cfg = std::filesystem::path(ARDIREC_BRIDGE_TEST_DATA_DIR) / "minimal_1999.cfg";
        const auto cfg_utf8 = cfg.u8string();
        const std::string cfg_path(reinterpret_cast<const char*>(cfg_utf8.data()), cfg_utf8.size());

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
        std::cout << "ardirec native bridge smoke: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec native bridge smoke: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
