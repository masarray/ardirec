// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
  #if defined(ARDIREC_BRIDGE_EXPORTS)
    #define ARDIREC_BRIDGE_API __declspec(dllexport)
  #else
    #define ARDIREC_BRIDGE_API __declspec(dllimport)
  #endif
#else
  #define ARDIREC_BRIDGE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ARDIREC_BRIDGE_ABI_VERSION 1u
#define ARDIREC_BRIDGE_TEXT_SMALL 64u
#define ARDIREC_BRIDGE_TEXT_MEDIUM 128u
#define ARDIREC_BRIDGE_TEXT_LARGE 256u

typedef void* ardirec_record_handle;

typedef struct ardirec_record_info {
    uint32_t abi_version;
    int32_t revision_year;
    int32_t data_format;
    uint32_t analog_count;
    uint32_t status_count;
    uint64_t frame_count;
    double nominal_frequency;
    double time_multiplier;
    char station_name[ARDIREC_BRIDGE_TEXT_LARGE];
    char recorder_id[ARDIREC_BRIDGE_TEXT_LARGE];
    char start_time[ARDIREC_BRIDGE_TEXT_MEDIUM];
    char trigger_time[ARDIREC_BRIDGE_TEXT_MEDIUM];
} ardirec_record_info;

typedef struct ardirec_analog_channel_info {
    int32_t index;
    double a;
    double b;
    double skew_us;
    double min_value;
    double max_value;
    double primary;
    double secondary;
    uint8_t has_primary;
    uint8_t has_secondary;
    char id[ARDIREC_BRIDGE_TEXT_MEDIUM];
    char phase[ARDIREC_BRIDGE_TEXT_SMALL];
    char circuit[ARDIREC_BRIDGE_TEXT_MEDIUM];
    char units[ARDIREC_BRIDGE_TEXT_SMALL];
    char primary_secondary[ARDIREC_BRIDGE_TEXT_SMALL];
} ardirec_analog_channel_info;

typedef struct ardirec_status_channel_info {
    int32_t index;
    int32_t normal_state;
    char id[ARDIREC_BRIDGE_TEXT_MEDIUM];
    char phase[ARDIREC_BRIDGE_TEXT_SMALL];
    char circuit[ARDIREC_BRIDGE_TEXT_MEDIUM];
} ardirec_status_channel_info;

// Fundamental RMS phasor at a reference frame. The analysis window is the
// nominal-frequency cycle ending at reference_frame and uses the same
// cosine-referenced phase convention as ArdIrec's Phasor view.
typedef struct ardirec_phasor_info {
    int32_t valid;
    double magnitude_rms;
    double angle_degrees;
    double real;
    double imag;
    uint64_t window_start_frame;
    uint64_t window_end_exclusive;
} ardirec_phasor_info;

typedef struct ardirec_harmonic_bin {
    int32_t order;
    double magnitude_rms;
    double percent_of_fundamental;
    double angle_degrees;
} ardirec_harmonic_bin;

typedef struct ardirec_harmonic_spectrum_info {
    int32_t valid;
    double dc_component;
    double fundamental_rms;
    double thd_percent;
    int32_t dominant_order;
    double dominant_rms;
    double dominant_percent;
    double estimated_sample_rate_hz;
    int32_t maximum_resolvable_order;
    uint32_t bin_count;
    uint64_t window_start_frame;
    uint64_t window_end_exclusive;
} ardirec_harmonic_spectrum_info;

// Return codes: 0 success, negative values are bridge errors.
ARDIREC_BRIDGE_API uint32_t ardirec_bridge_abi_version(void);
ARDIREC_BRIDGE_API int32_t ardirec_record_open_utf8(
    const char* cfg_path_utf8,
    ardirec_record_handle* out_handle,
    char* error_utf8,
    size_t error_capacity);
ARDIREC_BRIDGE_API void ardirec_record_close(ardirec_record_handle handle);
ARDIREC_BRIDGE_API int32_t ardirec_record_get_info(
    ardirec_record_handle handle,
    ardirec_record_info* out_info);
ARDIREC_BRIDGE_API int32_t ardirec_record_get_analog_channel(
    ardirec_record_handle handle,
    uint32_t channel_index,
    ardirec_analog_channel_info* out_info);
ARDIREC_BRIDGE_API int32_t ardirec_record_get_status_channel(
    ardirec_record_handle handle,
    uint32_t channel_index,
    ardirec_status_channel_info* out_info);
ARDIREC_BRIDGE_API int32_t ardirec_record_copy_analog(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t start_frame,
    uint64_t frame_count,
    double* destination);
ARDIREC_BRIDGE_API int32_t ardirec_record_copy_status(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t start_frame,
    uint64_t frame_count,
    uint8_t* destination);
ARDIREC_BRIDGE_API int32_t ardirec_record_copy_raw_timestamps(
    ardirec_record_handle handle,
    uint64_t start_frame,
    uint64_t frame_count,
    uint32_t* destination);
ARDIREC_BRIDGE_API int32_t ardirec_record_get_phasor(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t reference_frame,
    ardirec_phasor_info* out_info);
// Call once with bins=NULL/bin_capacity=0 to query bin_count, then again with
// a buffer of at least bin_count entries to copy the complete spectrum.
ARDIREC_BRIDGE_API int32_t ardirec_record_get_harmonic_spectrum(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t reference_frame,
    int32_t maximum_order,
    ardirec_harmonic_spectrum_info* out_info,
    ardirec_harmonic_bin* bins,
    uint32_t bin_capacity);

#ifdef __cplusplus
}
#endif
