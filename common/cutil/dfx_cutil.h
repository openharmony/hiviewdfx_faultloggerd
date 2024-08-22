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
#ifndef DFX_COMMON_CUTIL_H
#define DFX_COMMON_CUTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include "dfx_define.h"

#ifdef __cplusplus
extern "C" {
#endif

AT_SYMBOL_HIDDEN pid_t GetRealPid(void);

AT_SYMBOL_HIDDEN bool GetThreadName(char* buffer, size_t bufferSz);

AT_SYMBOL_HIDDEN bool GetThreadNameByTid(int32_t tid, char* buffer, size_t bufferSz);

AT_SYMBOL_HIDDEN bool GetProcessName(char* buffer, size_t bufferSz);

AT_SYMBOL_HIDDEN uint64_t GetTimeMilliseconds(void);

AT_SYMBOL_HIDDEN bool TrimAndDupStr(const char* src, char* dst);

#ifdef __cplusplus
}
#endif
#endif
