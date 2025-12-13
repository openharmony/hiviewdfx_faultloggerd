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

/**
 * NOTICE: The DfxCollectAsyncStack function filters requests using types from AsyncType.
 *
 * To enable stack backtrace, you need to set your own type as the target type using the DfxSetAsyncStackType function.
*/

/**
 * @brief init async stack
 *
 * @return if succeed return true, otherwise return false
*/
bool DfxInitAsyncStack();

/**
 * @brief get stack id of submitter
 *
 * @return stack id, if async stack not init, return 0
*/
uint64_t DfxGetSubmitterStackId(void);

/**
 * @brief get submitter stack in local case
 *
 * @param stackTrace  stackTrace of submitter
 * @param bufferSize  buffer size of stackTrace
 * @return if succeed return 0, otherwise return -1
*/
int DfxGetSubmitterStackLocal(char* stackTrace, size_t bufferSize);

typedef enum {
    ASYNC_TYPE_LIBUV_TIMER      = 1ULL << 0,
    ASYNC_TYPE_LIBUV_QUEUE      = 1ULL << 1,
    ASYNC_TYPE_LIBUV_SEND       = 1ULL << 2,
    ASYNC_TYPE_FFRT_POOL        = 1ULL << 8,
    ASYNC_TYPE_FFRT_QUEUE       = 1ULL << 9,
    ASYNC_TYPE_EVENTHANDLER     = 1ULL << 16,
    ASYNC_TYPE_PROMISE          = 1ULL << 17,
    ASYNC_TYPE_CUSTOMIZE        = 1ULL << 32
} AsyncType;

/**
 * @brief set the currently enabled asyn type and return the old asynchronous type value
 *
 * @param asyncType  new async type (specific type definition see AsyncType)
 * @return return the async type value before setting
*/
uint64_t DfxSetAsyncStackType(uint64_t asyncType);

/**
 * @brief collects async stack trace based on the specified type.
 *
 * @param type  the async type mask to filter which async stacks to collect.
 * @return stack id, if async stack not init or async type not allowed, return 0
*/
uint64_t DfxCollectAsyncStack(uint64_t type);

/**
 * @brief collect current stack with specified type and maximum depth
 *
 * @param type the async type mask to filter which async stacks to collect.
 * @param depth the maximum depth of the stack trace to collect.
 * @return stack id, stack id, if async stack not init or async type not allowed, return 0
*/
uint64_t DfxCollectStackWithDepth(uint64_t type, size_t depth);

/**
 * @brief set stack id to current running context(tls)
 *
 * @param stackId the stack ID to be associated with the current thread.
 * @return void
*/
void DfxSetSubmitterStackId(uint64_t stackId);

/**
 * @brief init async stack with buffer from profiler
 * @param buffer external buffer pointer, used to store stack pcs.
 * @param size buffer size.
 * @return if succeed return true, otherwise return false
*/
bool DfxInitProfilerAsyncStack(void* buffer, size_t size);
#ifdef __cplusplus
}
#endif
#endif