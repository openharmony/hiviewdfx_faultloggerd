/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DFX_NO_PRINT_LOG
#ifdef DFX_LOG_HILOG_BASE
#include <hilog_base/log_base.h>
#else
#include <hilog/log.h>
#endif
#include "dfx_log_define.h"

bool CheckDebugLevel(void);
void InitDebugFd(int fd);
void SetLogLevel(const LogLevel logLevel);
LogLevel GetLogLevel(void);

int DfxLogPrint(const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, ...);
int DfxLogPrintV(const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, va_list ap);

#define DFXLOG_PRINT(prio, domain, tag, ...) DfxLogPrint(prio, domain, tag, ##__VA_ARGS__)
#define DFXLOG_PRINTV(prio, domain, tag, fmt, args) DfxLogPrintV(prio, domain, tag, fmt, args)

#define DFXLOG_DEBUG(...) DFXLOG_PRINT(LOG_DEBUG, LOG_DOMAIN, LOG_TAG, ##__VA_ARGS__)
#define DFXLOG_INFO(...) DFXLOG_PRINT(LOG_INFO, LOG_DOMAIN, LOG_TAG, ##__VA_ARGS__)
#define DFXLOG_WARN(...) DFXLOG_PRINT(LOG_WARN, LOG_DOMAIN, LOG_TAG, ##__VA_ARGS__)
#define DFXLOG_ERROR(...) DFXLOG_PRINT(LOG_ERROR, LOG_DOMAIN, LOG_TAG, ##__VA_ARGS__)
#define DFXLOG_FATAL(...) DFXLOG_PRINT(LOG_FATAL, LOG_DOMAIN, LOG_TAG, ##__VA_ARGS__)

#define LOGD(...) DFXLOG_PRINT(LOG_DEBUG, LOG_DOMAIN, LOG_TAG, "[%s:%d]" , (__FILENAME__), (__LINE__), ##__VA_ARGS__)
#define LOGI(...) DFXLOG_PRINT(LOG_INFO, LOG_DOMAIN, LOG_TAG, "[%s:%d]" , (__FILENAME__), (__LINE__), ##__VA_ARGS__)
#define LOGW(...) DFXLOG_PRINT(LOG_WARN, LOG_DOMAIN, LOG_TAG, "[%s:%d]" , (__FILENAME__), (__LINE__), ##__VA_ARGS__)
#define LOGE(...) DFXLOG_PRINT(LOG_ERROR, LOG_DOMAIN, LOG_TAG, "[%s:%d]" , (__FILENAME__), (__LINE__), ##__VA_ARGS__)
#define LOGF(...) DFXLOG_PRINT(LOG_FATAL, LOG_DOMAIN, LOG_TAG, "[%s:%d]" , (__FILENAME__), (__LINE__), ##__VA_ARGS__)

#ifndef LOG_CHECK_MSG
#define LOG_CHECK_MSG(condition, ...) \
    if (!(condition)) { \
        DFXLOG_PRINT(LOG_FATAL, LOG_DOMAIN, LOG_TAG, " check failed: %s ", #condition, ##__VA_ARGS__); \
    }
#endif

#ifndef LOG_CHECK
#define LOG_CHECK(condition) LOG_CHECK_MSG(condition, "")
#endif

#ifndef LOG_CHECK_ABORT
#define LOG_CHECK_ABORT(condition) \
    if (!(condition)) { \
        LOGF(" check abort: %s", #condition); \
        abort(); \
    }
#endif

#else
#define DFXLOG_PRINT(prio, domain, tag, ...)
#define DFXLOG_PRINTV(prio, domain, tag, fmt, args)

#define DFXLOG_DEBUG(...)
#define DFXLOG_INFO(...)
#define DFXLOG_WARN(...)
#define DFXLOG_ERROR(...)
#define DFXLOG_FATAL(...)

#define LOGD(...)
#define LOGI(...)
#define LOGW(...)
#define LOGE(...)
#define LOGF(...)

#define LOG_CHECK_MSG(condition, ...)
#define LOG_CHECK(condition)
#define LOG_CHECK_ABORT(condition)
#endif

#ifdef DFX_LOG_UNWIND
#define LOGU(...) DFXLOG_PRINT(LOG_FATAL, LOG_DOMAIN, LOG_TAG, "[%s:%d]" , (__FILENAME__), (__LINE__), ##__VA_ARGS__)
#else
#define LOGU(...)
#endif

#ifdef __cplusplus
}
#endif
#endif
