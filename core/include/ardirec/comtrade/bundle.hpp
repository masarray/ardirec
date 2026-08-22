// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ardirec/comtrade/record.hpp"

#include <filesystem>

namespace ardirec::comtrade {

[[nodiscard]] FileBundle locate_bundle(const std::filesystem::path& selected_file);

} // namespace ardirec::comtrade
