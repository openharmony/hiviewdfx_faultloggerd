/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifdef DFX_LOG_USE_HILOG_BASE
#include <hilog_base/log_base.h>
#else
#include <hilog/log.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DFX_NO_PRINT_LOG
#define DfxLogDebug(fmt, ...)
#define DfxLogInfo(fmt, ...)
#define DfxLogWarn(fmt, ...)
#define DfxLogError(fmt, ...)
#define DfxLogFatal(fmt, ...)

#define LOGD(fmt, ...)
#define LOGI(fmt, ...)
#define LOGW(fmt, ...)
#define LOGE(fmt, ...)
#define LOGF(fmt, ...)

#else

#define FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))

#ifndef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifndef LOG_TAG
#define LOG_TAG "DfxFaultLogger"
#endif

typedef enum Level {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    FATAL
} Level;

bool CheckDebugLevel(void);
int DfxLog(const Level logLevel, const unsigned int domain, const char* tag, const char *fmt, ...);

#define DfxLogDebug(fmt, ...)   DfxLog(DEBUG, LOG_DOMAIN, LOG_TAG, fmt, ##__VA_ARGS__)
#define DfxLogInfo(fmt, ...)    DfxLog(INFO, LOG_DOMAIN, LOG_TAG, fmt, ##__VA_ARGS__)
#define DfxLogWarn(fmt, ...)    DfxLog(WARN, LOG_DOMAIN, LOG_TAG, fmt, ##__VA_ARGS__)
#define DfxLogError(fmt, ...)   DfxLog(ERROR, LOG_DOMAIN, LOG_TAG, fmt, ##__VA_ARGS__)
#define DfxLogFatal(fmt, ...)   DfxLog(FATAL, LOG_DOMAIN, LOG_TAG, fmt, ##__VA_ARGS__)

#define LOGD(fmt, ...) DfxLog(DEBUG, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGI(fmt, ...) DfxLog(INFO, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGW(fmt, ...) DfxLog(WARN, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGE(fmt, ...) DfxLog(ERROR, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGF(fmt, ...) DfxLog(FATAL, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)

#endif
#ifdef __cplusplus
}
#endif
#endif
