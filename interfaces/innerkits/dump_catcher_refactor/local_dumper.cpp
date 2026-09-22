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

#include "local_dumper.h"

#include <unistd.h>
#include <vector>

#include "backtrace_local.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "dump_catcher_constants.h"
#include "procinfo.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "LocalDumper"
#endif

namespace OHOS {
namespace HiviewDFX {

bool LocalDumper::DumpCurrentTid(size_t skipFrameNum, std::string& msg, size_t maxFrameNums)
{
    bool ret = GetBacktrace(msg, skipFrameNum + 1, false, maxFrameNums);
    if (!ret) {
        int currTid = gettid();
        msg.append("Failed to dump curr thread:" + std::to_string(currTid) + ".\n");
    }
    DFXLOGD("DumpCurrentTid :: return %{public}d.", ret);
    return ret;
}

bool LocalDumper::DumpLocalTid(int tid, std::string& msg, size_t maxFrameNums)
{
    if (tid <= 0) {
        DFXLOGE("DumpLocalTid :: return false as param error.");
        return false;
    }
    bool ret = GetBacktraceStringByTid(msg, tid, 0, false, maxFrameNums);
    if (!ret) {
        msg.append("Failed to dump thread:" + std::to_string(tid) + ".\n");
    }
    DFXLOGD("DumpLocalTid :: return %{public}d.", ret);
    return ret;
}

bool LocalDumper::DumpLocalPid(int pid, std::string& msg, size_t maxFrameNums)
{
    if (pid <= 0) {
        DFXLOGE("DumpLocalPid :: return false as param error.");
        return false;
    }
    size_t skipFramNum = DumpCatcherConstants::SKIP_FRAME_FOR_DUMP_PID;
    msg = GetStacktraceHeader();
    bool ret = false;
    auto func = [this, &ret, &msg, skipFramNum, maxFrameNums](int tid) {
        if (tid <= 0) {
            return false;
        }
        std::string threadMsg;
        if (tid == gettid()) {
            ret = DumpCurrentTid(skipFramNum, threadMsg, maxFrameNums);
        } else {
            ret = DumpLocalTid(tid, threadMsg, maxFrameNums);
        }
        msg += threadMsg;
        return ret;
    };
    std::vector<int> tids;
    ret = GetTidsByPidWithFunc(getpid(), tids, func);
    DFXLOGD("DumpLocalPid :: return %{public}d.", ret);
    return ret;
}

bool LocalDumper::DumpLocalLocked(int pid, int tid, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;
    if (tid == gettid()) {
        size_t skipFramNum = DumpCatcherConstants::SKIP_FRAME_FOR_DUMP_LOCAL;
        ret = DumpCurrentTid(skipFramNum, msg, maxFrameNums);
    } else if (tid == 0) {
        ret = DumpLocalPid(pid, msg, maxFrameNums);
    } else if (!IsThreadInPid(pid, tid)) {
        msg.append("tid(" + std::to_string(tid) + ") is not in pid(" + std::to_string(pid) + ").\n");
    } else {
        ret = DumpLocalTid(tid, msg, maxFrameNums);
    }
    DFXLOGD("DumpLocalLocked :: ret(%{public}d).", ret);
    return ret;
}

} // namespace HiviewDFX
} // namespace OHOS
