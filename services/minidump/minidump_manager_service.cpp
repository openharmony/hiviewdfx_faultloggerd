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

#include <memory>
#include <thread>
#include <securec.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "dfx_log.h"
#include "dfx_util.h"
#include "smart_fd.h"

#undef LOG_TAG
#define LOG_TAG "MinidumpManagerService"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr size_t BUFFER_SIZE = 4096;
constexpr const char* const MINIDUMP_PATH = "/data/log/faultlog/temp/";
constexpr const char* const MINIDUMP_PREFIX = "minidump-";
constexpr const char* const MINIDUMP_EXT = ".dmp";
constexpr uint32_t MAX_PDUMP_THREAD_COUNT = 400;

std::string GenerateMinidumpFilename(pid_t pid)
{
    return std::string(MINIDUMP_PATH) + MINIDUMP_PREFIX +
           std::to_string(pid) + "-" + std::to_string(GetTimeMilliSeconds());
}

bool TransferData(int srcFd, int dstFd)
{
    std::vector<char> buffer(BUFFER_SIZE);

    ssize_t totoBytesRead = 0;
    ssize_t bytesRead;

    while ((bytesRead = read(srcFd, buffer.data(), buffer.size())) > 0) {
        totoBytesRead += bytesRead;
        ssize_t bytesWritten = 0;
        while (bytesWritten < bytesRead) {
            ssize_t ret = write(dstFd, buffer.data() + bytesWritten, bytesRead - bytesWritten);
            if (ret < 0) {
                DFXLOGE("write failed, errno=%{public}d", errno);
                return false;
            }
            bytesWritten += ret;
        }
    }

    if (bytesRead < 0) {
        DFXLOGE("read failed, errno=%{public}d", errno);
        return false;
    }

    DFXLOGI("totol read form srcFd=%{public}d totoBytesRead=%{public}zd", srcFd, totoBytesRead);
    return true;
}

bool FinalizeMinidumpFile(const std::string& tmpPath)
{
    std::string finalPath = tmpPath + MINIDUMP_EXT;
    if (rename(tmpPath.c_str(), finalPath.c_str()) != 0) {
        DFXLOGE("failed to rename file from %{private}s to %{private}s, errno=%{public}d",
                tmpPath.c_str(), finalPath.c_str(), errno);
        unlink(tmpPath.c_str());
        return false;
    }
    DFXLOGI("minidump file finalized: %{public}s", finalPath.c_str());
    return true;
}

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
    initArg.target_pid = __PDUMP_PID_FROM_CONFIG;
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

bool MinidumpManagerService::GenerateMinidump(int pipeFd, pid_t pid)
{
    if (pipeFd < 0) {
        DFXLOGE("invalid pipeFd=%{public}d", pipeFd);
        return false;
    }
    SmartFd pipeGuard(pipeFd);

    std::string tmpFilename = GenerateMinidumpFilename(pid);
    int outputFd = open(tmpFilename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outputFd < 0) {
        DFXLOGE("failed to create temp file %{private}s, errno=%{public}d", tmpFilename.c_str(), errno);
        return false;
    }
    SmartFd outputGuard(outputFd);

    DFXLOGI("created temp file for minidump: pid=%{public}d", pid);

    if (!TransferData(pipeFd, outputFd)) {
        DFXLOGE("failed to transfer minidump data for pid=%{public}d", pid);
        unlink(tmpFilename.c_str());
        return false;
    }

    DFXLOGI("successfully transfered minidump data for pid=%{public}d", pid);
    return FinalizeMinidumpFile(tmpFilename);
}

int MinidumpManagerService::SetMiniDump(pid_t pid, bool enable)
{
    if (pFd_ < 0) {
        DFXLOGE("pdump device not initialized");
        return -1;
    }

    struct __pdump_dumpable_arg_s arg = {0};
    arg.target_pid = pid;
    int rc = 0;
    if (enable) {
        rc = ioctl(pFd_, __PDUMP_IOCTL_SET_DUMPABLE, &arg);
    } else {
        rc = ioctl(pFd_, __PDUMP_IOCTL_CLEAR_DUMPABLE, &arg);
    }

    if (rc < 0) {
        DFXLOGE("failed to call set %{public}d pdumpable errno%{public}d", enable, errno);
    } else {
        DFXLOGI("success to call set %{public}d pdumpable", enable);
    }
    return rc;
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

    std::thread(GenerateMinidump, data.data.work_data.pipefd, data.data.work_data.pid).detach();
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
