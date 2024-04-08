/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
#include <dirent.h>
#include <mutex>
#include <sstream>
#include <unistd.h>
#include <vector>

#include "backtrace_local_thread.h"
#include "dfx_frame_formatter.h"
#include "dfx_json_formatter.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "directory_ex.h"
#include "procinfo.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktrace"
#define LOG_DOMAIN 0xD002D11
}

bool GetBacktraceFramesByTid(std::vector<DfxFrame>& frames, int32_t tid, size_t skipFrameNum, bool fast,
                             size_t maxFrameNums)
{
    auto unwinder = std::make_shared<Unwinder>();
    BacktraceLocalThread thread(tid, unwinder);
    thread.SetMaxFrameNums(maxFrameNums);
    bool ret = thread.Unwind(skipFrameNum + 1, fast);
    frames = thread.GetFrames();
    return ret;
}

#ifndef is_ohos_lite
bool GetBacktraceJsonByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
    size_t maxFrameNums)
{
    std::vector<DfxFrame> frames;
    bool ret = GetBacktraceFramesByTid(frames, tid, skipFrameNum + 1, fast, maxFrameNums);
    out.clear();
    out = DfxJsonFormatter::GetFramesJson(frames);
    return ret;
}
#endif

bool GetBacktraceStringByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
                             size_t maxFrameNums)
{
    std::vector<DfxFrame> frames;
    bool ret = GetBacktraceFramesByTid(frames, tid, skipFrameNum + 1, fast, maxFrameNums);
    out.clear();
    out = DfxFrameFormatter::GetFramesStr(frames);
    return ret;
}

bool PrintBacktrace(int32_t fd, bool fast, size_t maxFrameNums)
{
    DFXLOG_INFO("%s", "Receive PrintBacktrace request.");
    std::vector<DfxFrame> frames;
    bool ret = GetBacktraceFramesByTid(frames,
        BACKTRACE_CURRENT_THREAD, 1, fast, maxFrameNums); // 1: skip current frame
    if (!ret) {
        return false;
    }

    for (auto const& frame : frames) {
        auto line = DfxFrameFormatter::GetFrameStr(frame);
        if (fd >= 0) {
            dprintf(fd, "    %s", line.c_str());
        }
        DFXLOG_INFO(" %s", line.c_str());
    }
    return ret;
}

bool GetBacktrace(std::string& out, bool fast, size_t maxFrameNums)
{
    DFXLOG_INFO("%s", "Receive GetBacktrace request.");
    return GetBacktraceStringByTid(out, BACKTRACE_CURRENT_THREAD, 1, fast, maxFrameNums); // 1: skip current frame
}

bool GetBacktrace(std::string& out, size_t skipFrameNum, bool fast, size_t maxFrameNums, bool isJson)
{
    DFXLOG_INFO("Receive GetBacktrace request for isJson(%d).", isJson);
    if (isJson) {
#ifndef is_ohos_lite
        return GetBacktraceJsonByTid(out, BACKTRACE_CURRENT_THREAD, skipFrameNum + 1, fast, maxFrameNums);
#endif
    } else {
        return GetBacktraceStringByTid(out, BACKTRACE_CURRENT_THREAD, skipFrameNum + 1, fast, maxFrameNums);
    }
}

bool PrintTrace(int32_t fd, size_t maxFrameNums)
{
    return PrintBacktrace(fd, false, maxFrameNums);
}

const char* GetTrace(size_t skipFrameNum, size_t maxFrameNums)
{
    static std::string trace;
    trace.clear();
    if (!GetBacktrace(trace, skipFrameNum, false, maxFrameNums)) {
        DFXLOG_ERROR("%s", "Failed to get trace string");
    }
    return trace.c_str();
}

static std::string GetStacktraceHeader()
{
    pid_t pid = getprocpid();
    std::ostringstream ss;
    ss << "" << std::endl << "Timestamp:" << GetCurrentTimeStr();
    ss << "Pid:" << pid << std::endl;
    ss << "Uid:" << getuid() << std::endl;
    std::string processName;
    ReadProcessName(pid, processName);
    ss << "Process name::" << processName << std::endl;
    return ss.str();
}

std::string GetProcessStacktrace(size_t maxFrameNums, bool isJson)
{
    auto unwinder = std::make_shared<Unwinder>();
    std::ostringstream ss;
    ss << std::endl << GetStacktraceHeader();
    std::function<bool(int)> func = [&](int tid) {
        if (tid <= 0 || tid == getproctid()) {
            return false;
        }
        BacktraceLocalThread thread(tid, unwinder);
        thread.SetMaxFrameNums(maxFrameNums);
        if (thread.Unwind(0)) {
            ss << thread.GetFormattedStr(true, isJson) << std::endl;
        } else {
            std::string msg;
            if (tid == getprocpid()) {
                ReadProcessStatus(msg, tid);
                ss << msg << std::endl;
                msg = "";
            }
            ReadThreadWchan(msg, tid, true);
            ss << msg << std::endl;
        }
        return true;
    };

    std::vector<int> tids;
    GetTidsByPidWithFunc(getprocpid(), tids, func);

    return ss.str();
}
} // namespace HiviewDFX
} // namespace OHOS
