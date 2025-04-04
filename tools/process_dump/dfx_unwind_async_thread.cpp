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
#include "dfx_unwind_async_thread.h"

#include "crash_exception.h"
#include "dfx_config.h"
#include "dfx_log.h"
#include "dfx_memory.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#ifdef PARSE_LOCK_OWNER
#include "lock_parser.h"
#endif
#include "process_dumper.h"
#include "printer.h"
#include "unique_stack_table.h"

namespace OHOS {
namespace HiviewDFX {
bool DfxUnwindAsyncThread::UnwindStack(pid_t vmPid)
{
    // 1: get crash stack
    // unwinding with context passed by dump request, only for crash thread or target thread.
    auto regs = thread_.GetThreadRegs();
    unwinder_.SetRegs(regs);
    pid_t tid = thread_.threadInfo_.nsTid;
    DFXLOGI("%{public}s, unwind tid(%{public}d) start.", __func__, tid);
    auto tmpPid = vmPid != 0 ? vmPid : tid;
    DFX_TRACE_START("KeyThreadUnwindRemote:%d", tid);
    bool ret = unwinder_.UnwindRemote(tmpPid, regs != nullptr, DfxConfig::GetConfig().maxFrameNums);
    DFX_TRACE_FINISH();
    if (ProcessDumper::GetInstance().IsCrash()) {
        ReportUnwinderException(unwinder_.GetLastErrorCode());
#ifndef __x86_64__
        if (!ret) {
            UnwindThreadFallback();
        }
#endif
        DFX_TRACE_START("KeyThreadGetFrames:%d", tid);
        thread_.SetFrames(unwinder_.GetFrames());
        DFX_TRACE_FINISH();
        UnwindThreadByParseStackIfNeed();
        // 2: get submitterStack
        std::vector<DfxFrame> submmiterFrames;
        GetSubmitterStack(submmiterFrames);
        thread_.AddFrames(submmiterFrames);
    } else {
#ifdef PARSE_LOCK_OWNER
        DFX_TRACE_START("KeyThreadGetFrames:%d", tid);
        thread_.SetFrames(unwinder_.GetFrames());
        DFX_TRACE_FINISH();
        LockParser::ParseLockInfo(unwinder_, tmpPid, thread_.threadInfo_.nsTid);
#else
        DFX_TRACE_START("KeyThreadGetFrames:%d", tid);
        thread_.SetFrames(unwinder_.GetFrames());
        DFX_TRACE_FINISH();
#endif
    }

    DFXLOGI("%{public}s, unwind tid(%{public}d) finish ret(%{public}d).", __func__, tid, ret);
    return ret;
}

void DfxUnwindAsyncThread::GetSubmitterStack(std::vector<DfxFrame> &submitterFrames)
{
    if (stackId_ == 0) {
        return;
    }
    const std::shared_ptr<DfxMaps>& maps = unwinder_.GetMaps();
    if (maps == nullptr) {
        return;
    }
    std::vector<std::shared_ptr<DfxMap>> mapVec;
    if (!maps->FindMapsByName("[anon:async_stack_table]", mapVec)) {
        DFXLOGE("%{public}s::Can not find map of async stack table", __func__);
        return;
    }
    auto map = mapVec.front();
    size_t size = map->end - map->begin;
    auto tableData = std::make_shared<std::vector<uint8_t>>(size);
    size_t byte = ReadProcMemByPid(thread_.threadInfo_.nsTid, map->begin, tableData->data(), size);
    if (byte != size) {
        DFXLOGE("Failed to read unique_table from target");
        return;
    }
    auto table = std::make_shared<UniqueStackTable>(tableData->data(), size, false);
    std::vector<uintptr_t> pcs;
    StackId id;
    id.value = stackId_;
    if (table->GetPcsByStackId(id, pcs)) {
        unwinder_.GetFramesByPcs(submitterFrames, pcs);
    } else {
        DFXLOGW("%{public}s::Failed to get pcs", __func__);
    }
}

#ifndef __x86_64__
void DfxUnwindAsyncThread::CreateFrame(DfxMaps& dfxMaps, size_t index, uintptr_t pc, uintptr_t sp)
{
    std::shared_ptr<DfxMap> map;
    DfxFrame frame;
    frame.pc = pc;
    frame.sp = sp;
    frame.index = index;
    if (dfxMaps.FindMapByAddr(pc, map)) {
        frame.relPc = map->GetRelPc(pc);
        frame.mapName = map->name;
    } else {
        frame.relPc = pc;
        frame.mapName = (index == 0 ? "Not mapped pc" : "Not mapped lr");
    }
    unwinder_.AddFrame(frame);
}

void DfxUnwindAsyncThread::UnwindThreadFallback()
{
    if (unwinder_.GetFrames().size() > 0) {
        return;
    }
    // As we failed to init libunwinder, just print pc and lr for first two frames
    std::shared_ptr<DfxRegs> regs = thread_.GetThreadRegs();
    if (regs == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("RegisterInfo is not existed for crash process");
        return;
    }

    std::shared_ptr<DfxMaps> maps = unwinder_.GetMaps();
    if (maps == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("MapsInfo is not existed for crash process");
        return;
    }
    CreateFrame(*maps, 0, regs->GetPc(), regs->GetSp());
    CreateFrame(*maps, 1, *(regs->GetReg(REG_LR)));
}
#endif

void DfxUnwindAsyncThread::UnwindThreadByParseStackIfNeed()
{
    auto frames = thread_.GetFrames();
    constexpr int minFramesNum = 3;
    size_t initSize = frames.size();
    if (initSize < minFramesNum || frames[minFramesNum - 1].mapName.find("Not mapped") != std::string::npos ||
        frames[minFramesNum - 1].mapName.find("[Unknown]") != std::string::npos) {
        bool needParseStack = true;
        thread_.InitFaultStack(needParseStack);
        auto faultStack = thread_.GetFaultStack();
        if (faultStack == nullptr || !faultStack->ParseUnwindStack(unwinder_.GetMaps(), frames)) {
            DFXLOGE("%{public}s : Failed to parse unwind stack.", __func__);
            return;
        }
        thread_.SetFrames(frames);
        unwindFailTip = StringPrintf(
            "Failed to unwind stack, try to get unreliable call stack from #%02zu by reparsing thread stack.",
            initSize);
    }
}
}
}