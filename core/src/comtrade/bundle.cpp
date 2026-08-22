// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/comtrade/bundle.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace ardirec::comtrade {
namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::filesystem::path sibling(const std::filesystem::path& base, const char* ext) {
    auto exact = base;
    exact.replace_extension(ext);
    if (std::filesystem::exists(exact)) return exact;

    const auto dir = base.has_parent_path() ? base.parent_path() : std::filesystem::path(".");
    const std::string wanted_stem = lower(base.stem().string());
    const std::string wanted_ext = lower(ext);
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto p = entry.path();
        if (lower(p.stem().string()) == wanted_stem && lower(p.extension().string()) == wanted_ext) {
            return p;
        }
    }
    return {};
}
} // namespace

FileBundle locate_bundle(const std::filesystem::path& selected_file) {
    FileBundle b;
    auto base = selected_file;
    if (lower(base.extension().string()) != ".cfg") base.replace_extension(".cfg");
    b.cfg = sibling(base, ".cfg");
    b.dat = sibling(base, ".dat");
    b.hdr = sibling(base, ".hdr");
    b.inf = sibling(base, ".inf");
    b.dmf = sibling(base, ".dmf");
    return b;
}

} // namespace ardirec::comtrade
