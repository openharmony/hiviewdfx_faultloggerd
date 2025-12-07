/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef DFX_SAFE_READER_H
#define DFX_SAFE_READER_H

#include <stdint.h>
#include <unistd.h>
#include "dfx_define.h"

#ifdef __cplusplus
extern "C" {
#endif

AT_SYMBOL_HIDDEN size_t CopyReadableBufSafe(uintptr_t destPtr, size_t destLen, uintptr_t srcPtr, size_t srcLen);
AT_SYMBOL_HIDDEN void DeinitPipe();

#ifdef __cplusplus
}
#endif
#endif
