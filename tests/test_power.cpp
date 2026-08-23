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
        require_near(spectrum.dc_component, 0.0, 1.0e-9, "zero-mean waveform DC");
        require_near(spectrum.fundamental_rms, 100.0, 1.0e-9, "H1 RMS magnitude");
        require_near(spectrum.bins[2].magnitude_rms, 10.0, 1.0e-9, "H3 RMS magnitude");
        require_near(spectrum.bins[4].magnitude_rms, 5.0, 1.0e-9, "H5 RMS magnitude");
        require_near(spectrum.thd_percent, 11.180339887498949, 1.0e-9, "THD literal result");
        require(spectrum.dominant_order == 3, "dominant non-fundamental harmonic is H3");
        require_near(spectrum.dominant_rms, 10.0, 1.0e-9, "dominant harmonic RMS");
        require_near(spectrum.dominant_percent, 10.0, 1.0e-9, "dominant harmonic percent H1");

        std::vector<double> withDc;
        withDc.reserve(sampleCount);
        for (const double t : times) {
            withDc.push_back(12.5
                             + std::sqrt(2.0)
                                   * (70.0 * std::cos(2.0 * kPi * frequency * t)
                                      + 7.0 * std::cos(2.0 * kPi * 2.0 * frequency * t)));
        }
        const auto dcSpectrum = ardirec::power::harmonic_spectrum(withDc, times, frequency, 10, 0.0);
        require(dcSpectrum.valid, "DC harmonic spectrum is valid");
        require_near(dcSpectrum.dc_component, 12.5, 1.0e-9, "H0 DC uses arithmetic mean without sqrt(2)");
        require_near(dcSpectrum.fundamental_rms, 70.0, 1.0e-9, "DC fixture H1 RMS");
        require_near(dcSpectrum.bins[1].magnitude_rms, 7.0, 1.0e-9, "DC fixture H2 RMS");
        require_near(dcSpectrum.thd_percent, 10.0, 1.0e-9, "THD excludes DC and H1");

        std::vector<double> clean;
        clean.reserve(sampleCount);
        for (const double t : times) {
            clean.push_back(std::sqrt(2.0) * 80.0 * std::cos(2.0 * kPi * frequency * t));
        }
        const auto cleanSpectrum = ardirec::power::harmonic_spectrum(clean, times, frequency, 15, 0.0);
        require(cleanSpectrum.valid, "clean sine spectrum is valid");
        require_near(cleanSpectrum.dc_component, 0.0, 1.0e-9, "clean sine DC is zero");
        require_near(cleanSpectrum.fundamental_rms, 80.0, 1.0e-9, "clean sine H1 RMS");
        require(cleanSpectrum.thd_percent < 1.0e-9, "clean sine has near-zero THD");
        require(cleanSpectrum.dominant_order == 0, "clean sine has no meaningful dominant distortion harmonic");

        auto withMissing = mixed;
        withMissing[17] = std::numeric_limits<double>::quiet_NaN();
        const auto missingSpectrum = ardirec::power::harmonic_spectrum(withMissing, times, frequency, 15, 0.0);
        require(missingSpectrum.valid, "spectrum tolerates isolated non-finite sample");
        require(std::isfinite(missingSpectrum.dc_component), "missing-sample DC remains finite");
        require(std::isfinite(missingSpectrum.fundamental_rms), "missing-sample fundamental remains finite");
        require(std::isfinite(missingSpectrum.thd_percent), "missing-sample THD remains finite");

        // SIGRA-compatible phase position: sine phase at the cursor/reference instant.
        // 1 kHz / 50 Hz gives 20 samples/cycle and therefore a Nyquist limit of H10.
        const auto lowRateTimes = cycle_times(20, frequency);
        std::vector<double> lowRate;
        lowRate.reserve(lowRateTimes.size());
        const double h1Phase = 30.0 * kPi / 180.0;
        const double h2Phase = -20.0 * kPi / 180.0;
        for (const double t : lowRateTimes) {
            lowRate.push_back(std::sqrt(2.0)
                              * (100.0 * std::sin(2.0 * kPi * frequency * t + h1Phase)
                                 + 10.0 * std::sin(2.0 * kPi * 2.0 * frequency * t + h2Phase)));
        }
        const auto lowRateSpectrum = ardirec::power::harmonic_spectrum(lowRate, lowRateTimes,
                                                                        frequency, 25, 0.0);
        require(lowRateSpectrum.valid, "low-rate spectrum is valid");
        require_near(lowRateSpectrum.estimated_sample_rate_hz, 1000.0, 1.0e-6,
                     "sample rate estimated from timestamps");
        require(lowRateSpectrum.maximum_resolvable_order == 10, "Nyquist supports only H10");
        require(lowRateSpectrum.bins.size() == 10, "requested H25 is clipped to H10 at 1 kHz / 50 Hz");
        require_near(lowRateSpectrum.fundamental_rms, 100.0, 1.0e-9, "low-rate H1 RMS");
        require_near(lowRateSpectrum.bins[1].magnitude_rms, 10.0, 1.0e-9, "low-rate H2 RMS");
        require_near(lowRateSpectrum.bins[0].angle_degrees, 30.0, 1.0e-9,
                     "H1 sine phase at reference instant");
        require_near(lowRateSpectrum.bins[1].angle_degrees, -20.0, 1.0e-9,
                     "H2 sine phase at reference instant");
        require_near(lowRateSpectrum.thd_percent, 10.0, 1.0e-8,
                     "THD does not double-count aliased H11-H25");

        const auto shiftedPhase = ardirec::power::harmonic_spectrum(lowRate, lowRateTimes,
                                                                     frequency, 10, 0.005);
        require_near(shiftedPhase.bins[0].angle_degrees, 120.0, 1.0e-9,
                     "H1 phase advances 90 degrees over 5 ms at 50 Hz");
        require_near(shiftedPhase.bins[1].angle_degrees, 160.0, 1.0e-9,
                     "H2 phase advances twice the fundamental phase");

        std::cout << "ardirec power tests: PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ardirec power tests: FAIL: " << ex.what() << '\n';
        return 1;
    }
}
