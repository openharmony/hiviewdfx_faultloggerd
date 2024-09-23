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
#include <hilog_base/log_base.h>
#else
#include <hilog/log.h>
#include "dfx_log_public.h"
#endif
#include "dfx_log_define.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DFX_NO_PRINT_LOG

#ifdef DFX_LOG_HILOG_BASE
// replace the old interface, and delete the old interface after the replacement is complete
#define LOGDEBUG(fmt, ...) \
    HILOG_BASE_DEBUG(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGINFO(fmt, ...) \
    HILOG_BASE_INFO(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGWARN(fmt, ...) \
    HILOG_BASE_WARN(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGERROR(fmt, ...) \
    HILOG_BASE_ERROR(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGFATAL(fmt, ...) \
    HILOG_BASE_FATAL(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)

#ifdef DFX_LOG_UNWIND
#define LOGUNWIND(fmt, ...) \
    HILOG_BASE_INFO(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#else
#define LOGUNWIND(fmt, ...)
#endif

#else
// replace the old interface, and delete the old interface after the replacement is complete
#define LOGDEBUG(fmt, ...) \
    HILOG_DEBUG(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGINFO(fmt, ...) \
    HILOG_INFO(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGWARN(fmt, ...) \
    HILOG_WARN(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGERROR(fmt, ...) \
    HILOG_ERROR(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#define LOGFATAL(fmt, ...) \
    HILOG_FATAL(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)

#ifdef DFX_LOG_UNWIND
#define LOGUNWIND(fmt, ...) \
    HILOG_INFO(LOG_CORE, "[%{public}s:%{public}d] " fmt, (__FILE_NAME__), (__LINE__), ##__VA_ARGS__)
#else
#define LOGUNWIND(fmt, ...)
#endif

#endif

#else

#define LOGDEBUG(fmt, ...)
#define LOGINFO(fmt, ...)
#define LOGWARN(fmt, ...)
#define LOGERROR(fmt, ...)
#define LOGFATAL(fmt, ...)
#define LOGUNWIND(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif
#endif
