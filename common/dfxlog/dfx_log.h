/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#ifndef DFX_LOG_H
#define DFX_LOG_H

#ifndef DFX_NO_PRINT_LOG
#ifdef DFX_LOG_HILOG_BASE
#include <log_base.h>
#else
#include <hilog/log.h>
#endif
#include "dfx_log_define.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DFX_NO_PRINT_LOG

#ifdef DFX_LOG_HILOG_BASE
#ifdef DFX_LOG_UNWIND
#define DFXLOGD(fmt, ...) HILOG_BASE_DEBUG(LOG_CORE, fmt, ##__VA_ARGS__)
#else
#define DFXLOGD(fmt, ...)
#endif
#define DFXLOGI(fmt, ...) HILOG_BASE_INFO(LOG_CORE, fmt, ##__VA_ARGS__)
#define DFXLOGW(fmt, ...) HILOG_BASE_WARN(LOG_CORE, fmt, ##__VA_ARGS__)
#define DFXLOGE(fmt, ...) HILOG_BASE_ERROR(LOG_CORE, fmt, ##__VA_ARGS__)
#define DFXLOGF(fmt, ...) \
    HILOG_BASE_FATAL(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)

#ifdef DFX_LOG_UNWIND
#ifdef ALLOW_TO_STDERR
#define DFXLOGU(fmt, ...) \
    HILOG_BASE_INFO(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__); \
    fprintf(stderr, "%s\n", fmt)
#else
#define DFXLOGU(fmt, ...) \
    HILOG_BASE_INFO(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#endif
#else
#define DFXLOGU(fmt, ...)
#endif

#else

#define DFXLOGD(fmt, ...) HILOG_DEBUG(LOG_CORE, fmt, ##__VA_ARGS__)
#define DFXLOGI(fmt, ...) HILOG_INFO(LOG_CORE, fmt, ##__VA_ARGS__)
#define DFXLOGW(fmt, ...) HILOG_WARN(LOG_CORE, fmt, ##__VA_ARGS__)
#define DFXLOGE(fmt, ...) HILOG_ERROR(LOG_CORE, fmt, ##__VA_ARGS__)
#define DFXLOGF(fmt, ...) \
    HILOG_FATAL(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)

#ifdef DFX_LOG_UNWIND
#define DFXLOGU(fmt, ...) \
    HILOG_INFO(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#else
#define DFXLOGU(fmt, ...)
#endif

#endif

#else

#define DFXLOGD(fmt, ...)
#define DFXLOGI(fmt, ...)
#define DFXLOGW(fmt, ...)
#define DFXLOGE(fmt, ...)
#define DFXLOGF(fmt, ...)
#define DFXLOGU(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif
#endif
