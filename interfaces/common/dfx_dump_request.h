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

/**
 * @brief ProcessDump type
 */
enum ProcessDumpType : int32_t {
    DUMP_TYPE_CPP_CRASH,
    DUMP_TYPE_DUMP_CATCH,
    DUMP_TYPE_MEM_LEAK,
    DUMP_TYPE_FDSAN,
    DUMP_TYPE_JEMALLOC,
    DUMP_TYPE_BADFD,
    DUMP_TYPE_COREDUMP,
    DUMP_TYPE_PIPE,
};

/**
 * @brief Process trace information
 */
enum OperateResult : int32_t {
    OPE_FAIL = 0,
    OPE_SUCCESS = 1,
    OPE_CONTINUE = 2,
};

typedef enum {
    MESSAGE_NONE = 0,
    MESSAGE_FATAL, // Need to write LastFatalMessage
    MESSAGE_CALLBACK, // call back memssage
} MessageType;

typedef struct {
    MessageType type;
    uintptr_t addr;
} Message;

#ifndef is_ohos_lite
typedef struct DumpHiTraceIdStruct {
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
} DumpHiTraceIdStruct;
#endif

/**
 * @brief ProcessDump request information
 * It is used to save and transfer the current process context from signalhandler to processdump,
 * and also contains some other information of the process.
 */
struct ProcessDumpRequest {
    /** processdump type */
    enum ProcessDumpType type;
    /** thread id */
    int32_t tid;
    /** asynchronous thread id */
    /** process id */
    int32_t pid;
    /** namespace process id */
    int32_t nsPid;
    /** process user id */
    uint32_t uid;
    /** reserved field */
    uint64_t reserved;
    /** timestamp */
    uint64_t timeStamp;
    /** current process signal context */
    siginfo_t siginfo;
    /** current process context */
    ucontext_t context;
    /** thread name */
    char threadName[NAME_BUF_LEN];
    /** process name */
    char processName[NAME_BUF_LEN];
    /** Storing different types of messages */
    Message msg;
    /** current porcess fd table address*/
    uint64_t fdTableAddr;
    /** stackId for async-stack */
    uint64_t stackId;
    /** application runing unique Id */
    char appRunningUniqueId[MAX_APP_RUNNING_UNIQUE_ID_LEN];
    /** source child process with processdump pipe */
    int childPipeFd[2];
    /** whether block source process pid */
    intptr_t blockCrashExitAddr;
    /** whether processdump unwind crash success */
    intptr_t unwindResultAddr;
    uintptr_t crashObj;
    uint64_t crashLogConfig;
#ifndef is_ohos_lite
    DumpHiTraceIdStruct hitraceId;
#endif
};

static const int CRASH_BLOCK_EXIT_FLAG  = 0x13579BDF;
static const int CRASH_UNWIND_SUCCESS_FLAG = 0x2468ACEF;
#ifdef __cplusplus
}
#endif
#endif
