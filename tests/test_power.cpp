// SPDX-License-Identifier: GPL-3.0-or-later
#include "ardirec/power/harmonics.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

std::vector<double> cycle_times(std::size_t samplesPerCycle, double frequencyHz) {
    std::vector<double> times;
    times.reserve(samplesPerCycle);
    const double sampleRate = static_cast<double>(samplesPerCycle) * frequencyHz;
    for (std::size_t i = 0; i < samplesPerCycle; ++i) {
        times.push_back(static_cast<double>(i) / sampleRate);
    }
    return times;
}
} // namespace

int main() {
    try {
        constexpr double frequency = 50.0;
        constexpr std::size_t sampleCount = 128;
        const auto times = cycle_times(sampleCount, frequency);

        std::vector<double> mixed;
        mixed.reserve(sampleCount);
        for (const double t : times) {
            mixed.push_back(std::sqrt(2.0)
                            * (100.0 * std::cos(2.0 * kPi * frequency * t)
                               + 10.0 * std::cos(2.0 * kPi * 3.0 * frequency * t)
                               + 5.0 * std::cos(2.0 * kPi * 5.0 * frequency * t)));
        }

        const auto spectrum = ardirec::power::harmonic_spectrum(mixed, times, frequency, 15, 0.0);
        require(spectrum.valid, "known harmonic spectrum is valid");
        require(spectrum.bins.size() == 15, "requested harmonic bin count");
        require_near(spectrum.fundamental_rms, 100.0, 1.0e-9, "H1 RMS magnitude");
        require_near(spectrum.bins[2].magnitude_rms, 10.0, 1.0e-9, "H3 RMS magnitude");
        require_near(spectrum.bins[4].magnitude_rms, 5.0, 1.0e-9, "H5 RMS magnitude");
        require_near(spectrum.thd_percent, 11.180339887498949, 1.0e-9, "THD literal result");
        require(spectrum.dominant_order == 3, "dominant non-fundamental harmonic is H3");
        require_near(spectrum.dominant_rms, 10.0, 1.0e-9, "dominant harmonic RMS");
        require_near(spectrum.dominant_percent, 10.0, 1.0e-9, "dominant harmonic percent H1");

        std::vector<double> clean;
        clean.reserve(sampleCount);
        for (const double t : times) {
            clean.push_back(std::sqrt(2.0) * 80.0 * std::cos(2.0 * kPi * frequency * t));
        }
        const auto cleanSpectrum = ardirec::power::harmonic_spectrum(clean, times, frequency, 15, 0.0);
        require(cleanSpectrum.valid, "clean sine spectrum is valid");
        require_near(cleanSpectrum.fundamental_rms, 80.0, 1.0e-9, "clean sine H1 RMS");
        require(cleanSpectrum.thd_percent < 1.0e-9, "clean sine has near-zero THD");
        require(cleanSpectrum.dominant_order == 0, "clean sine has no meaningful dominant distortion harmonic");

        auto withMissing = mixed;
        withMissing[17] = std::numeric_limits<double>::quiet_NaN();
        const auto missingSpectrum = ardirec::power::harmonic_spectrum(withMissing, times, frequency, 15, 0.0);
        require(missingSpectrum.valid, "spectrum tolerates isolated non-finite sample");
        require(std::isfinite(missingSpectrum.fundamental_rms), "missing-sample fundamental remains finite");
        require(std::isfinite(missingSpectrum.thd_percent), "missing-sample THD remains finite");

        std::cout << "ardirec power tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec power tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
