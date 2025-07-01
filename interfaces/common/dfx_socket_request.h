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

#ifndef DFX_SOCKET_REQUEST_H
#define DFX_SOCKET_REQUEST_H

#include <inttypes.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  type of request
 *
*/
typedef enum FaultLoggerType : int32_t {
    /** C/C++ crash at runtime */
    CPP_CRASH = 2,
    /** js crash at runtime */
    JS_CRASH,
    /** application freeze */
    APP_FREEZE,
    /** trace native stack */
    CPP_STACKTRACE = 101,
    /** trace js stack */
    JS_STACKTRACE,
    /** js heap */
    JS_HEAP_SNAPSHOT,
    /** js raw heap */
    JS_RAW_SNAPSHOT,
    /** js heap leak list */
    JS_HEAP_LEAK_LIST,
    /** leak stacktrace */
    LEAK_STACKTRACE,
    /** ffrt crash log */
    FFRT_CRASH_LOG,
    /** jit code log */
    JIT_CODE_LOG,
    /** address sanitizer log*/
    ADDR_SANITIZER,
    /** cj heap */
    CJ_HEAP_SNAPSHOT = 110
} FaultLoggerType;

/**
 * @brief  type of faultlogger client
 *
*/
typedef enum FaultLoggerClientType : int8_t {
    /** For request a debug file to record nornal unwind and process dump logs */
    LOG_FILE_DES_CLIENT,
    /** For request to dump stack */
    SDK_DUMP_CLIENT,
    /** For request file descriptor of pipe */
    PIPE_FD_CLIENT,
    /** For report crash dump exception */
    REPORT_EXCEPTION_CLIENT,
    /** For report dump stats */
    DUMP_STATS_CLIENT,
    /** For request to coredump */
    DO_COREDUMP_CLIENT,
    /** For request to cancel coredump */
    CANCEL_COREDUMP_CLIENT,
    /** For request to report coredump status */
    COREDUMP_PROCESS_DUMP_CLIENT,
    /** For request liteperf file descriptor of pipe */
    PIPE_FD_LITEPERF_CLIENT,
} FaultLoggerClientType;

typedef struct RequestDataHead {
    /** type of faultlogger client */
    int8_t clientType;
    /** target process id outside sandbox */
    int32_t clientPid;
} __attribute__((packed)) RequestDataHead;

typedef struct CoreDumpRequestData {
    /** request data head **/
    RequestDataHead head;
    /** target id */
    int32_t pid;
    /** endtime ms */
    uint64_t endTime;
} __attribute__((packed)) CoreDumpRequestData;

/**
 * @brief  type of coredump Status
 *
*/
typedef enum CoreDumpStatus : int32_t {
    /** core dump start */
    CORE_DUMP_START = 1,
    /** core dump end */
    CORE_DUMP_END,
    /** core dump error */
    CORE_DUMP_ERROR,
} CoreDumpStatus;

typedef struct CoreDumpStatusData {
    /** request data head **/
    RequestDataHead head;
    /** target id */
    int32_t pid;
    /** process id */
    int32_t processDumpPid;
    /** coredump status */
    // 1 start  2 end  3 error
    int32_t coredumpStatus;
    /** coredump file name */
    char fileName[256];
    /** coredump result */
    int32_t retCode;
} __attribute__((packed)) CoreDumpStatusData;

typedef struct SocketReceiveData {
    /** coredump file name **/
    char fileName[256];
    /** coredump result */
    int32_t retCode;
} __attribute__((packed)) SocketReceiveData;

typedef struct SdkDumpRequestData {
    /** request data head **/
    RequestDataHead head;
    /** process id */
    int32_t pid;
    /** signal code */
    int32_t sigCode;
    /** thread id */
    int32_t tid;
    /** thread id of calling sdk dump ,only for sdk dump client */
    int32_t callerTid;
    /** time of current request */
    uint64_t time;
    /** dumpcatcher remote unwind endtime ms */
    uint64_t endTime;
} __attribute__((packed)) SdkDumpRequestData;

/**
 * @brief  type of request about pipe
*/
typedef enum FaultLoggerPipeType : int8_t {
    /** For request file descriptor of pipe to read buffer  */
    PIPE_FD_READ = 0,
    /** For request file descriptor of pipe to write buffer  */
    PIPE_FD_WRITE,
    /** For request to delete file descriptor of pipe */
    PIPE_FD_DELETE,
} FaultLoggerPipeType;

typedef struct PipFdRequestData {
    /** request data head **/
    RequestDataHead head;
    /** process id */
    int32_t pid;
    /** type of pipe */
    int8_t pipeType;
} __attribute__((packed)) PipFdRequestData;

/**
 * @brief  request information
*/
typedef struct FaultLoggerdRequest {
    /** request data head **/
    RequestDataHead head;
    /** process id */
    int32_t pid;
    /** type of resquest */
    int32_t type;
    /** thread id */
    int32_t tid;
    /** time of current request */
    uint64_t time;
} __attribute__((packed)) FaultLoggerdRequest;

/**
 * @brief  type of faultloggerd stats request
*/
typedef enum FaultLoggerdStatType : int32_t {
    /** dump catcher stats */
    DUMP_CATCHER = 0,
    /** processdump stats */
    PROCESS_DUMP
} FaultLoggerdStatType;

/**
 * @brief  struct of faultloggerd stats request
*/
typedef struct FaultLoggerdStatsRequest {
    /** request data head **/
    RequestDataHead head;
    /** type of resquest */
    int32_t type;
    /** target process id outside sandbox */
    int32_t pid;
    /** the time call dumpcatcher interface */
    uint64_t requestTime;
    /** the time signal arrive in targe process */
    uint64_t signalTime;
    /** the time enter processdump main */
    uint64_t processdumpStartTime;
    /** the time finish processdump */
    uint64_t processdumpFinishTime;
    /** the time return from dumpcatcher interface */
    uint64_t dumpCatcherFinishTime;
    /** dumpcatcher result */
    int32_t result;
    /** caller elf offset */
    uintptr_t offset;
    char summary[128]; // 128 : max summary size
    /** the caller elf of dumpcatcher interface */
    char callerElf[128]; // 128 : max function name size
    /** the caller processName */
    char callerProcess[128]; // 128 : max function name size
    /** the target processName */
    char targetProcess[128]; // 128 : max function name size
} __attribute__((packed)) FaultLoggerdStatsRequest;

typedef enum ResponseCode : int32_t {
    /** failed receive msg form server */
    RECEIVE_DATA_FAILED = -4,
    /** failed send msg to server */
    SEND_DATA_FAILED = -3,
    /** failed connect to server */
    CONNECT_FAILED = -2,
    /** default code **/
    DEFAULT_ERROR_CODE = -1,
    /** request success */
    REQUEST_SUCCESS = 0,
    /** unknown client type */
    UNKNOWN_CLIENT_TYPE = 1,
    /** the data size is not matched to client type */
    INVALID_REQUEST_DATA = 2,
    /** reject to resolve the request */
    REQUEST_REJECT = 3,
    /** abnormal service */
    ABNORMAL_SERVICE = 4,
    /** repeat dump */
    SDK_DUMP_REPEAT,
    /** the process to dump not exist */
    SDK_DUMP_NOPROC,
    /** the process to dump has crashed */
    SDK_PROCESS_CRASHED,
    /** repeat coredump */
    CORE_DUMP_REPEAT,
    /** the process to coredump has crashed */
    CORE_PROCESS_CRASHED,
    /** the process to coredump not exist */
    CORE_DUMP_NOPROC,
    /** cancel coredump */
    CORE_DUMP_CANCEL,
    /** coredump generate fail */
    CORE_DUMP_GENERATE_FAIL,
} ResponseCode;

#ifdef __cplusplus
}
#endif
#endif