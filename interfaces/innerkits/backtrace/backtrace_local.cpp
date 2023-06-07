/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "backtrace_local.h"

#include <cstring>
#include <mutex>
#include <sstream>
#include <vector>

#include "dfx_frame_format.h"
#include "dfx_util.h"
#include "directory_ex.h"
#include "backtrace_local_thread.h"

#include <dirent.h>
#include <hilog/log.h>
#include <unistd.h>

#include <libunwind_i-ohos.h>

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktrace"
#define LOG_DOMAIN 0xD002D11

constexpr int SKIP_ONE_FRAME = 1; // skip current frame
}

bool GetBacktraceFramesByTid(std::vector<DfxFrame>& frames, int32_t tid, size_t skipFrameNum, bool fast)
{
    bool ret = false;
    BacktraceLocalThread thread(tid);
    if (fast) {
#ifdef __aarch64__
        ret = thread.Unwind(nullptr, nullptr, skipFrameNum, fast);
#endif
    }
    if (!ret) {
        unw_addr_space_t as;
        unw_init_local_address_space(&as);
        if (as == nullptr) {
            return ret;
        }
        auto symbol = std::make_shared<DfxSymbols>();

        ret = thread.Unwind(as, symbol, skipFrameNum, fast);

        unw_destroy_local_address_space(as);
    }
    frames.clear();
    frames = thread.GetFrames();
    return ret;
}

bool GetBacktraceStringByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast)
{
    bool ret = false;
    BacktraceLocalThread thread(tid);
    if (fast) {
#ifdef __aarch64__
        ret = thread.Unwind(nullptr, nullptr, skipFrameNum, fast);
#endif
    }
    if (!ret) {
        unw_addr_space_t as;
        unw_init_local_address_space(&as);
        if (as == nullptr) {
            return ret;
        }
        auto symbol = std::make_shared<DfxSymbols>();

        ret = thread.Unwind(as, symbol, skipFrameNum, fast);

        unw_destroy_local_address_space(as);
    }
    out.clear();
    out = thread.GetFormatedStr();
    return ret;
}

bool PrintBacktrace(int32_t fd, bool fast)
{
    std::vector<DfxFrame> frames;
    bool ret = GetBacktraceFramesByTid(frames, BACKTRACE_CURRENT_THREAD, SKIP_ONE_FRAME, fast);
    if (!ret) {
        return false;
    }

    for (auto const& frame : frames) {
        auto line = DfxFrameFormat::GetFrameStr(frame);
        if (fd < 0) {
            // print to hilog
            HILOG_INFO(LOG_CORE, " %{public}s", line.c_str());
        } else {
            dprintf(fd, "    %s", line.c_str());
        }
    }
    return ret;
}

bool GetBacktrace(std::string& out, bool fast)
{
    return GetBacktraceStringByTid(out, BACKTRACE_CURRENT_THREAD, SKIP_ONE_FRAME, fast);
}

bool GetBacktrace(std::string& out, size_t skipFrameNum, bool fast)
{
    return GetBacktraceStringByTid(out, BACKTRACE_CURRENT_THREAD, skipFrameNum + 1, fast);
}

bool PrintTrace(int32_t fd)
{
    return PrintBacktrace(fd, false);
}

const char* GetTrace(size_t skipFrameNum)
{
    static std::string trace;
    trace.clear();
    if (!GetBacktrace(trace, skipFrameNum)) {
        HILOG_ERROR(LOG_CORE, "Failed to get trace string");
    }
    return trace.c_str();
}

static std::string GetStacktraceHeader()
{
    pid_t pid = getpid();
    std::ostringstream ss;
    ss << "" << std::endl << "Timestamp:" << GetCurrentTimeStr();
    ss << "Pid:" << pid << std::endl;
    ss << "Uid:" << getuid() << std::endl;
    std::string processName;
    ReadProcessName(pid, processName);
    ss << "Process name::" << processName << std::endl;
    return ss.str();
}

std::string GetProcessStacktrace()
{
    unw_addr_space_t as;
    unw_init_local_address_space(&as);
    if (as == nullptr) {
        return "";
    }

    DIR *dir = opendir(PROC_SELF_TASK_PATH);
    if (dir == nullptr) {
        return "";
    }

    std::ostringstream ss;
    ss << std::endl << GetStacktraceHeader();
    auto symbol = std::make_shared<DfxSymbols>();
    auto curTid = gettid();
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        pid_t tid = atoi(ent->d_name);
        if (tid <= 0 || tid == curTid) {
            continue;
        }

        BacktraceLocalThread thread(tid);
        if (thread.Unwind(as, symbol, 0)) {
            ss << thread.GetFormatedStr(true) << std::endl;
        }
    }

    closedir(dir);
    unw_destroy_local_address_space(as);
    return ss.str();
}
} // namespace HiviewDFX
} // namespace OHOS
