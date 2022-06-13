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

#ifndef DFX_LOG_H
#define DFX_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifndef LOG_TAG
#define LOG_TAG "FaultLogger"
#endif

#ifndef LOG_DOMAIN
#define LOG_DOMAIN (0x2d01)
#endif

#define FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))

typedef enum Level {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    FATAL
} Level;

void Log(Level logLevel, unsigned int domain, const char *tag, const char *fmt, ...);

#define LOGV(fmt, ...) \
    Log(DEBUG, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGI(fmt, ...) \
    Log(INFO, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGW(fmt, ...) \
    Log(WARN, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGE(fmt, ...) \
    Log(ERROR, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define LOGF(fmt, ...) \
    Log(FATAL, LOG_DOMAIN, LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // DFX_LOG_H
