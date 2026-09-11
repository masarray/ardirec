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

#define ARDIREC_BRIDGE_CAP_CURSOR_MEASUREMENT   (1ull << 0)
#define ARDIREC_BRIDGE_CAP_CHANNEL_SEMANTICS    (1ull << 1)
#define ARDIREC_BRIDGE_CAP_VALUE_REPRESENTATION (1ull << 2)
#define ARDIREC_BRIDGE_CAP_STATUS_STATE         (1ull << 3)
#define ARDIREC_BRIDGE_CAP_DIGITAL_EDGE_SNAP    (1ull << 4)
#define ARDIREC_BRIDGE_CAP_PHASOR               (1ull << 5)
#define ARDIREC_BRIDGE_CAP_HARMONICS             (1ull << 6)
#define ARDIREC_BRIDGE_CAP_DISTANCE_LOCUS        (1ull << 7)

typedef void* ardirec_record_handle;

typedef enum ardirec_value_representation {
    ARDIREC_VALUE_RECORDED = 0,
    ARDIREC_VALUE_SECONDARY = 1,
    ARDIREC_VALUE_PRIMARY = 2
} ardirec_value_representation;

typedef enum ardirec_analog_role {
    ARDIREC_ANALOG_OTHER = 0,
    ARDIREC_ANALOG_VOLTAGE = 1,
    ARDIREC_ANALOG_CURRENT = 2
} ardirec_analog_role;

typedef enum ardirec_phase_role {
    ARDIREC_PHASE_OTHER = 0,
    ARDIREC_PHASE_L1 = 1,
    ARDIREC_PHASE_L2 = 2,
    ARDIREC_PHASE_L3 = 3,
    ARDIREC_PHASE_NEUTRAL = 4
} ardirec_phase_role;

typedef enum ardirec_distance_loop {
    ARDIREC_DISTANCE_L1_E = 0,
    ARDIREC_DISTANCE_L2_E = 1,
    ARDIREC_DISTANCE_L3_E = 2,
    ARDIREC_DISTANCE_L1_L2 = 3,
    ARDIREC_DISTANCE_L2_L3 = 4,
    ARDIREC_DISTANCE_L3_L1 = 5
} ardirec_distance_loop;

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

typedef struct ardirec_analog_semantics_info {
    int32_t role;
    int32_t phase_role;
    int32_t recorded_representation;
    int32_t has_valid_transformer_ratio;
    double scale_to_secondary;
    double scale_to_primary;
} ardirec_analog_semantics_info;

typedef struct ardirec_cursor_measurement_info {
    int32_t valid;
    uint64_t reference_frame;
    uint32_t raw_timestamp;
    double time_seconds;
    double instantaneous;
    double rms;
    uint64_t window_start_frame;
    uint64_t window_end_exclusive;
    uint32_t window_sample_count;
} ardirec_cursor_measurement_info;

typedef struct ardirec_status_state_info {
    int32_t raw_state;
    int32_t normal_state;
    int32_t is_active;
} ardirec_status_state_info;

typedef struct ardirec_status_edge_info {
    int32_t valid;
    uint32_t channel_index;
    uint64_t frame_index;
    uint32_t raw_timestamp;
    int32_t before_state;
    int32_t after_state;
    int32_t normal_state;
    int32_t became_active;
    double distance_seconds;
} ardirec_status_edge_info;

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

// Layout is intentionally stable for P/Invoke. Invalid points are retained so locus paths can
// preserve time-aligned gaps instead of drawing through a low-current/open-breaker interval.
typedef struct ardirec_distance_point {
    int32_t valid;
    int32_t loop;
    uint64_t reference_frame;
    uint32_t raw_timestamp;
    double time_seconds;
    double r;
    double x;
    double magnitude;
    double angle_degrees;
    double measuring_current;
    double minimum_current;
} ardirec_distance_point;

// Return codes: 0 success, negative values are bridge errors.
ARDIREC_BRIDGE_API uint32_t ardirec_bridge_abi_version(void);
ARDIREC_BRIDGE_API uint64_t ardirec_bridge_capabilities(void);
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
ARDIREC_BRIDGE_API int32_t ardirec_record_get_analog_semantics(
    ardirec_record_handle handle,
    uint32_t channel_index,
    ardirec_analog_semantics_info* out_info);
ARDIREC_BRIDGE_API int32_t ardirec_record_get_representation_scale(
    ardirec_record_handle handle,
    uint32_t channel_index,
    int32_t representation,
    double* out_scale);
ARDIREC_BRIDGE_API int32_t ardirec_record_get_cursor_measurement(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t reference_frame,
    int32_t representation,
    ardirec_cursor_measurement_info* out_info);
ARDIREC_BRIDGE_API int32_t ardirec_record_get_status_state(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t reference_frame,
    ardirec_status_state_info* out_info);
ARDIREC_BRIDGE_API int32_t ardirec_record_find_nearest_status_edge(
    ardirec_record_handle handle,
    uint64_t reference_frame,
    double max_distance_seconds,
    ardirec_status_edge_info* out_info);
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
ARDIREC_BRIDGE_API int32_t ardirec_record_get_harmonic_spectrum(
    ardirec_record_handle handle,
    uint32_t channel_index,
    uint64_t reference_frame,
    int32_t maximum_order,
    ardirec_harmonic_spectrum_info* out_info,
    ardirec_harmonic_bin* bins,
    uint32_t bin_capacity);

ARDIREC_BRIDGE_API int32_t ardirec_record_get_distance_current_floor(
    ardirec_record_handle handle,
    int32_t representation,
    double* out_minimum_current);
// points must contain at least six entries. The six outputs always use the enum ordering above;
// unavailable or invalid loops are returned as valid=0, preserving one stable cursor payload.
ARDIREC_BRIDGE_API int32_t ardirec_record_get_distance_loops(
    ardirec_record_handle handle,
    uint64_t reference_frame,
    int32_t representation,
    double grounding_factor_magnitude,
    double grounding_factor_angle_degrees,
    double minimum_current,
    ardirec_distance_point* points,
    uint32_t point_capacity);
// Query required point count with points=NULL/point_capacity=0. Sampling is source-frame aligned,
// bounded by maximum_points, preserves first/last samples and retains invalid gaps.
ARDIREC_BRIDGE_API int32_t ardirec_record_get_distance_locus(
    ardirec_record_handle handle,
    int32_t loop,
    uint64_t start_frame,
    uint64_t frame_count,
    uint32_t maximum_points,
    int32_t representation,
    double grounding_factor_magnitude,
    double grounding_factor_angle_degrees,
    double minimum_current,
    ardirec_distance_point* points,
    uint32_t point_capacity,
    uint32_t* out_point_count);

#ifdef __cplusplus
}
#endif