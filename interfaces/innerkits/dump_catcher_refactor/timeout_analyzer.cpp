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

#include "timeout_analyzer.h"

#include <vector>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_dump_catcher_errno.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "file_ex.h"
#include "procinfo.h"
#include "string_printf.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "TimeoutAnalyzer"
#endif

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr int SIGNAL_MAX_NUM = 64;
constexpr int SIGBLK_HEX_LEN = 16;
constexpr int SIGBLK_FIELD_OFFSET = 2;
const std::string WATCHDOG_THREAD_NAME = "OS_DfxWatchdog";
}

bool TimeoutAnalyzer::IsBitOn(const std::string& content, const std::string& field, int signal)
{
    if (content.find(field) == std::string::npos) {
        return false;
    }
    if (signal <= 0 || signal > SIGNAL_MAX_NUM) {
        DFXLOGW("IsBitOn invalid signal %{public}d", signal);
        return false;
    }
    // SigBlk:   0000000000000000
    size_t pos = content.find(field) + field.size() + SIGBLK_FIELD_OFFSET;
    if (pos > content.size() || pos + SIGBLK_HEX_LEN > content.size()) {
        DFXLOGW("IsBitOn content too short");
        return false;
    }
    std::string num = content.substr(pos, SIGBLK_HEX_LEN);
    uint64_t hexValue = strtoul(num.c_str(), nullptr, 16);
    uint64_t mask = 1ULL << (signal - 1);
    return (hexValue & mask) != 0;
}

bool TimeoutAnalyzer::FindWatchdogTid(int pid, int& targetTid)
{
    std::vector<int> tids;
    std::vector<int> nstids;
    GetTidsByPid(pid, tids, nstids);
    std::string threadName;
    for (size_t i = 0; i < tids.size(); ++i) {
        ReadThreadNameByPidAndTid(pid, tids[i], threadName);
        if (threadName == WATCHDOG_THREAD_NAME) {
            targetTid = tids[i];
            return true;
        }
    }
    return false;
}

bool TimeoutAnalyzer::IsSignalBlocked(int pid, int32_t& ret)
{
    int targetTid = -1;
    if (!FindWatchdogTid(pid, targetTid)) {
        return false;
    }
    std::string content;
    std::string threadStatusPath = StringPrintf("/proc/%d/task/%d/status", pid, targetTid);
    if (!LoadStringFromFile(threadStatusPath, content) || content.empty()) {
        DFXLOGE("the pid(%{public}d)thread(%{public}d) read status fail, errno(%{public}d)", pid, targetTid, errno);
        ret = DUMPCATCH_TIMEOUT_PARSE_FAIL_READ_ESTATUS;
        return true;
    }
    if (IsBitOn(content, "SigBlk", SIGDUMP) || IsBitOn(content, "SigIgn", SIGDUMP)) {
        DFXLOGI("the pid(%{public}d)thread(%{public}d) signal has been blocked by target process", pid, targetTid);
        ret = DUMPCATCH_TIMEOUT_SIGNAL_BLOCK;
        return true;
    }
    return false;
}

bool TimeoutAnalyzer::IsFrozen(int pid, int32_t& ret)
{
    std::string content;
    std::string cgroupPath = StringPrintf("/proc/%d/cgroup", pid);
    if (!LoadStringFromFile(cgroupPath, content)) {
        DFXLOGE("the pid (%{public}d) read cgroup fail, errno (%{public}d)", pid, errno);
        ret = DUMPCATCH_TIMEOUT_PARSE_FAIL_READ_ECGROUP;
        return true;
    }
    if (content.find("Frozen") != std::string::npos) {
        DFXLOGI("the pid (%{public}d) has been frozen", pid);
        ret = DUMPCATCH_TIMEOUT_KERNEL_FROZEN;
        return true;
    }
    return false;
}

void TimeoutAnalyzer::AnalyzeTimeoutReason(int pid, int32_t& ret)
{
    std::string statusPath = StringPrintf("/proc/%d/status", pid);
    if (access(statusPath.c_str(), F_OK) != 0) {
        DFXLOGI("the pid (%{public}d) process exit during the dump, errno (%{public}d)", pid, errno);
        ret = DUMPCATCH_TIMEOUT_PROCESS_KILLED;
        return;
    }
    if (IsSignalBlocked(pid, ret)) {
        return;
    }
    if (IsFrozen(pid, ret)) {
        return;
    }
    DFXLOGI("the pid (%{public}d) dump slow", pid);
    ret = DUMPCATCH_TIMEOUT_DUMP_SLOW;
}

} // namespace HiviewDFX
} // namespace OHOS
