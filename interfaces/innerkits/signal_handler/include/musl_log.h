/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef DFX_MUSL_LOG_H
#define DFX_MUSL_LOG_H

#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>

#include "dfx_log_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_SIGHAND_MUSL_LOG

/* Log type */
typedef enum {
    /* min log type */
    LOG_TYPE_MIN = 0,
    /* Used by app log. */
    LOG_APP = 0,
    /* Log to kmsg, only used by init phase. */
    LOG_INIT = 1,
    /* Used by core service, framework. */
    LOG_CORE = 3,
    /* Used by kmsg log. */
    LOG_KMSG = 4,
    /* max log type */
    LOG_TYPE_MAX
} LogType;

/* Log level */
typedef enum {
    /* min log level */
    LOG_LEVEL_MIN = 0,
    /* Designates lower priority log. */
    LOG_DEBUG = 3,
    /* Designates useful information. */
    LOG_INFO = 4,
    /* Designates hazardous situations. */
    LOG_WARN = 5,
    /* Designates very serious errors. */
    LOG_ERROR = 6,
    /* Designates major fatal anomaly. */
    LOG_FATAL = 7,
    /* max log level */
    LOG_LEVEL_MAX,
} LogLevel;

extern int HiLogAdapterPrintArgs(
    const LogType type, const LogLevel level, const unsigned int domain, const char *tag, const char *fmt, va_list ap);
extern int vsnprintfp_s(char *strDest, size_t destMax, size_t count, int priv, const char *format, va_list arglist);

__attribute__ ((visibility("hidden"))) int MuslHiLogPrinter(
    LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...);

__attribute__ ((visibility("hidden"))) int DfxLogPrint(
    const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, ...);

#define DFXLOG_PRINT(prio, ...) DfxLogPrint(prio, LOG_DOMAIN, LOG_TAG, ##__VA_ARGS__)

#define DFXLOG_DEBUG(...) DFXLOG_PRINT(LOG_DEBUG, ##__VA_ARGS__)
#define DFXLOG_INFO(...) DFXLOG_PRINT(LOG_INFO, ##__VA_ARGS__)
#define DFXLOG_WARN(...) DFXLOG_PRINT(LOG_WARN, ##__VA_ARGS__)
#define DFXLOG_ERROR(...) DFXLOG_PRINT(LOG_ERROR, ##__VA_ARGS__)
#define DFXLOG_FATAL(...) DFXLOG_PRINT(LOG_FATAL, ##__VA_ARGS__)

#else

#define DFXLOG_PRINT(prio, ...)

#define DFXLOG_DEBUG(...)
#define DFXLOG_INFO(...)
#define DFXLOG_WARN(...)
#define DFXLOG_ERROR(...)
#define DFXLOG_FATAL(...)
#endif

#ifdef __cplusplus
}
#endif
#endif