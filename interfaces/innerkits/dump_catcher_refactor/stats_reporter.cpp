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

#include "stats_reporter.h"

#include <cstring>
#include <dlfcn.h>
#include <vector>

#include "dfx_dump_catcher_errno.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "dfx_util.h"
#include "file_ex.h"
#include "procinfo.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "StatsReporter"
#endif

namespace OHOS {
namespace HiviewDFX {

namespace {
enum DumpStatRes : int32_t {
    DUMP_RES_NO_KERNELSTACK = -2,
    DUMP_RES_WITH_KERNELSTACK = -1,
    DUMP_RES_WITH_USERSTACK = 0,
    DUMP_RES_WITH_USERSTACK_NO_PARSE_SYMBOL = 1,
    DUMP_RES_WITH_USERSTACK_PARSE_SYMBOL_TIMEOUT = 2,
};
}

int32_t StatsReporter::ConvertDumpResultToDumpStats(int32_t dumpRes)
{
    int32_t stats = DUMP_RES_WITH_KERNELSTACK;
    switch (dumpRes) {
        case DUMPCATCH_ESUCCESS:
            stats = DUMP_RES_WITH_USERSTACK;
            break;
        case DUMPCATCH_DUMP_ESYMBOL_NO_PARSE:
            stats = DUMP_RES_WITH_USERSTACK_NO_PARSE_SYMBOL;
            break;
        case DUMPCATCH_DUMP_ESYMBOL_PARSE_TIMEOUT:
            stats = DUMP_RES_WITH_USERSTACK_PARSE_SYMBOL_TIMEOUT;
            break;
        default:
            break;
    }
    return stats;
}

bool StatsReporter::CopyProcessName(int32_t pid, struct FaultLoggerdStatsRequest* stat)
{
    std::string processName;
    ReadProcessName(pid, processName);
    size_t copyLen = std::min(sizeof(stat->targetProcess) - 1, processName.size());
    if (memcpy_s(stat->targetProcess, sizeof(stat->targetProcess) - 1, processName.c_str(), copyLen) != 0) {
        DFXLOGE("Failed to copy target process");
        return false;
    }
    return true;
}

bool StatsReporter::CopySummary(int32_t ret, struct FaultLoggerdStatsRequest* stat)
{
    if (ret == DUMPCATCH_ESUCCESS) {
        return true;
    }
    std::string summary = DfxDumpCatchError::ToString(ret);
    size_t copyLen = std::min(sizeof(stat->summary) - 1, summary.size());
    if (memcpy_s(stat->summary, sizeof(stat->summary) - 1, summary.c_str(), copyLen) != 0) {
        DFXLOGE("Failed to copy dumpcatcher summary");
        return false;
    }
    return true;
}

bool StatsReporter::CopyCallerElf(void* retAddr, struct FaultLoggerdStatsRequest* stat)
{
    Dl_info info;
    if (dladdr(retAddr, &info) == 0 || info.dli_fname == nullptr || info.dli_fbase == nullptr) {
        return true;
    }
    size_t copyLen = std::min(sizeof(stat->callerElf) - 1, strlen(info.dli_fname));
    if (memcpy_s(stat->callerElf, sizeof(stat->callerElf) - 1, info.dli_fname, copyLen) != 0) {
        DFXLOGE("Failed to copy caller elf info");
        return false;
    }
    stat->offset = reinterpret_cast<uintptr_t>(retAddr) - reinterpret_cast<uintptr_t>(info.dli_fbase);
    return true;
}

bool StatsReporter::CopyCallerCmdline(struct FaultLoggerdStatsRequest* stat)
{
    std::string cmdline;
    if (!OHOS::LoadStringFromFile("/proc/self/cmdline", cmdline)) {
        return true;
    }
    size_t copyLen = std::min(sizeof(stat->callerProcess) - 1, cmdline.size());
    if (memcpy_s(stat->callerProcess, sizeof(stat->callerProcess) - 1, cmdline.c_str(), copyLen) != 0) {
        DFXLOGE("Failed to copy caller cmdline");
        return false;
    }
    return true;
}

void StatsReporter::ReportDumpCatcherStats(int32_t pid, uint64_t requestTime, int32_t ret, void* retAddr)
{
    std::vector<uint8_t> buf(sizeof(struct FaultLoggerdStatsRequest), 0);
    auto stat = reinterpret_cast<struct FaultLoggerdStatsRequest*>(buf.data());
    stat->type = DUMP_CATCHER;
    stat->pid = pid;
    stat->requestTime = requestTime;
    stat->dumpCatcherFinishTime = GetTimeMilliSeconds();
    stat->result = ConvertDumpResultToDumpStats(ret);
    if ((stat->result == DUMP_RES_WITH_KERNELSTACK) && state_.stack.msg.empty()) {
        stat->result = DUMP_RES_NO_KERNELSTACK;
    }
    stat->targetProcessThreadCount = state_.stack.threadCount;
    if (!CopyProcessName(pid, stat) || !CopySummary(ret, stat) || !CopyCallerElf(retAddr, stat) ||
        !CopyCallerCmdline(stat)) {
        return;
    }
    ReportDumpStats(stat);
}

} // namespace HiviewDFX
} // namespace OHOS
