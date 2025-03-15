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

typedef enum {
    MESSAGE_NONE = 0,
    MESSAGE_FATAL, // Need to write LastFatalMessage
    MESSAGE_CALLBACK, // call back memssage
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
    char appRunningId[MAX_APP_RUNNING_UNIQUE_ID_LEN];
    /** source child process with processdump pipe */
    int childPipeFd[2];
    /** vm process pid addr */
    intptr_t vmProcRealPidAddr;
    /** whether block source process pid */
    intptr_t blockCrashExitAddr;
    /** whether processdump unwind crash success */
    intptr_t unwindResultAddr;
    uintptr_t crashObj;
};

static const int CRASH_BLOCK_EXIT_FLAG  = 0x13579BDF;
static const int CRASH_UNWIND_SUCCESS_FLAG = 0x2468ACEF;
#ifdef __cplusplus
}
#endif
#endif
