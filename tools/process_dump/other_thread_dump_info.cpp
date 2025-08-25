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
#include "decorative_dump_info.h"
#include "crash_exception.h"
#include "dfx_log.h"
#include "dump_utils.h"
#include "dfx_trace.h"
#include "process_dump_config.h"
#include "dfx_buffer_writer.h"
namespace OHOS {
namespace HiviewDFX {
REGISTER_DUMP_INFO_CLASS(OtherThreadDumpInfo);

int OtherThreadDumpInfo::UnwindStack(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    int unwindSuccessCnt = DecorativeDumpInfo::UnwindStack(process, request, unwinder);
    DFXLOGI("unwind other thread dump start");
    auto pid = process.GetVmPid();
    if (pid == 0) {
        DFXLOGW("vm pid is null, can not unwind other thread!");
        return unwindSuccessCnt;
    }
    for (const auto &thread : process.GetOtherThreads()) {
        auto regs = thread->GetThreadRegs();
        unwinder.SetRegs(regs);
        bool withRegs = regs != nullptr;
        pid_t tid = thread->GetThreadInfo().nsTid;
        if (!withRegs) {
            DFXLOGI("not with regs");
            DumpUtils::GetThreadKernelStack(*thread);
            continue;
        }
        DFX_TRACE_START("OtherThreadUnwindRemote:%d", tid);
        if (unwinder.UnwindRemote(pid, withRegs, ProcessDumpConfig::GetInstance().GetConfig().maxFrameNums)) {
            unwindSuccessCnt++;
        } else {
            DFXLOGW("%{public}s, unwind tid(%{public}d) failed.", __func__, tid);
        }
        DFX_TRACE_FINISH();
        DFX_TRACE_START("OtherThreadGetFrames:%d", tid);
        thread->SetFrames(unwinder.GetFrames());
        DFX_TRACE_FINISH();
#ifdef PARSE_LOCK_OWNER
        DumpUtils::ParseLockInfo(unwinder, pid, tid);
#endif
        ReportUnwinderException(unwinder.GetLastErrorCode());
    }
    return unwindSuccessCnt;
}

void OtherThreadDumpInfo::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    std::string dumpInfo;
    if (request.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH && !process.GetOtherThreads().empty()) {
        dumpInfo += "Other thread info:\n";
    }
    for (const auto &thread : process.GetOtherThreads()) {
        dumpInfo += thread->ToString();
    }
    DfxBufferWriter::GetInstance().WriteMsg(dumpInfo);
}

void OtherThreadDumpInfo::Symbolize(DfxProcess& process, Unwinder& unwinder)
{
    DecorativeDumpInfo::Symbolize(process, unwinder);
    for (const auto &thread : process.GetOtherThreads()) {
        DFX_TRACE_START("ParseSymbol otherThread:%d", thread->GetThreadInfo().nsTid);
        thread->ParseSymbol(unwinder);
        DFX_TRACE_FINISH();
    }
}
}
}