/*
 *  Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef PB_V2X_MSG_TYPE_PB_H_INCLUDED
#define PB_V2X_MSG_TYPE_PB_H_INCLUDED
#include <pb.h>

#include "v2x_common.pb.h"

#include "v2x_kinematics_data_types.pb.h"

/* @@protoc_insertion_point(includes) */
#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Struct definitions */
typedef struct _v2x_rate_notification_t {
    v2x_status_enum_type status;
    v2x_rates_t rate;
/* @@protoc_insertion_point(struct:v2x_rate_notification_t) */
} v2x_rate_notification_t;

typedef struct _v2x_set_rate_msg_t {
    v2x_rates_t new_rate;
/* @@protoc_insertion_point(struct:v2x_set_rate_msg_t) */
} v2x_set_rate_msg_t;

typedef struct _v2x_msg_t {
    pb_size_t which_payload;
    union {
        v2x_location_fix_t fix;
        v2x_set_rate_msg_t set_rate;
        v2x_rate_notification_t rate_notification;
        v2x_kinematics_capabilities_t capabilities;
        v2x_msg_t_enable_fixes_tag enable_fixes;
        v2x_msg_t_disable_fixes_tag disable_fixes;
    } payload;
/* @@protoc_insertion_point(struct:v2x_msg_t) */
} v2x_msg_t;

/* Default values for struct fields */

/* Initializer values for message structs */
#define v2x_set_rate_msg_t_init_default          {v2x_rates_t_init_default}
#define v2x_rate_notification_t_init_default     {(v2x_status_enum_type)0, v2x_rates_t_init_default}
#define v2x_msg_t_init_default                   {0, {v2x_location_fix_t_init_default}}
#define v2x_set_rate_msg_t_init_zero             {v2x_rates_t_init_zero}
#define v2x_rate_notification_t_init_zero        {(v2x_status_enum_type)0, v2x_rates_t_init_zero}
#define v2x_msg_t_init_zero                      {0, {v2x_location_fix_t_init_zero}}

/* Field tags (for use in manual encoding/decoding) */
#define v2x_rate_notification_t_status_tag       1
#define v2x_rate_notification_t_rate_tag         2
#define v2x_set_rate_msg_t_new_rate_tag          1
#define v2x_msg_t_fix_tag                        1
#define v2x_msg_t_set_rate_tag                   2
#define v2x_msg_t_rate_notification_tag          3
#define v2x_msg_t_capabilities_tag               4
#define v2x_msg_t_enable_fixes_tag               5
#define v2x_msg_t_disable_fixes_tag              6

/* Struct field encoding specification for nanopb */
extern const pb_field_t v2x_set_rate_msg_t_fields[2];
extern const pb_field_t v2x_rate_notification_t_fields[3];
extern const pb_field_t v2x_msg_t_fields[7];

/* Maximum encoded size of messages (where known) */
#define v2x_set_rate_msg_t_size                  14
#define v2x_rate_notification_t_size             17
#define v2x_msg_t_size                           230

/* Message IDs (where set with "msgid" option) */
#ifdef PB_MSGID

#define V2X_MSG_TYPE_MESSAGES \


#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
/* @@protoc_insertion_point(eof) */

#endif
