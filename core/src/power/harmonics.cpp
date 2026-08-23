// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/power/harmonics.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace ardirec::power {
namespace {
constexpr long double kPi = 3.141592653589793238462643383279502884L;
constexpr double kMinimumMagnitude = 1.0e-12;
} // namespace

HarmonicSpectrum harmonic_spectrum(std::span<const double> samples,
                                   std::span<const double> times_seconds,
                                   double nominal_frequency_hz,
                                   int maximum_order,
                                   double reference_time_seconds) {
    HarmonicSpectrum result;
    const std::size_t sample_count = std::min(samples.size(), times_seconds.size());
    if (sample_count < 4 || !std::isfinite(nominal_frequency_hz) || nominal_frequency_hz <= 0.0
        || !std::isfinite(reference_time_seconds) || maximum_order < 1) {
        return result;
    }

    maximum_order = std::clamp(maximum_order, 1, 50);
    std::vector<std::complex<long double>> accumulators(static_cast<std::size_t>(maximum_order),
                                                         std::complex<long double>{0.0L, 0.0L});
    std::size_t finite_count = 0;
    long double dc_sum = 0.0L;
    const long double omega = 2.0L * kPi * static_cast<long double>(nominal_frequency_hz);

    for (std::size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        const double sample = samples[sample_index];
        const double time = times_seconds[sample_index];
        if (!std::isfinite(sample) || !std::isfinite(time)) continue;

        dc_sum += static_cast<long double>(sample);
        const long double angle = -omega
                                  * static_cast<long double>(time - reference_time_seconds);
        const std::complex<long double> fundamental_basis{std::cos(angle), std::sin(angle)};
        std::complex<long double> harmonic_basis = fundamental_basis;
        const long double value = static_cast<long double>(sample);

        for (int order = 0; order < maximum_order; ++order) {
            accumulators[static_cast<std::size_t>(order)] += value * harmonic_basis;
            harmonic_basis *= fundamental_basis;
        }
        ++finite_count;
    }

    if (finite_count < 4) return result;

    result.dc_component = static_cast<double>(dc_sum / static_cast<long double>(finite_count));
    const long double rms_scale = std::sqrt(2.0L) / static_cast<long double>(finite_count);
    result.bins.reserve(static_cast<std::size_t>(maximum_order));
    for (int order = 1; order <= maximum_order; ++order) {
        const auto value = accumulators[static_cast<std::size_t>(order - 1)] * rms_scale;
        const double magnitude = static_cast<double>(std::abs(value));
        const double angle_degrees = static_cast<double>(std::atan2(value.imag(), value.real())
                                                          * 180.0L / kPi);
        result.bins.push_back(HarmonicBin{order, magnitude, angle_degrees});
    }

    result.valid = true;
    result.fundamental_rms = result.bins.front().magnitude_rms;
    long double distortion_squared = 0.0L;
    double largest_distortion = 0.0;
    int largest_order = 0;

    for (std::size_t index = 1; index < result.bins.size(); ++index) {
        const double magnitude = result.bins[index].magnitude_rms;
        distortion_squared += static_cast<long double>(magnitude) * static_cast<long double>(magnitude);
        if (magnitude > largest_distortion) {
            largest_distortion = magnitude;
            largest_order = result.bins[index].order;
        }
    }

    if (result.fundamental_rms > kMinimumMagnitude) {
        result.thd_percent = std::sqrt(static_cast<double>(distortion_squared))
                             / result.fundamental_rms * 100.0;
        const double meaningful_threshold = std::max(kMinimumMagnitude, result.fundamental_rms * 1.0e-10);
        if (largest_distortion > meaningful_threshold) {
            result.dominant_order = largest_order;
            result.dominant_rms = largest_distortion;
            result.dominant_percent = largest_distortion / result.fundamental_rms * 100.0;
        }
    }

    return result;
}

} // namespace ardirec::power
