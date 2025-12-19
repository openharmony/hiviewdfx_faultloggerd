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
#ifndef DFX_FAULTLOG_CLIENT_H
#define DFX_FAULTLOG_CLIENT_H

#include <stdint.h>
#include <unistd.h>
#include "dfx_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief request limited process pipe file descriptor
 * @param pipeType type of request about pipe
 * @param pipeFd pipeFd to transfer message
 * @param timeout timeout ms
 * @param pid dump process pid
 * @return if succeed return 0, otherwise return -1
*/
AT_SYMBOL_HIDDEN int32_t RequestLimitedPipeFd(const int32_t pipeType, int* pipeFd, int pid, const char* procName);

/**
 * @brief request delete limited process file descriptor
 * @return if succeed return 0, otherwise return the error code
*/
AT_SYMBOL_HIDDEN int32_t RequestLimitedDelPipeFd(int pid);

/**
 * @brief request limited processdump
 * @return if succeed return 0, otherwise return the error code
*/
AT_SYMBOL_HIDDEN int32_t RequestLimitedProcessDump(int pid);

#ifdef __cplusplus
}
#endif
#endif
