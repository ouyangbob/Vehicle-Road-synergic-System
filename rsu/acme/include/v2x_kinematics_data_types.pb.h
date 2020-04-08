/*
 *  Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef PB_V2X_KINEMATICS_DATA_TYPES_PB_H_INCLUDED
#define PB_V2X_KINEMATICS_DATA_TYPES_PB_H_INCLUDED
#include <pb.h>

/* @@protoc_insertion_point(includes) */
#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _v2x_fix_mode_t {
    V2X_GNSS_MODE_NOT_SEEN = 0,
    V2X_GNSS_MODE_NO_FIX = 1,
    V2X_GNSS_MODE_2D = 2,
    V2X_GNSS_MODE_3D = 3
} v2x_fix_mode_t;
#define _v2x_fix_mode_t_MIN V2X_GNSS_MODE_NOT_SEEN
#define _v2x_fix_mode_t_MAX V2X_GNSS_MODE_3D
#define _v2x_fix_mode_t_ARRAYSIZE ((v2x_fix_mode_t)(V2X_GNSS_MODE_3D+1))
#define v2x_fix_mode_t_V2X_GNSS_MODE_NOT_SEEN V2X_GNSS_MODE_NOT_SEEN
#define v2x_fix_mode_t_V2X_GNSS_MODE_NO_FIX V2X_GNSS_MODE_NO_FIX
#define v2x_fix_mode_t_V2X_GNSS_MODE_2D V2X_GNSS_MODE_2D
#define v2x_fix_mode_t_V2X_GNSS_MODE_3D V2X_GNSS_MODE_3D

/* Struct definitions */
typedef struct _v2x_msg_t_disable_fixes_tag {
    char dummy_field;
/* @@protoc_insertion_point(struct:v2x_msg_t_disable_fixes_tag) */
} v2x_msg_t_disable_fixes_tag;

typedef struct _v2x_msg_t_enable_fixes_tag {
    char dummy_field;
/* @@protoc_insertion_point(struct:v2x_msg_t_enable_fixes_tag) */
} v2x_msg_t_enable_fixes_tag;

typedef struct _v2x_GNSSstatus_t {
    bool unavailable;
    bool aPDOPofUnder5;
    bool inViewOfUnder5;
    bool localCorrectionsPresent;
    bool networkCorrectionsPresent;
/* @@protoc_insertion_point(struct:v2x_GNSSstatus_t) */
} v2x_GNSSstatus_t;

typedef struct _v2x_gnss_fix_rates_supported_list_t {
    uint32_t qty_rates_supported;
    pb_size_t rates_supported_hz_array_count;
    uint32_t rates_supported_hz_array[32];
/* @@protoc_insertion_point(struct:v2x_gnss_fix_rates_supported_list_t) */
} v2x_gnss_fix_rates_supported_list_t;

typedef struct _v2x_init_t {
    uint32_t log_level_mask;
    char server_ip_addr[32];
/* @@protoc_insertion_point(struct:v2x_init_t) */
} v2x_init_t;

typedef struct _v2x_kinematics_capabilities_t_feature_flags_t {
    bool has_3_axis_gyro;
    bool has_3_axis_accelerometer;
    bool has_imu_supplemented_dead_reckoning;
    bool has_yaw_rate_sensor;
    bool used_vehicle_speed;
    bool used_single_wheel_ticks;
    bool used_front_differential_wheel_ticks;
    bool used_rear_differential_wheel_ticks;
    bool used_vehicle_dynamic_model;
/* @@protoc_insertion_point(struct:v2x_kinematics_capabilities_t_feature_flags_t) */
} v2x_kinematics_capabilities_t_feature_flags_t;

typedef struct _v2x_rates_t {
    uint32_t rate_report_hz;
    uint32_t offset_nanoseconds;
/* @@protoc_insertion_point(struct:v2x_rates_t) */
} v2x_rates_t;

typedef struct _v2x_kinematics_capabilities_t {
    v2x_kinematics_capabilities_t_feature_flags_t feature_flags;
    uint32_t max_fix_rate_supported_hz;
    v2x_gnss_fix_rates_supported_list_t rates_list;
/* @@protoc_insertion_point(struct:v2x_kinematics_capabilities_t) */
} v2x_kinematics_capabilities_t;

typedef struct _v2x_location_fix_t {
    double utc_fix_time;
    v2x_fix_mode_t fix_mode;
    double latitude;
    double longitude;
    double altitude;
    uint32_t qty_SV_in_view;
    uint32_t qty_SV_used;
    v2x_GNSSstatus_t gnss_status;
    bool has_SemiMajorAxisAccuracy;
    double SemiMajorAxisAccuracy;
    bool has_SemiMinorAxisAccuracy;
    double SemiMinorAxisAccuracy;
    bool has_SemiMajorAxisOrientation;
    double SemiMajorAxisOrientation;
    bool has_heading;
    double heading;
    bool has_velocity;
    double velocity;
    bool has_climb;
    double climb;
    bool has_lateral_acceleration;
    double lateral_acceleration;
    bool has_longitudinal_acceleration;
    double longitudinal_acceleration;
    bool has_vehicle_vertical_acceleration;
    double vehicle_vertical_acceleration;
    bool has_yaw_rate_degrees_per_second;
    double yaw_rate_degrees_per_second;
    bool has_yaw_rate_95pct_confidence;
    double yaw_rate_95pct_confidence;
    bool has_lane_position_number;
    double lane_position_number;
    bool has_lane_position_95pct_confidence;
    double lane_position_95pct_confidence;
    bool has_time_confidence;
    float time_confidence;
    bool has_heading_confidence;
    float heading_confidence;
    bool has_velocity_confidence;
    float velocity_confidence;
    bool has_elevation_confidence;
    float elevation_confidence;
    uint32_t leap_seconds;
/* @@protoc_insertion_point(struct:v2x_location_fix_t) */
} v2x_location_fix_t;

/* Default values for struct fields */

/* Initializer values for message structs */
#define v2x_gnss_fix_rates_supported_list_t_init_default {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
#define v2x_kinematics_capabilities_t_init_default {v2x_kinematics_capabilities_t_feature_flags_t_init_default, 0, v2x_gnss_fix_rates_supported_list_t_init_default}
#define v2x_kinematics_capabilities_t_feature_flags_t_init_default {0, 0, 0, 0, 0, 0, 0, 0, 0}
#define v2x_GNSSstatus_t_init_default            {0, 0, 0, 0, 0}
#define v2x_location_fix_t_init_default          {0, (v2x_fix_mode_t)0, 0, 0, 0, 0, 0, v2x_GNSSstatus_t_init_default, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, 0}
#define v2x_rates_t_init_default                 {0, 0}
#define v2x_init_t_init_default                  {0, ""}
#define v2x_msg_t_enable_fixes_tag_init_default  {0}
#define v2x_msg_t_disable_fixes_tag_init_default {0}
#define v2x_gnss_fix_rates_supported_list_t_init_zero {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
#define v2x_kinematics_capabilities_t_init_zero  {v2x_kinematics_capabilities_t_feature_flags_t_init_zero, 0, v2x_gnss_fix_rates_supported_list_t_init_zero}
#define v2x_kinematics_capabilities_t_feature_flags_t_init_zero {0, 0, 0, 0, 0, 0, 0, 0, 0}
#define v2x_GNSSstatus_t_init_zero               {0, 0, 0, 0, 0}
#define v2x_location_fix_t_init_zero             {0, (v2x_fix_mode_t)0, 0, 0, 0, 0, 0, v2x_GNSSstatus_t_init_zero, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, 0}
#define v2x_rates_t_init_zero                    {0, 0}
#define v2x_init_t_init_zero                     {0, ""}
#define v2x_msg_t_enable_fixes_tag_init_zero     {0}
#define v2x_msg_t_disable_fixes_tag_init_zero    {0}

/* Field tags (for use in manual encoding/decoding) */
#define v2x_GNSSstatus_t_unavailable_tag         1
#define v2x_GNSSstatus_t_aPDOPofUnder5_tag       2
#define v2x_GNSSstatus_t_inViewOfUnder5_tag      3
#define v2x_GNSSstatus_t_localCorrectionsPresent_tag 4
#define v2x_GNSSstatus_t_networkCorrectionsPresent_tag 5
#define v2x_gnss_fix_rates_supported_list_t_qty_rates_supported_tag 1
#define v2x_gnss_fix_rates_supported_list_t_rates_supported_hz_array_tag 2
#define v2x_init_t_log_level_mask_tag            1
#define v2x_init_t_server_ip_addr_tag            2
#define v2x_kinematics_capabilities_t_feature_flags_t_has_3_axis_gyro_tag 1
#define v2x_kinematics_capabilities_t_feature_flags_t_has_3_axis_accelerometer_tag 2
#define v2x_kinematics_capabilities_t_feature_flags_t_has_imu_supplemented_dead_reckoning_tag 3
#define v2x_kinematics_capabilities_t_feature_flags_t_has_yaw_rate_sensor_tag 4
#define v2x_kinematics_capabilities_t_feature_flags_t_used_vehicle_speed_tag 5
#define v2x_kinematics_capabilities_t_feature_flags_t_used_single_wheel_ticks_tag 6
#define v2x_kinematics_capabilities_t_feature_flags_t_used_front_differential_wheel_ticks_tag 7
#define v2x_kinematics_capabilities_t_feature_flags_t_used_rear_differential_wheel_ticks_tag 8
#define v2x_kinematics_capabilities_t_feature_flags_t_used_vehicle_dynamic_model_tag 9
#define v2x_rates_t_rate_report_hz_tag           1
#define v2x_rates_t_offset_nanoseconds_tag       2
#define v2x_kinematics_capabilities_t_feature_flags_tag 1
#define v2x_kinematics_capabilities_t_max_fix_rate_supported_hz_tag 2
#define v2x_kinematics_capabilities_t_rates_list_tag 3
#define v2x_location_fix_t_utc_fix_time_tag      1
#define v2x_location_fix_t_fix_mode_tag          2
#define v2x_location_fix_t_latitude_tag          3
#define v2x_location_fix_t_longitude_tag         4
#define v2x_location_fix_t_altitude_tag          5
#define v2x_location_fix_t_qty_SV_in_view_tag    6
#define v2x_location_fix_t_qty_SV_used_tag       7
#define v2x_location_fix_t_gnss_status_tag       8
#define v2x_location_fix_t_SemiMajorAxisAccuracy_tag 9
#define v2x_location_fix_t_SemiMinorAxisAccuracy_tag 10
#define v2x_location_fix_t_SemiMajorAxisOrientation_tag 11
#define v2x_location_fix_t_heading_tag           12
#define v2x_location_fix_t_velocity_tag          13
#define v2x_location_fix_t_climb_tag             14
#define v2x_location_fix_t_lateral_acceleration_tag 15
#define v2x_location_fix_t_longitudinal_acceleration_tag 16
#define v2x_location_fix_t_vehicle_vertical_acceleration_tag 17
#define v2x_location_fix_t_yaw_rate_degrees_per_second_tag 18
#define v2x_location_fix_t_yaw_rate_95pct_confidence_tag 19
#define v2x_location_fix_t_lane_position_number_tag 20
#define v2x_location_fix_t_lane_position_95pct_confidence_tag 21
#define v2x_location_fix_t_time_confidence_tag   22
#define v2x_location_fix_t_heading_confidence_tag 23
#define v2x_location_fix_t_velocity_confidence_tag 24
#define v2x_location_fix_t_elevation_confidence_tag 25
#define v2x_location_fix_t_leap_seconds_tag      26

/* Struct field encoding specification for nanopb */
extern const pb_field_t v2x_gnss_fix_rates_supported_list_t_fields[3];
extern const pb_field_t v2x_kinematics_capabilities_t_fields[4];
extern const pb_field_t v2x_kinematics_capabilities_t_feature_flags_t_fields[10];
extern const pb_field_t v2x_GNSSstatus_t_fields[6];
extern const pb_field_t v2x_location_fix_t_fields[27];
extern const pb_field_t v2x_rates_t_fields[3];
extern const pb_field_t v2x_init_t_fields[3];
extern const pb_field_t v2x_msg_t_enable_fixes_tag_fields[1];
extern const pb_field_t v2x_msg_t_disable_fixes_tag_fields[1];

/* Maximum encoded size of messages (where known) */
#define v2x_gnss_fix_rates_supported_list_t_size 198
#define v2x_kinematics_capabilities_t_size       227
#define v2x_kinematics_capabilities_t_feature_flags_t_size 18
#define v2x_GNSSstatus_t_size                    10
#define v2x_location_fix_t_size                  216
#define v2x_rates_t_size                         12
#define v2x_init_t_size                          40
#define v2x_msg_t_enable_fixes_tag_size          0
#define v2x_msg_t_disable_fixes_tag_size         0

/* Message IDs (where set with "msgid" option) */
#ifdef PB_MSGID

#define V2X_KINEMATICS_DATA_TYPES_MESSAGES \


#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
/* @@protoc_insertion_point(eof) */

#endif
