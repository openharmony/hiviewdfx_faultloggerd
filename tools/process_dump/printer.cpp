/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "printer.h"

#include <cinttypes>
#include <dlfcn.h>
#include <map>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#include "dfx_config.h"
#include "dfx_frame_formatter.h"
#include "dfx_logger.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_signal.h"
#include "dfx_util.h"
#include "string_util.h"

#ifndef PAGE_SIZE
constexpr size_t PAGE_SIZE = 4096;
#endif

namespace OHOS {
namespace HiviewDFX {
void Printer::PrintDumpHeader(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                              std::shared_ptr<Unwinder> unwinder)
{
    bool isCrash = (request->siginfo.si_signo != SIGDUMP);
    if (isCrash) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("Timestamp:" + GetCurrentTimeStr(request->timeStamp));
    } else {
        DfxRingBufferWrapper::GetInstance().AppendMsg("Timestamp:" + GetCurrentTimeStr());
    }
    DfxRingBufferWrapper::GetInstance().AppendBuf("Pid:%d\n", process->processInfo_.pid);
    DfxRingBufferWrapper::GetInstance().AppendBuf("Uid:%d\n", process->processInfo_.uid);
    DfxRingBufferWrapper::GetInstance().AppendBuf("Process name:%s\n", process->processInfo_.processName.c_str());

    if (isCrash) {
        PrintReason(request, process, unwinder);
        auto msg = process->GetFatalMessage();
        if (!msg.empty()) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("LastFatalMessage:%s\n", msg.c_str());
        }

        auto traceId = request->traceInfo;
        if (traceId.chainId != 0) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("TraceId:%llx\n",
                static_cast<unsigned long long>(traceId.chainId));
        }

        if (process->vmThread_ != nullptr) {
            DfxRingBufferWrapper::GetInstance().AppendMsg("Fault thread Info:\n");
        }
    }
}

void Printer::PrintReason(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                          std::shared_ptr<Unwinder> unwinder)
{
    DfxRingBufferWrapper::GetInstance().AppendMsg("Reason:");
    process->reason += DfxSignal::PrintSignal(request->siginfo);
    uint64_t addr = (uint64_t)(request->siginfo.si_addr);
    if (request->siginfo.si_signo == SIGSEGV &&
        (request->siginfo.si_code == SEGV_MAPERR || request->siginfo.si_code == SEGV_ACCERR)) {
        if (addr < PAGE_SIZE) {
            process->reason += " probably caused by NULL pointer dereference";
        } else {
            std::shared_ptr<DfxMaps> maps = unwinder->GetMaps();
            std::vector<std::shared_ptr<DfxMap>> map;
            if (process->vmThread_ == nullptr) {
                DFXLOG_WARN("vmThread_ is nullptr");
                return;
            }
            auto regs = DfxRegs::CreateFromUcontext(request->context);
            if (regs == nullptr) {
                DFXLOG_WARN("regs is nullptr");
                return;
            }
            std::string elfName = StringPrintf("[anon:stack:%d]", process->keyThread_->threadInfo_.tid);
            if (maps != nullptr && maps->FindMapsByName(elfName, map)) {
                std::string guardMapName = StringPrintf("[anon:guard:%d]", process->keyThread_->threadInfo_.tid);
                if ((addr < map[0]->begin && map[0]->begin - addr <= PAGE_SIZE) ||
                    (map[0]->name.compare(guardMapName) == 0)) {
                    process->reason += StringPrintf(
#if defined(__LP64__)
                        " current thread stack low address = %#018llx, probably caused by stack-buffer-overflow",
#else
                        " current thread stack low address = %#010llx, probably caused by stack-buffer-overflow",
#endif
                        map[0]->begin);
                }
            }
        }
    }
    process->reason += "\n";
    DfxRingBufferWrapper::GetInstance().AppendMsg(process->reason);
}

void Printer::PrintProcessMapsByConfig(std::shared_ptr<DfxMaps> maps)
{
    if (DfxConfig::GetConfig().displayMaps) {
        if (maps == nullptr) {
            return;
        }
        auto mapsVec = maps->GetMaps();
        DfxRingBufferWrapper::GetInstance().AppendMsg("\nMaps:\n");
        for (auto iter = mapsVec.begin(); iter != mapsVec.end(); iter++) {
            DfxRingBufferWrapper::GetInstance().AppendMsg((*iter)->ToString());
        }
    }
}

void Printer::PrintOtherThreadHeaderByConfig()
{
    if (DfxConfig::GetConfig().displayBacktrace) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("Other thread info:\n");
    }
}

void Printer::PrintThreadHeaderByConfig(std::shared_ptr<DfxThread> thread)
{
    if (DfxConfig::GetConfig().displayBacktrace) {
        DfxRingBufferWrapper::GetInstance().AppendBuf("Tid:%d, Name:%s\n",\
            thread->threadInfo_.tid, thread->threadInfo_.threadName.c_str());
    }
}

void Printer::PrintThreadBacktraceByConfig(std::shared_ptr<DfxThread> thread)
{
    if (DfxConfig::GetConfig().displayBacktrace) {
        const auto& frames = thread->GetFrames();
        if (frames.size() == 0) {
            return;
        }
        for (const auto& frame : frames) {
            DfxRingBufferWrapper::GetInstance().AppendMsg(DfxFrameFormatter::GetFrameStr(frame));
        }
    }
}

void Printer::PrintThreadRegsByConfig(std::shared_ptr<DfxThread> thread)
{
    if (DfxConfig::GetConfig().displayRegister) {
        auto regs = thread->GetThreadRegs();
        if (regs != nullptr) {
            DfxRingBufferWrapper::GetInstance().AppendMsg(regs->PrintRegs());
        }
    }
}

void Printer::PrintRegsByConfig(std::shared_ptr<DfxRegs> regs)
{
    if (regs == nullptr) {
        return;
    }
    if (DfxConfig::GetConfig().displayRegister) {
        DfxRingBufferWrapper::GetInstance().AppendMsg(regs->PrintRegs());
    }
}

void Printer::PrintThreadFaultStackByConfig(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread,
                                            std::shared_ptr<Unwinder> unwinder)
{
    if (DfxConfig::GetConfig().displayFaultStack) {
        if (process == nullptr) {
            return;
        }
        thread->InitFaultStack();
        auto faultStack = thread->GetFaultStack();
        faultStack->CollectRegistersBlock(thread->GetThreadRegs(), unwinder->GetMaps());
        faultStack->Print();
    }
}
} // namespace HiviewDFX
} // namespace OHOS
