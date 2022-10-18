/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifdef __cplusplus
extern "C" {
#endif

// Log type
typedef enum {
    LOG_TYPE_MIN = 0,
    LOG_APP = 0,
    // Log to kmsg, only used by init phase.
    LOG_INIT = 1,
    // Used by core service, framework.
    LOG_CORE = 3,
    LOG_KMSG = 4,
    LOG_TYPE_MAX
} LogType;

// Log level
typedef enum {
    LOG_LEVEL_MIN = 0,
    LOG_DEBUG = 3,
    LOG_INFO = 4,
    LOG_WARN = 5,
    LOG_ERROR = 6,
    LOG_FATAL = 7,
    LOG_LEVEL_MAX,
} LogLevel;

#ifndef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifndef LOG_TAG
#define LOG_TAG "DfxFaultLogger"
#endif

#ifdef ENABLE_MUSL_LOG
extern int HiLogAdapterPrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...);
extern int vsnprintfp_s(char *strDest, size_t destMax, size_t count, int priv, const char *format, va_list arglist);

int DfxLog(const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, ...)
{
    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, fmt);
    ret = vsnprintfp_s(buf, sizeof(buf), sizeof(buf) - 1, false, fmt, args);
    va_end(args);
    HiLogAdapterPrint(LOG_CORE, logLevel, domain, tag, "%{public}s", buf);
    return ret;
}

#define DfxLogDebug(...) DfxLog(LOG_DEBUG, LOG_DOMAIN, LOG_TAG, __VA_ARGS__)
#define DfxLogInfo(...) DfxLog(LOG_INFO, LOG_DOMAIN, LOG_TAG, __VA_ARGS__)
#define DfxLogWarn(...) DfxLog(LOG_WARN, LOG_DOMAIN, LOG_TAG, __VA_ARGS__)
#define DfxLogError(...) DfxLog(LOG_ERROR, LOG_DOMAIN, LOG_TAG, __VA_ARGS__)
#define DfxLogFatal(...) DfxLog(LOG_FATAL, LOG_DOMAIN, LOG_TAG, __VA_ARGS__)
#else
#define DfxLogDebug(...)
#define DfxLogInfo(...)
#define DfxLogWarn(...)
#define DfxLogError(...)
#define DfxLogFatal(...)
#endif

#ifdef __cplusplus
}
#endif
#endif