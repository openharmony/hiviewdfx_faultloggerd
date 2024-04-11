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
#include "process_dumper.h"
#include "printer.h"
#include "unique_stack_table.h"

namespace OHOS {
namespace HiviewDFX {
void DfxUnwindAsyncThread::UnwindStack()
{
    if (unwinder_ == nullptr || thread_ == nullptr) {
        DFXLOG_ERROR("%s::thread or unwinder is not initialized.", __func__);
        return;
    }
    // 1: get crash stack
    // unwinding with context passed by dump request, only for crash thread or target thread.
    unwinder_->SetRegs(thread_->GetThreadRegs());
    MAYBE_UNUSED bool ret = unwinder_->UnwindRemote(thread_->threadInfo_.nsTid,
                                                    ProcessDumper::GetInstance().IsCrash(),
                                                    DfxConfig::GetConfig().maxFrameNums);
    if (ProcessDumper::GetInstance().IsCrash()) {
        ReportUnwinderException(unwinder_->GetLastErrorCode());
    }

    if (!ret && ProcessDumper::GetInstance().IsCrash()) {
        UnwindThreadFallback();
    }

    if (ProcessDumper::GetInstance().IsCrash()) {
        thread_->SetFrames(unwinder_->GetFrames());
        UnwindThreadByParseStackIfNeed();
        // 2: get submitterStack
        std::vector<DfxFrame> submmiterFrames;
        GetSubmitterStack(submmiterFrames);
        // 3: merge two stack
        MergeStack(submmiterFrames);
    } else {
        thread_->Detach();
        thread_->SetFrames(unwinder_->GetFrames());
    }
    DFXLOG_INFO("%s, unwind tid(%d) finish.", __func__, thread_->threadInfo_.nsTid);
}

void DfxUnwindAsyncThread::GetSubmitterStack(std::vector<DfxFrame> &submitterFrames)
{
    if (stackId_ == 0) {
        return;
    }
    const std::shared_ptr<DfxMaps>& maps = unwinder_->GetMaps();
    std::vector<std::shared_ptr<DfxMap>> mapVec;
    if (!maps->FindMapsByName("[anon:async_stack_table]", mapVec)) {
        DFXLOG_ERROR("%s::Can not find map of async stack table", __func__);
        return;
    }
    auto map = mapVec.front();
    size_t size = map->end - map->begin;
    auto tableData = std::make_shared<std::vector<uint8_t>>(size);
    size_t byte = DfxMemory::ReadProcMemByPid(thread_->threadInfo_.nsTid, map->begin, tableData->data(), size);
    if (byte != size) {
        DFXLOG_ERROR("%s", "Failed to read unique_table from target");
        return;
    }
    auto table = std::unique_ptr<UniqueStackTable>(new UniqueStackTable(tableData->data(), size));
    std::vector<uintptr_t> pcs;
    StackId id;
    id.value = stackId_;
    if (table->GetPcsByStackId(id, pcs)) {
        unwinder_->GetFramesByPcs(submitterFrames, pcs);
    } else {
        DFXLOG_WARN("%s::Failed to get pcs", __func__);
    }
}

void DfxUnwindAsyncThread::MergeStack(std::vector<DfxFrame> &submitterFrames)
{
    auto frames = thread_->GetFrames();
    frames.insert(frames.end(), submitterFrames.begin(), submitterFrames.end());
    thread_->SetFrames(frames);
}

void DfxUnwindAsyncThread::UnwindThreadFallback()
{
#ifndef __x86_64__
    if (unwinder_->GetFrames().size() > 0) {
        return;
    }
    // As we failed to init libunwind, just print pc and lr for first two frames
    std::shared_ptr<DfxRegs> regs = thread_->GetThreadRegs();
    if (regs == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("RegisterInfo is not existed for crash process");
        return;
    }

    std::shared_ptr<DfxMaps> maps = unwinder_->GetMaps();
    if (maps == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("MapsInfo is not existed for crash process");
        return;
    }
    std::shared_ptr<Unwinder> unwinder = unwinder_;

    auto createFrame = [maps, unwinder] (size_t index, uintptr_t pc, uintptr_t sp = 0) {
        std::shared_ptr<DfxMap> map;
        DfxFrame frame;
        frame.pc = pc;
        frame.sp = sp;
        frame.index = index;
        if (maps->FindMapByAddr(pc, map)) {
            frame.relPc = map->GetRelPc(pc);
            frame.mapName = map->name;
        } else {
            frame.relPc = pc;
            frame.mapName = (index == 0 ? "Not mapped pc" : "Not mapped lr");
        }
        unwinder->AddFrame(frame);
    };

    createFrame(0, regs->GetPc(), regs->GetSp());
    createFrame(1, *(regs->GetReg(REG_LR)));
#endif
}

void DfxUnwindAsyncThread::UnwindThreadByParseStackIfNeed()
{
    if (thread_ == nullptr || unwinder_ == nullptr) {
        return;
    }
    auto frames = thread_->GetFrames();
    constexpr int minFramesNum = 3;
    size_t initSize = frames.size();
    if (initSize < minFramesNum || frames[minFramesNum - 1].mapName.find("Not mapped") != std::string::npos) {
        bool needParseStack = true;
        thread_->InitFaultStack(needParseStack);
        auto faultStack = thread_->GetFaultStack();
        if (faultStack == nullptr || !faultStack->ParseUnwindStack(unwinder_->GetMaps(), frames)) {
            DFXLOG_ERROR("%s : Failed to parse unwind stack.", __func__);
            return;
        }
        thread_->SetFrames(frames);
        tip = StringPrintf(
            " Failed to unwind stack, try to get unreliable call stack from #%02zu by reparsing thread stack",
            initSize);
    }
}
}
}