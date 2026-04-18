/*
* Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "minidump_manager_service.h"

#include <fcntl.h>
#include <memory>
#include <securec.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <thread>

#include "dfx_log.h"
#include "dfx_util.h"
#include "smart_fd.h"

#undef LOG_TAG
#define LOG_TAG "MinidumpManagerService"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr uint32_t MAX_PDUMP_THREAD_COUNT = 400;


const char* DumpTypeToString(int dumpType)
{
    switch (dumpType) {
        case __PDUMP_TYPE_CALLSTACK:  return "callstack";
        case __PDUMP_TYPE_MINIDUMP:   return "minidump";
        case __PDUMP_TYPE_COREDUMP:   return "coredump";
        default:                      return "unknown";
    }
}

const char* DataTypeToString(int dataType)
{
    switch (dataType) {
        case __DATA_TYPE_WORK_START:  return "WORK_START";
        case __DATA_TYPE_WORK_END:    return "WORK_END";
        default:                      return "UNKNOWN";
    }
}
}

void PDumpListener::OnEventPoll()
{
    DFXLOGI("recv dev pdump event");

    constexpr size_t pdumpDataSize = sizeof(struct __pdump_data_s);
    struct __pdump_data_s srcData;
    ssize_t bytesRead;

    while ((bytesRead = OHOS_TEMP_FAILURE_RETRY(read(GetFd(), &srcData, pdumpDataSize))) > 0) {
        if (bytesRead != static_cast<ssize_t>(pdumpDataSize)) {
            DFXLOGE("read incomplete pdump header, bytesRead=%{public}zd, expected=%{public}zu",
                bytesRead, pdumpDataSize);
            continue;
        }

        if (!MinidumpManagerService::GetInstance().ParsePDumpData(srcData)) {
            DFXLOGE("failed to parse pdump data");
        }
    }

    if (bytesRead < 0 && errno != EAGAIN) {
        DFXLOGE("failed to read pdump data, errno=%{public}d", errno);
    }

    DFXLOGI("finish deal pdump event");
}

MinidumpManagerService& MinidumpManagerService::GetInstance()
{
    static MinidumpManagerService instance;
    return instance;
}

bool MinidumpManagerService::Init()
{
    DFXLOGI("minidump manager init");
    pFd_ = open("/dev/pdump", O_NONBLOCK);
    if (pFd_ < 0) {
        DFXLOGE("failed to open /dev/pdump, errno=%{public}d", errno);
        return false;
    }

    struct __pdump_init_arg_s initArg = {0};
    initArg.target_pid = __PDUMP_PID_NO_RESTRICT;
    initArg.children_only = false;
    initArg.read_nonblock = false;
    initArg.dump_type_flag = __PDUMP_TYPE_FLAG_MINIDUMP;
    initArg.config.nr_threads_max = MAX_PDUMP_THREAD_COUNT;

    int ret = ioctl(pFd_, __PDUMP_IOCTL_INIT, &initArg);
    if (ret < 0) {
        DFXLOGE("failed to ioctl init pdump, errno=%{public}d", errno);
        close(pFd_);
        pFd_ = -1;
        return false;
    }
    DFXLOGI("pdump init successfully");

    auto listener = std::make_unique<PDumpListener>(SmartFd{pFd_}, true);
    EpollManager::GetInstance().AddListener(std::move(listener));
    return true;
}

int MinidumpManagerService::SetMiniDump(pid_t pid, int8_t enableMinidump, int8_t enableMinidumpToCrashLog)
{
    if (pFd_ < 0) {
        DFXLOGE("pdump device not initialized");
        return -1;
    }
    std::lock_guard<std::mutex> lock(configsMutex_);
    if (enableMinidump == 1) {
        enableMinidumpConfigs.emplace(pid);
    } else if (enableMinidump == 0) {
        enableMinidumpConfigs.erase(pid);
    }
    if (enableMinidumpToCrashLog == 0) {
        disableMinidumpToCrashLogConfigs.emplace(pid);
    } else if (enableMinidumpToCrashLog == 1) {
        disableMinidumpToCrashLogConfigs.erase(pid);
    }
    DFXLOGE("success to call set %{public}d %{public}d pdumpable", enableMinidump, enableMinidumpToCrashLog);
    return 0;
}

bool MinidumpManagerService::ParsePDumpData(const struct __pdump_data_s& data)
{
    DFXLOGI("processing data chunk: workid=%{public}u, data_type=%{public}s",
            data.header.workid, DataTypeToString(data.header.data_type));

    switch (data.header.data_type) {
        case __DATA_TYPE_WORK_START:
            ProcessWorkStart(data);
            break;
        case __DATA_TYPE_WORK_END:
            ProcessWorkEnd(data);
            break;
        default:
            DFXLOGE("invalid pdump data type: %{public}d", data.header.data_type);
            return false;
    }
    return true;
}

void MinidumpManagerService::ProcessWorkStart(const struct __pdump_data_s& data)
{
    DFXLOGI("dump started: workid=%{public}u, type=%{public}s pid=%{public}d, pipeFd=%{public}d",
        data.header.workid,
        DumpTypeToString(data.data.work_data.dump_type),
        data.data.work_data.pid,
        data.data.work_data.pipefd);
    bool enableMinidump = false;
    bool enableMinidumpToCrashLog = false;
    {
        std::lock_guard<std::mutex> lock(configsMutex_);
        enableMinidump = enableMinidumpConfigs.count(data.data.work_data.pid) == 1;
        enableMinidumpToCrashLog = disableMinidumpToCrashLogConfigs.count(data.data.work_data.pid) == 0;
        enableMinidumpConfigs.erase(data.data.work_data.pid);
        disableMinidumpToCrashLogConfigs.erase(data.data.work_data.pid);
    }
    if (!enableMinidump && !enableMinidumpToCrashLog) {
        struct __pdump_work_cancel_arg_s arg = {0};
        arg.workid = data.header.workid;
        int ret = ioctl(pFd_, __PDUMP_IOCTL_CANCEL, &arg);
        if (ret < 0) {
            DFXLOGE("failed to ioctl cancel pdump, errno=%{public}d", errno);
        }
        close(data.data.work_data.pipefd);
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        pid_t childPid = fork();
        if (childPid == 0) {
            DFXLOGI("start launch processdump to handle minidump %{public}d, %{public}d",
                enableMinidump, enableMinidumpToCrashLog);
            char argStr[32]; // 32 : pid buf len
            int ret = snprintf_s(argStr, sizeof(argStr), sizeof(argStr) - 1, "%d %d %d %d",
                data.data.work_data.pid, data.data.work_data.pipefd,
                static_cast<int8_t>(enableMinidump), static_cast<int8_t>(enableMinidumpToCrashLog));
            if (ret < 0) {
                DFXLOGE("fill dumpPid fail, not launch processdump %{public}d", ret);
                _exit(0);
            }
            execl(PROCESSDUMP_PATH, "processdump", "-minidump", argStr, NULL);
        }
        _exit(0);
    }
    waitpid(pid, nullptr, 0);
}

void MinidumpManagerService::ProcessWorkEnd(const struct __pdump_data_s& data)
{
    DFXLOGI("dump completed: workid=%{public}u type=%{public}s errcode=%{public}u outputBytes=%{public}u",
        data.header.workid,
        DumpTypeToString(data.data.result_data.dump_type),
        data.data.result_data.errcode,
        data.data.result_data.output_bytes);
}
}
}
