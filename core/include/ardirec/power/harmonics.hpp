// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <span>
#include <vector>

namespace ardirec::power {

struct HarmonicBin {
    int order{};
    double magnitude_rms{};
    double angle_degrees{};
};

struct HarmonicSpectrum {
    bool valid{};
    double fundamental_rms{};
    double thd_percent{};
    int dominant_order{};
    double dominant_rms{};
    double dominant_percent{};
    std::vector<HarmonicBin> bins;
};

[[nodiscard]] HarmonicSpectrum harmonic_spectrum(std::span<const double> samples,
                                                 std::span<const double> times_seconds,
                                                 double nominal_frequency_hz,
                                                 int maximum_order,
                                                 double reference_time_seconds = 0.0);

} // namespace ardirec::power
