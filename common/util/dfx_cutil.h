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

#include <inttypes.h>
#include <securec.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ReadStringFromFile(char* path, char* dst, size_t dstSz);

bool GetThreadName(char* buffer, size_t bufferSz);

bool GetProcessName(char* buffer, size_t bufferSz);

uint64_t GetTimeMillseconds(void);

#ifdef __cplusplus
}
#endif
#endif
