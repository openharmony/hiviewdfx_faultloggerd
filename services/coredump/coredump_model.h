/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef COREDUMP_MODEL_H
#define COREDUMP_MODEL_H

#include <chrono>
#include <string>
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
enum class CoredumpStatus {
    PENDING,         // 请求已接收，正在等待核心转储开始
    RUNNING,         // 核心转储进行中
    SUCCESS,         // 核心转储成功完成
    FAILED,          // 核心转储失败
    CANCEL_PENDING,  // 收到取消请求，正在等待取消执行
    CANCELED,        // 已成功取消核心转储
    CANCEL_TIMEOUT,  // 取消操作超时，核心转储未及时终止
    INVALID_STATUS
};

enum class CoredumpEvent {
    UPDATE_DUMPER,  // Worker 上报 PID
    CANCEL,         // 客户端发起取消
    REPORT_SUCCESS, // Worker 上报成功
    REPORT_FAIL,    // Worker 上报失败
    TIMEOUT,        // 超时未上报
    INVALID_EVENT
};

struct CreateCoredumpRequest {
    pid_t targetPid;
    std::string dumpType;
    int clientFd;
    uint64_t endTime;
};

struct CoredumpCallbackReport {
    pid_t workerPid;
    CoredumpStatus status;
    std::string filePath;
    int errorCode;
};

using SessionId = pid_t;
struct CoredumpSession {
    SessionId sessionId {0};
    pid_t workerPid {0};
    bool cancelRequest {false};
    CoredumpStatus status {CoredumpStatus::PENDING};
    std::string filePath;
    int errorCode {0};
    int clientFd {-1};
    uint64_t startTime {0};
    uint64_t endTime {0};
    uint64_t delayTaskId {0};

    static std::string StatusToString(CoredumpStatus coredumpStatus)
    {
        switch (coredumpStatus) {
            case CoredumpStatus::PENDING:
                return "PENDING";
            case CoredumpStatus::RUNNING:
                return "RUNNING";
            case CoredumpStatus::SUCCESS:
                return "SUCCESS";
            case CoredumpStatus::FAILED:
                return "FAILED";
            case CoredumpStatus::CANCEL_PENDING:
                return "CANCEL_PENDING";
            case CoredumpStatus::CANCELED:
                return "CANCELED";
            case CoredumpStatus::CANCEL_TIMEOUT:
                return "CANCEL_TIMEOUT";
            default:
                return "UNKNOWN";
        }
    }

    std::string ToString() const
    {
        return std::string("sessionId: ") +  std::to_string(sessionId) + " " + StatusToString(status);
    }
};
}
}
#endif
