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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  type of request
 *
*/
enum FaultLoggerType {
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
    /** leak stacktrace */
    LEAK_STACKTRACE,
    /** ffrt crash log */
    FFRT_CRASH_LOG,
};

/**
 * @brief  type of faultlogger client
 *
*/
enum FaultLoggerClientType {
    /** For original request crash info temp file */
    DEFAULT_CLIENT = 0,
    /** For request a debug file to record nornal unwind and process dump logs */
    LOG_FILE_DES_CLIENT,
    /** For request to record nornal unwind and process dump to hilog */
    PRINT_T_HILOG_CLIENT,
    /** For request to check permission */
    PERMISSION_CLIENT,
    /** For request to dump stack */
    SDK_DUMP_CLIENT,
    /** For request file descriptor of pipe */
    PIPE_FD_CLIENT,
    /** For report crash dump exception */
    REPORT_EXCEPTION_CLIENT,
};

/**
 * @brief  type of request about pipe
*/
enum FaultLoggerPipeType {
    /** For request file descriptor of pipe to read buffer  */
    PIPE_FD_READ_BUF = 0,
    /** For request file descriptor of pipe to write buffer  */
    PIPE_FD_WRITE_BUF,
    /** For request file descriptor of pipe to read result  */
    PIPE_FD_READ_RES,
    /** For request file descriptor of pipe to write result  */
    PIPE_FD_WRITE_RES,
    /** For request file descriptor of pipe to json read buffer  */
    PIPE_FD_JSON_READ_BUF,
    /** For request file descriptor of pipe to json write buffer  */
    PIPE_FD_JSON_WRITE_BUF,
    /** For request file descriptor of pipe to json read result  */
    PIPE_FD_JSON_READ_RES,
    /** For request file descriptor of pipe to json write result  */
    PIPE_FD_JSON_WRITE_RES,
    /** For request to delete file descriptor of pipe */
    PIPE_FD_DELETE,
};
/**
 * @brief  type of responding check permission request
*/
enum FaultLoggerCheckPermissionResp {
    /** pass */
    CHECK_PERMISSION_PASS = 1,
    /** reject */
    CHECK_PERMISSION_REJECT,
};
/**
 * @brief  type of responding sdk dump request
*/
enum FaultLoggerSdkDumpResp {
    /** pass */
    SDK_DUMP_PASS = 1,
    /** reject */
    SDK_DUMP_REJECT,
    /** repeat request */
    SDK_DUMP_REPEAT,
    /** process not exist */
    SDK_DUMP_NOPROC,
};
/**
 * @brief  request information
*/
struct FaultLoggerdRequest {
    /** type of resquest */
    int32_t type;
    /** type of faultlogger client */
    int32_t clientType;
    /** type of pipe */
    int32_t pipeType;
    /** signal code */
    int32_t sigCode;
    /** process id */
    int32_t pid;
    /** thread id */
    int32_t tid;
    /** user id */
    uint32_t uid;
    /** process id of calling sdk dump ,only for sdk dump client */
    int32_t callerPid;
    /** thread id of calling sdk dump ,only for sdk dump client */
    int32_t callerTid;
    /** time of current request */
    uint64_t time;
    /** ture output json string, false output default string */
    bool isJson;
} __attribute__((packed));

#ifdef __cplusplus
}
#endif
#endif