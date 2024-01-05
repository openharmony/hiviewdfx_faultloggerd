/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "dfx_unwind_remote.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <link.h>
#include <securec.h>

#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_symbols.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "process_dumper.h"
#include "printer.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
void UnwindThreadByParseStackIfNeed(std::shared_ptr<DfxProcess> &process,
    std::shared_ptr<DfxThread> &thread, std::shared_ptr<Unwinder> unwinder)
{
    if (process == nullptr || thread == nullptr) {
        return;
    }
    auto frames = unwinder->GetFrames();
    constexpr int minFramesNum = 3;
    size_t initSize = frames.size();
    if (initSize < minFramesNum || frames[minFramesNum - 1].mapName.find("Not mapped") != std::string::npos) {
        bool needParseStack = true;
        thread->InitFaultStack(needParseStack);
        auto faultStack = thread->GetFaultStack();
        if (faultStack == nullptr || !faultStack->ParseUnwindStack(unwinder->GetMaps(), frames)) {
            DFXLOG_ERROR("%s : Failed to parse unwind stack.", __func__);
            return;
        }
        thread->SetFrames(frames);
        std::string tip = StringPrintf(
            " Failed to unwind stack, try to get call stack from #%02zu by reparsing thread stack", initSize);
        std::string msg = process->GetFatalMessage() + tip;
        process->SetFatalMessage(msg);
    }
}
}

DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                    std::shared_ptr<Unwinder> unwinder)
{
    bool ret = false;
    if (process == nullptr || unwinder == nullptr) {
        DFXLOG_WARN("%s::process or unwinder is not initialized.", __func__);
        return ret;
    }

    std::shared_ptr<DfxThread> unwThread = process->keyThread_;
    if (ProcessDumper::GetInstance().IsCrash() && (process->vmThread_ != nullptr)) {
        unwThread = process->vmThread_;
    }

    do {
        if (unwThread == nullptr) {
            break;
        }

        // unwinding with context passed by dump request, only for crash thread or target thread.
        unwinder->SetRegs(unwThread->GetThreadRegs());
        ret = unwinder->UnwindRemote(unwThread->threadInfo_.tid, true, DfxConfig::GetConfig().maxFrameNums);
        if (!ret && ProcessDumper::GetInstance().IsCrash()) {
            UnwindThreadFallback(process, unwThread, unwinder);
        }
        unwThread->SetFrames(unwinder->GetFrames());
        UnwindThreadByParseStackIfNeed(process, unwThread, unwinder);
        Printer::PrintDumpHeader(request, process, unwinder);
        Printer::PrintThreadHeaderByConfig(process->keyThread_);
        Printer::PrintThreadBacktraceByConfig(unwThread);
        if (ProcessDumper::GetInstance().IsCrash()) {
            // Registers of unwThread has been changed, we should print regs from request context.
            Printer::PrintRegsByConfig(DfxRegs::CreateFromUcontext(request->context));
            if (!DfxConfig::GetConfig().dumpOtherThreads) {
                break;
            }
        }

        auto threads = process->GetOtherThreads();
        if (threads.empty()) {
            break;
        }

        size_t index = 0;
        for (auto thread : threads) {
            if ((index == 0) && ProcessDumper::GetInstance().IsCrash()) {
                Printer::PrintOtherThreadHeaderByConfig();
            }

            if (thread->Attach()) {
                Printer::PrintThreadHeaderByConfig(thread);
                unwinder->UnwindRemote(thread->threadInfo_.tid, false, DfxConfig::GetConfig().maxFrameNums);
                thread->Detach();
                thread->SetFrames(unwinder->GetFrames());
                Printer::PrintThreadBacktraceByConfig(thread);
            }
            index++;
        }
        ret = true;
    } while (false);

    if (ProcessDumper::GetInstance().IsCrash()) {
        Printer::PrintThreadFaultStackByConfig(process, unwThread, unwinder);
        Printer::PrintProcessMapsByConfig(unwinder->GetMaps());
    }

    return ret;
}

void DfxUnwindRemote::UnwindThreadFallback(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread,
                                           std::shared_ptr<Unwinder> unwinder)
{
    if (unwinder->GetFrames().size() > 0) {
        return;
    }
    // As we failed to init libunwind, just print pc and lr for first two frames
    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
    if (regs == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("RegisterInfo is not existed for crash process");
        return;
    }

    std::shared_ptr<DfxMaps> maps = unwinder->GetMaps();
    if (maps == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("MapsInfo is not existed for crash process");
        return;
    }

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
}
} // namespace HiviewDFX
} // namespace OHOS
