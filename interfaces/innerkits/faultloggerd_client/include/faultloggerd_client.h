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
#ifndef DFX_FAULTLOGGERD_CLIENT_H
#define DFX_FAULTLOGGERD_CLIENT_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define FAULTLOGGER_DAEMON_RESP "RESP:COMPLETE"

enum class FaultLoggerType {
    JAVA_CRASH = 1,
    CPP_CRASH,
    JS_CRASH,
    APP_FREEZE,
    JAVA_STACKTRACE = 100, // unsupported yet
    CPP_STACKTRACE,
    JS_STACKTRACE,
    JS_HEAP_SNAPSHOT,
};

enum class FaultLoggerClientType {
    DEFAULT_CLIENT = 0, // For original request crash info file
    LOG_FILE_DES_CLIENT, // For request a file to record nornal unwind and process dump logs.
    PRINT_T_HILOG_CLIENT, // For request a file to record nornal unwind and process dump logs.
    PERMISSION_CLIENT,
    SDK_DUMP_CLIENT,
    MAX_CLIENT
};

enum class FaultLoggerCheckPermissionResp {
    CHECK_PERMISSION_PASS = 1,
    CHECK_PERMISSION_REJECT,
    CHECK_PERMISSION_MAX
};

enum class FaultLoggerSdkDumpResp {
    SDK_DUMP_PASS = 1,
    SDK_DUMP_REJECT,
    SDK_DUMP_MAX
};

struct FaultLoggerdRequest {
    int32_t type;
    int32_t clientType;
    int32_t pid;
    int32_t tid;
    int32_t uid;
    int32_t callerPid; // only for sdk dump client
    int32_t callerTid; // only for sdk dump client
    char module[128];
    uint64_t time;
} __attribute__((packed));

int32_t RequestFileDescriptor(int32_t type);
int32_t RequestLogFileDescriptor(struct FaultLoggerdRequest *request);
int RequestFileDescriptorEx(const struct FaultLoggerdRequest *request);
bool RequestCheckPermission(int32_t pid);
void RequestPrintTHilog(const char *msg, int length);
bool RequestSdkDump(int32_t pid, int32_t tid);

#ifdef __cplusplus
}
#endif

#endif
