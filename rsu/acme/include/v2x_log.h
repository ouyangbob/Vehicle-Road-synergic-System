/*
 *  Copyright (c) 2017 Qualcomm Technologies, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef V2X_LOG_H
#define V2X_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "v2x_common.pb.h"

#define LOGI(fmt, args...) \
    v2x_log(LOG_INFO, "[I][%s:%d] " fmt, __func__, __LINE__, ## args)

#define LOGW(fmt, args...) \
    v2x_log(LOG_WARNING, "[W][%s:%d] " fmt, __func__, __LINE__, ## args)

#define LOGD(fmt, args...) \
    v2x_log(LOG_DEBUG, "[D][%s:%d] " fmt, __func__, __LINE__, ## args)

#define LOGE(fmt, args...) \
    v2x_log(LOG_ERR, "[E][%s:%d] " fmt, __func__, __LINE__, ## args)

#ifdef __cplusplus
extern "C" {
#endif

extern int v2x_use_syslog;
extern int v2x_log_level;

static inline const char *v2x_log_prio_name(int level)
{
#ifdef SYSLOG_NAMES
    int n;

    for (n = 0; n <= LOG_WARNING; n++) {
        if (prioritynames[n].c_val == level) {
            break;
        }
    }
    return (prioritynames[n].c_name);
#else
    switch (level) {
    case LOG_ERR:     return ("error");
    case LOG_NOTICE:  return ("notice");
    case LOG_WARNING:  return ("warning");
    case LOG_INFO:    return ("info");
    case LOG_DEBUG:   return ("debug");
    default:
        return ("other");
    }
#endif
}

static inline void v2x_log(int level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (level <= v2x_log_level) {
        if (!v2x_use_syslog) {
            vprintf(fmt, args);
        } else {
            vsyslog(level, fmt, args);
        }
    }
    va_end(args);
}

/* Call with the new log filter level */
extern void v2x_log_level_set(int new_level);
extern void v2x_log_to_syslog(int boolean_true_or_false);

#ifdef __cplusplus
}
#endif

#endif
