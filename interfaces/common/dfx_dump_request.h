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
    /** dump process stack */
    DUMP_TYPE_PROCESS,
    /** dump thread stack */
    DUMP_TYPE_THREAD,
};

/**
 * @brief Process trace information
 */
enum OperateResult : int32_t {
    OPE_FAIL = 0,
    OPE_SUCCESS = 1,
    OPE_CONTINUE = 2,
};

enum ProcessFlowMode : int32_t {
    SPLIT_MODE,
    FUSION_MODE,
};

enum RemoteProcessType : int32_t {
    MAIN_PROCESS,
    VIRTUAL_PROCESS
};

enum PidType : int32_t {
    REAL_PROCESS_PID,
    VIRTUAL_PROCESS_PID,
    PID_MAX,
};

/**
 * @brief Process trace information
 * keep sync with the definition in hitracec.h
 */
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

typedef enum {
    NONE = 0,
    MESSAGE_FATAL, // hilog last fatal log message
    MESSAGE_FDSAN_DEBUG, // fdsan debug message
    MESSAGE_JEMALLOC, // jemalloc message
    MESSAGE_BADFD, // badfd message
} MessageType;

typedef struct {
    MessageType type;
    char body[MAX_FATAL_MSG_SIZE];
} Message;

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
    int32_t recycleTid;
    /** process id */
    int32_t pid;
    /** namespace process id */
    int32_t nsPid;
    /** virtual process id */
    int32_t vmPid;
    /** virtual namespace process id */
    int32_t vmNsPid;
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
    /** process trace info */
    TraceInfo traceInfo;
    /** current porcess fd table address*/
    uint64_t fdTableAddr;
    /** stackId for async-stack */
    uint64_t stackId;
    /** application runing unique Id */
    char appRunningId[MAX_APP_RUNNING_UNIQUE_ID_LEN];
    /** source process with processdump pipe */
    int pmPipeFd[2];
    /** vm process with proceeesump pipe */
    int vmPipeFd[2];
    /** is integrate crash dump flow 0:false 1:true */
    int32_t dumpMode;
    /** vm process pid addr */
    intptr_t vmProcRealPid;
    uintptr_t crashObj;
};
#ifdef __cplusplus
}
#endif
#endif
