// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/power/harmonics.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

namespace ardirec::power {
namespace {
constexpr long double kPi = 3.141592653589793238462643383279502884L;
constexpr double kMinimumMagnitude = 1.0e-12;

double wrap_degrees(double angle) {
    while (angle <= -180.0) angle += 360.0;
    while (angle > 180.0) angle -= 360.0;
    return angle;
}

double estimate_sample_rate(std::span<const double> times_seconds) {
    std::vector<double> deltas;
    deltas.reserve(times_seconds.size());
    for (std::size_t i = 1; i < times_seconds.size(); ++i) {
        const double previous = times_seconds[i - 1];
        const double current = times_seconds[i];
        const double delta = current - previous;
        if (std::isfinite(previous) && std::isfinite(current) && delta > 0.0) {
            deltas.push_back(delta);
        }
    }
    if (deltas.empty()) return 0.0;
    const auto middle = deltas.begin() + static_cast<std::ptrdiff_t>(deltas.size() / 2);
    std::nth_element(deltas.begin(), middle, deltas.end());
    double median = *middle;
    if (deltas.size() % 2 == 0 && middle != deltas.begin()) {
        const auto lower = std::max_element(deltas.begin(), middle);
        median = (*lower + *middle) * 0.5;
    }
    return median > 0.0 ? 1.0 / median : 0.0;
}
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

    result.estimated_sample_rate_hz = estimate_sample_rate(times_seconds.first(sample_count));
    if (!std::isfinite(result.estimated_sample_rate_hz) || result.estimated_sample_rate_hz <= 0.0) {
        return result;
    }

    const double nyquist_hz = result.estimated_sample_rate_hz * 0.5;
    result.maximum_resolvable_order = std::max(
        0, static_cast<int>(std::floor((nyquist_hz + nominal_frequency_hz * 1.0e-9)
                                       / nominal_frequency_hz)));
    maximum_order = std::min({maximum_order, 50, result.maximum_resolvable_order});
    if (maximum_order < 1) return result;

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
    result.bins.reserve(static_cast<std::size_t>(maximum_order));
    for (int order = 1; order <= maximum_order; ++order) {
        const double harmonic_hz = nominal_frequency_hz * static_cast<double>(order);
        const bool at_nyquist = std::abs(harmonic_hz - nyquist_hz)
                                <= std::max(1.0e-9, nyquist_hz * 1.0e-9);
        const long double rms_scale = (at_nyquist ? 1.0L : std::sqrt(2.0L))
                                      / static_cast<long double>(finite_count);
        const auto value = accumulators[static_cast<std::size_t>(order - 1)] * rms_scale;
        const double magnitude = static_cast<double>(std::abs(value));
        // DFT coefficients are cosine-referenced. Protection tools such as SIGRA
        // report sine-wave phase position at the reference instant, hence +90 deg.
        const double raw_angle = static_cast<double>(std::atan2(value.imag(), value.real())
                                                       * 180.0L / kPi);
        const double angle_degrees = wrap_degrees(raw_angle + 90.0);
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
