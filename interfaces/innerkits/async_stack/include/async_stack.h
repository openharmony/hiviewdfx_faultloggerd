/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef DFX_ASYNC_STACK_H
#define DFX_ASYNC_STACK_H

#include <inttypes.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// return stackId
uint64_t CollectAsyncStack(void);

// set stack id to TLS
void SetStackId(uint64_t stackId);

// get stack id from TLS
uint64_t GetStackId(void);

/**
 * @brief get submitter stack in local case
 *
 * @param stackTrace  stackTrace of submitter
 * @param bufferSize  buffer size of stackTrace
 * @return if succeed return 0, otherwise return -1
*/
int DfxGetSubmitterStackLocal(char* stackTrace, size_t bufferSize);

#ifdef __cplusplus
}
#endif
#endif