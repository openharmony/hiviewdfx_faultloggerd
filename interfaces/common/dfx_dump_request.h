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
#ifndef DFX_DUMP_REQUEST_H
#define DFX_DUMP_REQUEST_H

#include <inttypes.h>
#include <signal.h>
#include <ucontext.h>
#include "dfx_define.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ProcessDumpType : int32_t {
    DUMP_TYPE_PROCESS,
    DUMP_TYPE_THREAD,
};

// keep sync with the definition in hitracec.h
typedef struct TraceInfo {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint64_t valid : 1;
    uint64_t ver : 3;
    uint64_t chainId : 60;

    uint64_t flags : 12;
    uint64_t spanId : 26;
    uint64_t parentSpanId : 26;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint64_t chainId : 60;
    uint64_t ver : 3;
    uint64_t valid : 1;

    uint64_t parentSpanId : 26;
    uint64_t spanId : 26;
    uint64_t flags : 12;
#else
#error "ERROR: No BIG_LITTLE_ENDIAN defines."
#endif
} TraceInfo;

struct ProcessDumpRequest {
    enum ProcessDumpType type;
    int32_t tid;
    int32_t recycleTid;
    int32_t pid;
    int32_t nsPid;
    int32_t vmPid;
    int32_t vmNsPid;
    uint32_t uid;
    uint64_t reserved;
    uint64_t timeStamp;
    siginfo_t siginfo;
    ucontext_t context;
    char threadName[NAME_LEN];
    char processName[NAME_LEN];
    char lastFatalMessage[MAX_FATAL_MSG_SIZE];
    TraceInfo traceInfo;
};
#ifdef __cplusplus
}
#endif
#endif
