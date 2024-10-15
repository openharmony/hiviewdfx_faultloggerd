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
#include "stdbool.h"

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

// replace the old interface, and delete the old interface after the replacement is complete
#define DFXMUSL_PRINT(prio, ...) HiLogAdapterPrint(LOG_CORE, prio, LOG_DOMAIN, LOG_TAG, __VA_ARGS__)

#define DFXLOGD(...) DFXMUSL_PRINT(LOG_DEBUG, __VA_ARGS__)
#define DFXLOGI(...) DFXMUSL_PRINT(LOG_INFO, __VA_ARGS__)
#define DFXLOGW(...) DFXMUSL_PRINT(LOG_WARN, __VA_ARGS__)
#define DFXLOGE(...) DFXMUSL_PRINT(LOG_ERROR, __VA_ARGS__)
#define DFXLOGF(...) DFXMUSL_PRINT(LOG_FATAL, __VA_ARGS__)

#else

// replace the old interface, and delete the old interface after the replacement is complete
#define DFXLOGD(...)
#define DFXLOGI(...)
#define DFXLOGW(...)
#define DFXLOGE(...)
#define DFXLOGF(...)

#endif

#ifdef __cplusplus
}
#endif
#endif