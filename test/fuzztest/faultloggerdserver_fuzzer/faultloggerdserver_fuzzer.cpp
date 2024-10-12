/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "faultloggerdserver_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <unistd.h>
#include <sys/socket.h>

#include "dfx_exception.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "faultloggerd_fuzzertest_common.h"
#include "fault_logger_config.h"
#define private public
#include "fault_logger_daemon.h"
#undef private

namespace OHOS {
namespace HiviewDFX {
constexpr int32_t FAULTLOGGERD_FUZZ_READ_BUFF = 1024;

void *ReadThread1(void *param)
{
    int fd = static_cast<int>(reinterpret_cast<long>(param));
    char buff[FAULTLOGGERD_FUZZ_READ_BUFF];
    OHOS_TEMP_FAILURE_RETRY(read(fd, buff, sizeof(buff)));
    char msg[] = "any test str";
    OHOS_TEMP_FAILURE_RETRY(write(fd, reinterpret_cast<char*>(msg), sizeof(msg)));
    return nullptr;
}

void *ReadThread2(void *param)
{
    int fd = static_cast<int>(reinterpret_cast<long>(param));
    char buff[FAULTLOGGERD_FUZZ_READ_BUFF];
    OHOS_TEMP_FAILURE_RETRY(read(fd, buff, sizeof(buff)));
    CrashDumpException test;
    test.error = CRASH_DUMP_LOCAL_REPORT;
    OHOS_TEMP_FAILURE_RETRY(write(fd, reinterpret_cast<char*>(&test), sizeof(CrashDumpException)));
    return nullptr;
}

void *ReadThread3(void *param)
{
    int fd = static_cast<int>(reinterpret_cast<long>(param));
    char buff[FAULTLOGGERD_FUZZ_READ_BUFF];
    OHOS_TEMP_FAILURE_RETRY(read(fd, buff, sizeof(buff)));
    CrashDumpException test{};
    OHOS_TEMP_FAILURE_RETRY(write(fd, reinterpret_cast<char*>(&test), sizeof(CrashDumpException)));
    return nullptr;
}

void HandleRequestTestCommon(std::shared_ptr<FaultLoggerDaemon> daemon, char* buff, size_t len,
    void *(*startRoutine)(void *))
{
    int socketFd[2]; // 2 : the length of the array
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
        OHOS_TEMP_FAILURE_RETRY(write(socketFd[1], buff, len));
        daemon->connectionMap_[socketFd[0]] = socketFd[0];

        if (!startRoutine) {
            daemon->HandleRequest(0, socketFd[0]);
        } else {
            pthread_t threadId;
            if (pthread_create(&threadId, nullptr, startRoutine, reinterpret_cast<void*>(socketFd[1])) != 0) {
                perror("Failed to create thread");
                close(socketFd[0]);
                close(socketFd[1]);
                return;
            }

            daemon->HandleRequest(0, socketFd[0]);

            pthread_join(threadId, nullptr);
        }
        close(socketFd[1]);
    }
}

std::vector<FaultLoggerdStatsRequest> InitStatsRequests(const uint8_t* data, size_t size)
{
    std::vector<FaultLoggerdStatsRequest> statsRequest;
    {
        FaultLoggerdStatsRequest requst{};
        requst.type = PROCESS_DUMP;
        requst.signalTime = GetTimeMilliSeconds();
        requst.pid = 1;
        requst.processdumpStartTime = time(nullptr);
        requst.processdumpFinishTime = time(nullptr) + 10; // 10 : Get the last 10 seconds of the current time
        statsRequest.emplace_back(requst);
    }
    {
        FaultLoggerdStatsRequest requst{};
        auto lastRequst = statsRequest.back();
        requst.type = DUMP_CATCHER;
        requst.pid = lastRequst.pid;
        requst.requestTime = time(nullptr);
        requst.dumpCatcherFinishTime = time(nullptr) + 10; // 10 : Get the last 10 seconds of the current time
        requst.result = 1;
        statsRequest.emplace_back(requst);
    }
    {
        FaultLoggerdStatsRequest requst{};
        requst.type = DUMP_CATCHER;
        requst.pid = 1;
        requst.requestTime = time(nullptr);
        requst.dumpCatcherFinishTime = time(nullptr) + 10; // 10 : Get the last 10 seconds of the current time
        requst.result = 1;
        statsRequest.emplace_back(requst);
    }
    return statsRequest;
}

void HandleRequestByClientTypeForDefaultClientTest(std::shared_ptr<FaultLoggerDaemon> daemon)
{
    FaultLoggerdRequest requst;
    requst.clientType = FaultLoggerClientType::DEFAULT_CLIENT;
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    requst.type = FaultLoggerType::CPP_CRASH;
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    requst.type = FaultLoggerType::JS_HEAP_SNAPSHOT;
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    daemon->crashTimeMap_[1] = time(nullptr) - 10; // 10 : Get the first 10 seconds of the current time
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    daemon->crashTimeMap_[1] = time(nullptr);
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
}

void HandleRequestByClientTypeTest(std::shared_ptr<FaultLoggerDaemon> daemon)
{
    HandleRequestByClientTypeForDefaultClientTest(daemon);
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::LOG_FILE_DES_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::PRINT_T_HILOG_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::PERMISSION_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::SDK_DUMP_CLIENT;
        requst.pid = 1;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::PIPE_FD_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::REPORT_EXCEPTION_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread1);
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread3);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = -1;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    }
}

void HandStaticTest(std::shared_ptr<FaultLoggerDaemon> daemon)
{
    daemon->HandleStaticForFuzzer(FaultLoggerType::CPP_STACKTRACE, 0);
    daemon->HandleStaticForFuzzer(FaultLoggerType::JS_STACKTRACE, 0);
    daemon->HandleStaticForFuzzer(FaultLoggerType::LEAK_STACKTRACE, 1);
    daemon->HandleStaticForFuzzer(FaultLoggerType::FFRT_CRASH_LOG, 1);
    daemon->HandleStaticForFuzzer(FaultLoggerType::JIT_CODE_LOG, 1);
}

bool CheckReadResp(int sockfd)
{
    char controlBuffer[SOCKET_BUFFER_SIZE] = {0};
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(sockfd, controlBuffer, sizeof(controlBuffer) - 1));
    if (nread != static_cast<ssize_t>(strlen(FAULTLOGGER_DAEMON_RESP))) {
        return false;
    }
    return true;
}

void HandleRequestByPipeTypeCommon(std::shared_ptr<FaultLoggerDaemon> daemon, int32_t pipeType,
    bool isPassCheck = false, bool isJson = false)
{
    int fd = -1;
    FaultLoggerdRequest request;
    request.pipeType = pipeType;
    std::unique_ptr<FaultLoggerPipe2> ptr = std::make_unique<FaultLoggerPipe2>(GetTimeMilliSeconds(), isJson);

    if (!isPassCheck) {
        daemon->HandleRequestByPipeType(fd, 1, &request, ptr.get());
        close(fd);
        return;
    }

    int socketFd[2]; // 2 : the length of the array
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
        pid_t pid = fork();
        if (pid == 0) {
            sleep(1);
            if (CheckReadResp(socketFd[1])) {
                std::string test = "test";
                OHOS_TEMP_FAILURE_RETRY(write(socketFd[1], test.c_str(), sizeof(test)));
            }
        } else if (pid > 0) {
            daemon->connectionMap_[socketFd[0]] = socketFd[0];
            daemon->HandleRequestByPipeType(fd, socketFd[0], &request, ptr.get());
            close(fd);
            close(socketFd[1]);
        }
    }
}

void HandleRequestByPipeTypeTest(std::shared_ptr<FaultLoggerDaemon> daemon)
{
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_READ_BUF, true, false);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_READ_RES, true, false);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_DELETE, true, false);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_JSON_READ_BUF, true, true);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_JSON_READ_RES, true, true);

    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_READ_BUF);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_WRITE_BUF);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_READ_RES);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_WRITE_RES);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_JSON_READ_BUF, false, true);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_BUF, false, true);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_JSON_READ_RES, false, true);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES, false, true);
    HandleRequestByPipeTypeCommon(daemon, -1);
}

void FaultloggerdServerTestCallOnce(const uint8_t* data, size_t size)
{
    static bool callCnt = false;
    if (callCnt) {
        return;
    }
    callCnt = true;

    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    // HandleRequest
    daemon->HandleRequest(-1, 0);

    // HandleRequestTest
    std::vector<FaultLoggerdStatsRequest> statsRequest = InitStatsRequests(data, size);
    for (auto requst : statsRequest) {
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdStatsRequest), nullptr);
    }

    // CheckReadRequest
    {
        int socketFd[2]; // 2 : the length of the array
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
            close(socketFd[1]);
            daemon->HandleRequest(0, socketFd[0]);
        }
    }
    char buff[] = "any test str";
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(buff), sizeof(buff), nullptr);

    // CheckRequestCredential
    {
        FaultLoggerdRequest requst;
        int socketFd[2]; // 2 : the length of the array
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
            OHOS_TEMP_FAILURE_RETRY(write(socketFd[1], reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest)));
            daemon->HandleRequest(0, socketFd[0]);
            close(socketFd[1]);
        }
    }

    // init
    if (!daemon->InitEnvironment()) {
        daemon->CleanupSockets();
        return;
    }

    HandleRequestByClientTypeTest(daemon);
    HandleRequestByPipeTypeTest(daemon);
    HandStaticTest(daemon);
}

void FaultloggerdServerTest(const uint8_t* data, size_t size)
{
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();

    int32_t epollFd;
    int32_t connectionFd;
    int32_t type;
    int32_t pid;
    int32_t tid;
    int32_t uid;

    int offsetTotalLength = sizeof(epollFd) + sizeof(connectionFd) + sizeof(type) + sizeof(pid) +
                            sizeof(tid) + sizeof(uid);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, epollFd);
    STREAM_TO_VALUEINFO(data, connectionFd);
    STREAM_TO_VALUEINFO(data, type);
    STREAM_TO_VALUEINFO(data, pid);
    STREAM_TO_VALUEINFO(data, tid);
    STREAM_TO_VALUEINFO(data, uid);

    if (epollFd < 0 || connectionFd < 3) { // 3: not allow fd = 0,1,2 because they are reserved by system
        return;
    }

    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.type = type;
    request.pid = pid;
    request.tid = tid;
    request.uid = uid;
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();

    daemon->HandleRequestForFuzzer(epollFd, connectionFd, &request, &request);
    sleep(1);
}
} // namespace HiviewDFX
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    /* Run your code on data */
    OHOS::HiviewDFX::FaultloggerdServerTestCallOnce(data, size);
    OHOS::HiviewDFX::FaultloggerdServerTest(data, size);
    return 0;
}
