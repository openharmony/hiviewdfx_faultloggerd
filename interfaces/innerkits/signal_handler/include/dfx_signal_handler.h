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
#ifndef DFX_SIGNAL_HANDLER_H
#define DFX_SIGNAL_HANDLER_H

#include <inttypes.h>
#include <signal.h>
#include <ucontext.h>
#include "dfx_define.h"

#ifdef __cplusplus
extern "C" {
#endif
struct ProcessDumpRequest {
    int32_t type;
    int32_t tid;
    int32_t pid;
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

void DFX_InstallSignalHandler(void);

#ifdef __cplusplus
}
#endif
#endif
