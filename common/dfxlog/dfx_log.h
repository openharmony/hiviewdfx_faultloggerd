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

bool CheckDebugLevel(void);
void InitDebugFd(int fd);
void SetLogLevel(const LogLevel logLevel);
LogLevel GetLogLevel(void);

#if defined(DFX_LOG_HILOG_BASE) || defined(is_ohos_lite)
int DfxLogPrint(const LogLevel logLevel, const unsigned int domain, const char* tag, \
                const char *fmt, ...) __attribute__((format(printf, 4, 5)));
int DfxLogPrintV(const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, va_list ap);

#define DFXLOG_PRINT(prio, ...) DfxLogPrint(prio, LOG_DOMAIN, LOG_TAG, ##__VA_ARGS__)

#define LOGD(fmt, ...) DFXLOG_PRINT(LOG_DEBUG, "[%s:%d] " fmt, (FILENAME_), (__LINE__), ##__VA_ARGS__)
#define LOGI(fmt, ...) DFXLOG_PRINT(LOG_INFO, "[%s:%d] " fmt, (FILENAME_), (__LINE__), ##__VA_ARGS__)
#define LOGW(fmt, ...) DFXLOG_PRINT(LOG_WARN, "[%s:%d] " fmt, (FILENAME_), (__LINE__), ##__VA_ARGS__)
#define LOGE(fmt, ...) DFXLOG_PRINT(LOG_ERROR, "[%s:%d] " fmt, (FILENAME_), (__LINE__), ##__VA_ARGS__)
#define LOGF(fmt, ...) DFXLOG_PRINT(LOG_FATAL, "[%s:%d] " fmt, (FILENAME_), (__LINE__), ##__VA_ARGS__)

#ifdef DFX_LOG_UNWIND
#define LOGU(fmt, ...) DFXLOG_PRINT(LOG_INFO, "[%s:%d] " fmt, (FILENAME_), (__LINE__), ##__VA_ARGS__)
#else
#define LOGU(fmt, ...)
#endif

#define DFXLOG_DEBUG(fmt, ...) DFXLOG_PRINT(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define DFXLOG_INFO(fmt, ...) DFXLOG_PRINT(LOG_INFO, fmt, ##__VA_ARGS__)
#define DFXLOG_WARN(fmt, ...) DFXLOG_PRINT(LOG_WARN, fmt, ##__VA_ARGS__)
#define DFXLOG_ERROR(fmt, ...) DFXLOG_PRINT(LOG_ERROR, fmt, ##__VA_ARGS__)
#define DFXLOG_FATAL(fmt, ...) DFXLOG_PRINT(LOG_FATAL, fmt, ##__VA_ARGS__)
#else

#ifdef HILOG_FMTID
#define DFXLOG_STD_ARRAY(level, fmt, ...) \
do { \
    constexpr HILOG_FMT_IN_SECTION static auto hilogFmt = fmt ## _DfxToPublic; \
    FmtId fmtid { HILOG_UUID, HILOG_FMT_OFFSET(hilogFmt.data()) }; \
    HiLogPrintDict(LOG_CORE, level, LOG_DOMAIN, LOG_TAG, &fmtid, hilogFmt.data(), ##__VA_ARGS__); \
} while (0)
#define DFXLOG_STD_ARRAY_FILE(level, fmt, ...) \
do { \
    constexpr HILOG_FMT_IN_SECTION static auto hilogFmt = OHOS::HiviewDFX::ConcatStr("[%{public}s:%{public}d] ", \
        fmt ## _DfxToPublic); \
    FmtId fmtid { HILOG_UUID, HILOG_FMT_OFFSET(hilogFmt.data()) }; \
    HiLogPrintDict(LOG_CORE, level, LOG_DOMAIN, LOG_TAG, \
        &fmtid, hilogFmt.data(), (FILENAME_), (__LINE__), ##__VA_ARGS__); \
} while (0)
#else
#define DFXLOG_STD_ARRAY(level, fmt, ...) \
do { \
    HiLogPrint(LOG_CORE, level, LOG_DOMAIN, LOG_TAG, fmt ## _DfxToPublic.data(), ##__VA_ARGS__); \
} while (0)
#define DFXLOG_STD_ARRAY_FILE(level, fmt, ...) \
do { \
    constexpr auto hilogFmt = OHOS::HiviewDFX::ConcatStr("[%{public}s:%{public}d] ", fmt ## _DfxToPublic); \
    HiLogPrint(LOG_CORE, level, LOG_DOMAIN, LOG_TAG, hilogFmt.data(), (FILENAME_), (__LINE__), ##__VA_ARGS__); \
} while (0)
#endif

#define LOGD(fmt, ...) DFXLOG_STD_ARRAY_FILE(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) DFXLOG_STD_ARRAY_FILE(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) DFXLOG_STD_ARRAY_FILE(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) DFXLOG_STD_ARRAY_FILE(LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOGF(fmt, ...) DFXLOG_STD_ARRAY_FILE(LOG_FATAL, fmt, ##__VA_ARGS__)

#ifdef DFX_LOG_UNWIND
#define LOGU(fmt, ...) DFXLOG_STD_ARRAY_FILE(LOG_INFO, fmt, ##__VA_ARGS__)
#else
#define LOGU(fmt, ...)
#endif

#define DFXLOG_DEBUG(fmt, ...) DFXLOG_STD_ARRAY(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define DFXLOG_INFO(fmt, ...) DFXLOG_STD_ARRAY(LOG_INFO, fmt, ##__VA_ARGS__)
#define DFXLOG_WARN(fmt, ...) DFXLOG_STD_ARRAY(LOG_WARN, fmt, ##__VA_ARGS__)
#define DFXLOG_ERROR(fmt, ...) DFXLOG_STD_ARRAY(LOG_ERROR, fmt, ##__VA_ARGS__)
#define DFXLOG_FATAL(fmt, ...) DFXLOG_STD_ARRAY(LOG_FATAL, fmt, ##__VA_ARGS__)
#endif

#else
#define DFXLOG_PRINT(prio, ...)

#define DFXLOG_DEBUG(fmt, ...)
#define DFXLOG_INFO(fmt, ...)
#define DFXLOG_WARN(fmt, ...)
#define DFXLOG_ERROR(fmt, ...)
#define DFXLOG_FATAL(fmt, ...)

#define LOGD(fmt, ...)
#define LOGI(fmt, ...)
#define LOGW(fmt, ...)
#define LOGE(fmt, ...)
#define LOGF(fmt, ...)
#define LOGU(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif
#endif
