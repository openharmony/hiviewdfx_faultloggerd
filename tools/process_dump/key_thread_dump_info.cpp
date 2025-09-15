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

#include "key_thread_dump_info.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <link.h>
#include <securec.h>
#include <thread>

#include "crash_exception.h"
#include "decorative_dump_info.h"
#include "dfx_dump_request.h"
#include "dfx_log.h"
#include "dump_utils.h"
#include "process_dump_config.h"
#include "dfx_define.h"
#include "dfx_frame_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_ptrace.h"
#include "dfx_regs.h"
#include "dfx_buffer_writer.h"
#include "dfx_symbols.h"
#include "dfx_thread.h"
#include "dfx_trace.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
bool ParseUnwindStack(const std::vector<uintptr_t>& stackValues,
    std::shared_ptr<DfxMaps> maps, std::vector<DfxFrame>& frames)
{
    bool hasAddFrame = false;
    if (maps == nullptr) {
        DFXLOGE("%{public}s : maps is null.", __func__);
        return false;
    }
    size_t index = frames.size();
    for (const auto& value : stackValues) {
        std::shared_ptr<DfxMap> map;
        if (!maps->FindMapByAddr(value, map) ||
            (map->prots & PROT_EXEC) == 0) {
            continue;
        }
        DfxFrame frame;
        frame.index = index;
        frame.pc = value;
        frame.relPc = map->GetRelPc(frame.pc);
        frame.map = map;
        frame.mapName = map->name;
        frames.emplace_back(frame);
        hasAddFrame = true;
        constexpr int MAX_VALID_ADDRESS_NUM = 32;
        if (++index >= MAX_VALID_ADDRESS_NUM) {
            return true;
        }
    }
    return hasAddFrame;
}
}

REGISTER_DUMP_INFO_CLASS(KeyThreadDumpInfo);

void KeyThreadDumpInfo::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    auto thread = process.GetKeyThread();
    if (thread == nullptr) {
        DFXLOGE("key thread is nullptr!");
        return;
    }
    std::string dumpInfo;
    if (request.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        dumpInfo += "Fault thread info:\n";
    }

    dumpInfo += thread->ToString();
    DfxBufferWriter::GetInstance().WriteMsg(dumpInfo);
    DfxBufferWriter::GetInstance().AppendBriefDumpInfo(dumpInfo);
}

int KeyThreadDumpInfo::UnwindStack(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DFXLOGI("unwind key thread dump start");
    int result = 0;
    if (process.GetKeyThread() == nullptr) {
        DFXLOGW("%{public}s::unwind thread is not initialized.", __func__);
        return result;
    }
    if (process.GetVmPid() == 0) {
        DFXLOGW("vm pid is null, can not unwind key thread!");
        return result;
    }
    unwinder.SetIsJitCrashFlag(request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    if (process.GetKeyThread()->GetThreadRegs() != nullptr) {
        result = GetKeyThreadStack(process, unwinder) ? 1 : 0;
    } else {
        DumpUtils::GetThreadKernelStack(*process.GetKeyThread());
    }
    unwinder.SetIsJitCrashFlag(false);
    if (!unwindFailTip_.empty()) {
        ReportCrashException(CrashExceptionCode::CRASH_UNWIND_ESTACK);
        process.AppendFatalMessage(unwindFailTip_);
    }
    return result;
}

bool KeyThreadDumpInfo::GetKeyThreadStack(DfxProcess& process, Unwinder& unwinder)
{
    // unwinding with context passed by dump request, only for crash thread or target thread
    auto thread = process.GetKeyThread();
    pid_t tid = thread->GetThreadInfo().nsTid;
    DFXLOGI("%{public}s, unwind tid(%{public}d) start.", __func__, tid);
    auto tmpPid = process.GetVmPid() != 0 ? process.GetVmPid() : tid;
    auto regs = thread->GetThreadRegs();
    unwinder.SetRegs(regs);
    DFX_TRACE_START("KeyThreadUnwindRemote:%d", tid);
    bool ret = unwinder.UnwindRemote(tmpPid, regs != nullptr,
                                     ProcessDumpConfig::GetInstance().GetConfig().maxFrameNums);
    DFX_TRACE_FINISH();
    DFX_TRACE_START("KeyThreadGetFrames:%d", tid);
    thread->SetFrames(unwinder.GetFrames());
    for (const auto& frame : unwinder.GetFrames()) {
        if (!frame.isJsFrame) {
            process.AddNativeFramesTable(std::make_pair(frame.pc, frame));
        }
    }
    DFX_TRACE_FINISH();
    ReportUnwinderException(unwinder.GetLastErrorCode());
    UnwindThreadByParseStackIfNeed(thread, unwinder.GetMaps());
#ifdef PARSE_LOCK_OWNER
    DumpUtils::ParseLockInfo(unwinder, tmpPid, thread->GetThreadInfo().nsTid);
#endif
    DFXLOGI("%{public}s, unwind tid(%{public}d) finish ret(%{public}d).", __func__, tid, ret);
    return ret;
}

void KeyThreadDumpInfo::UnwindThreadByParseStackIfNeed(std::shared_ptr<DfxThread> thread, std::shared_ptr<DfxMaps> maps)
{
    auto frames = thread->GetFrames();
    constexpr int minFramesNum = 3;
    size_t initSize = frames.size();
    if (initSize < minFramesNum || frames[minFramesNum - 1].mapName.find("Not mapped") != std::string::npos ||
        frames[minFramesNum - 1].mapName.find("[Unknown]") != std::string::npos) {
        bool needParseStack = true;
        auto faultStack = std::make_shared<FaultStack>();
        if (faultStack == nullptr) {
            return;
        }
        faultStack->CollectStackInfo(thread->GetThreadInfo().nsTid, frames, needParseStack);
        if (!ParseUnwindStack(faultStack->GetStackValues(), maps, frames)) {
            DFXLOGE("%{public}s : Failed to parse unwind stack.", __func__);
            return;
        }
        thread->SetFrames(frames);
        unwindFailTip_ = StringPrintf(
            "Failed to unwind stack, try to get unreliable call stack from #%02zu by reparsing thread stack.",
            initSize);
    }
}
}
}