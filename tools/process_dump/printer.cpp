/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#include "crash_exception.h"
#include "string_printf.h"
#include "string_util.h"
#ifndef is_ohos_lite
#include "parameter.h"
#include "parameters.h"
#endif

#ifndef PAGE_SIZE
constexpr size_t PAGE_SIZE = 4096;
#endif

namespace OHOS {
namespace HiviewDFX {
void Printer::PrintDumpHeader(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                              std::shared_ptr<Unwinder> unwinder)
{
    if (process == nullptr || request == nullptr) {
        return;
    }
    std::string headerInfo;
    bool isCrash = (request->siginfo.si_signo != SIGDUMP);
#ifndef is_ohos_lite
    if (isCrash) {
        std::string buildInfo = OHOS::system::GetParameter("const.product.software.version", "Unknown");
        headerInfo = "Build info:" + buildInfo + "\n";
        DfxRingBufferWrapper::GetInstance().AppendMsg("Build info:" + buildInfo + "\n");
    }
#endif
    headerInfo += "Timestamp:" + GetCurrentTimeStr(request->timeStamp);
    DfxRingBufferWrapper::GetInstance().AppendMsg("Timestamp:" + GetCurrentTimeStr(request->timeStamp));
    headerInfo += "Pid:" + std::to_string(process->processInfo_.pid) + "\n" +
                  "Uid:" + std::to_string(process->processInfo_.uid) + "\n" +
                  "Process name:" + process->processInfo_.processName + "\n";
    DfxRingBufferWrapper::GetInstance().AppendBuf("Pid:%d\n", process->processInfo_.pid);
    DfxRingBufferWrapper::GetInstance().AppendBuf("Uid:%d\n", process->processInfo_.uid);
    DfxRingBufferWrapper::GetInstance().AppendBuf("Process name:%s\n", process->processInfo_.processName.c_str());
    if (isCrash) {
        auto lifeCycle = DfxProcess::GetProcessLifeCycle(process->processInfo_.pid);
        DfxRingBufferWrapper::GetInstance().AppendBuf("Process life time:%s\n", lifeCycle.c_str());
        if (lifeCycle.empty()) {
            ReportCrashException(request->processName, request->pid, request->uid,
                                 CrashExceptionCode::CRASH_LOG_EPROCESS_LIFECYCLE);
        }

        std::string reasonInfo;
        PrintReason(request, process, unwinder, reasonInfo);
        headerInfo += reasonInfo + "\n";
        auto msg = process->GetFatalMessage();
        if (!msg.empty()) {
            headerInfo += "LastFatalMessage:" + msg + "\n";
            DfxRingBufferWrapper::GetInstance().AppendBuf("LastFatalMessage:%s\n", msg.c_str());
        }

        headerInfo += "Fault thread info:\n";
        DfxRingBufferWrapper::GetInstance().AppendMsg("Fault thread info:\n");
    }
    DfxRingBufferWrapper::GetInstance().AppendBaseInfo(headerInfo);
}

void Printer::PrintReason(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                          std::shared_ptr<Unwinder> unwinder, std::string& reasonInfo)
{
    reasonInfo += "Reason:";
    DfxRingBufferWrapper::GetInstance().AppendMsg("Reason:");
    if (process == nullptr) {
        DFXLOGW("process is nullptr");
        return;
    }
    process->reason += DfxSignal::PrintSignal(request->siginfo);
    uint64_t addr = (uint64_t)(request->siginfo.si_addr);
    if (request->siginfo.si_signo == SIGSEGV &&
        (request->siginfo.si_code == SEGV_MAPERR || request->siginfo.si_code == SEGV_ACCERR)) {
        if (addr < PAGE_SIZE) {
            process->reason += " probably caused by NULL pointer dereference\n";
            DfxRingBufferWrapper::GetInstance().AppendMsg(process->reason);
            reasonInfo += process->reason;
            return;
        }
        if (unwinder == nullptr || process->keyThread_ == nullptr) {
            DFXLOGW("%{public}s is nullptr", unwinder == nullptr ? "unwinder" : "keyThread_");
            return;
        }
        std::shared_ptr<DfxMaps> maps = unwinder->GetMaps();
        std::vector<std::shared_ptr<DfxMap>> map;
        if (DfxRegs::CreateFromUcontext(request->context) == nullptr) {
            DFXLOGW("regs is nullptr");
            return;
        }
        std::string elfName = StringPrintf("[anon:stack:%d]", process->keyThread_->threadInfo_.tid);
        if (maps != nullptr && maps->FindMapsByName(elfName, map)) {
            if (map[0] != nullptr && (addr < map[0]->begin && map[0]->begin - addr <= PAGE_SIZE)) {
                process->reason += StringPrintf(
                    " current thread stack low address = %" PRIX64_ADDR ", probably caused by stack-buffer-overflow",
                    map[0]->begin);
            }
        }
    } else if (request->siginfo.si_signo == SIGSYS && request->siginfo.si_code == SYS_SECCOMP) {
        process->reason += StringPrintf(" syscall number is %d", request->siginfo.si_syscall);
    }
    process->reason += "\n";
    DfxRingBufferWrapper::GetInstance().AppendMsg(process->reason);
    reasonInfo += process->reason;
}

void Printer::PrintProcessMapsByConfig(std::shared_ptr<DfxMaps> maps)
{
    if (DfxConfig::GetConfig().displayMaps) {
        if (maps == nullptr) {
            return;
        }
        auto mapsVec = maps->GetMaps();
        DfxRingBufferWrapper::GetInstance().AppendMsg("\nMaps:\n");
        for (auto iter = mapsVec.begin(); iter != mapsVec.end() && (*iter) != nullptr; iter++) {
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

void Printer::PrintThreadHeaderByConfig(std::shared_ptr<DfxThread> thread, bool isKeyThread)
{
    std::string headerInfo;
    if (DfxConfig::GetConfig().displayBacktrace && thread != nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendBuf("Tid:%d, Name:%s\n",\
            thread->threadInfo_.tid, thread->threadInfo_.threadName.c_str());
        headerInfo = "Tid:" + std::to_string(thread->threadInfo_.tid) +
            ", Name:" + thread->threadInfo_.threadName + "\n";
    }
    if (isKeyThread) {
        DfxRingBufferWrapper::GetInstance().AppendBaseInfo(headerInfo);
    }
}

bool Printer::IsLastValidFrame(const DfxFrame& frame)
{
    static uintptr_t libcStartPc = 0;
    static uintptr_t libffrtStartEntry = 0;
    if (((libcStartPc != 0) && (frame.pc == libcStartPc)) ||
        ((libffrtStartEntry != 0) && (frame.pc == libffrtStartEntry))) {
        return true;
    }

    if (frame.mapName.find("ld-musl-aarch64.so.1") != std::string::npos &&
        frame.funcName.find("start") != std::string::npos) {
        libcStartPc = frame.pc;
        return true;
    }

    if (frame.mapName.find("libffrt") != std::string::npos &&
        frame.funcName.find("CoStartEntry") != std::string::npos) {
        libffrtStartEntry = frame.pc;
        return true;
    }

    return false;
}

void Printer::PrintThreadBacktraceByConfig(std::shared_ptr<DfxThread> thread, bool isKeyThread)
{
    if (DfxConfig::GetConfig().displayBacktrace && thread != nullptr) {
        const auto& frames = thread->GetFrames();
        if (frames.size() == 0) {
            return;
        }
        bool needSkip = false;
        bool isSubmitter = true;
        for (const auto& frame : frames) {
            if (frame.index == 0) {
                isSubmitter = !isSubmitter;
            }
            if (isSubmitter) {
                DfxRingBufferWrapper::GetInstance().AppendMsg("========SubmitterStacktrace========\n");
                DfxRingBufferWrapper::GetInstance().AppendBaseInfo("========SubmitterStacktrace========\n");
                isSubmitter = false;
                needSkip = false;
            }
            if (needSkip) {
                continue;
            }
            DfxRingBufferWrapper::GetInstance().AppendMsg(DfxFrameFormatter::GetFrameStr(frame));
            if (isKeyThread) {
                DfxRingBufferWrapper::GetInstance().AppendBaseInfo(DfxFrameFormatter::GetFrameStr(frame));
            }
#if defined(__aarch64__)
            if (IsLastValidFrame(frame)) {
                needSkip = true;
            }
#endif
        }
    }
}

void Printer::PrintThreadRegsByConfig(std::shared_ptr<DfxThread> thread)
{
    if (thread == nullptr) {
        return;
    }
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
        DfxRingBufferWrapper::GetInstance().AppendBaseInfo(regs->PrintRegs());
    }
}

void Printer::CollectThreadFaultStackByConfig(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread,
                                              std::shared_ptr<Unwinder> unwinder)
{
    if (DfxConfig::GetConfig().displayFaultStack) {
        if (process == nullptr || thread == nullptr) {
            return;
        }
        thread->InitFaultStack();
        auto faultStack = thread->GetFaultStack();
        if (faultStack == nullptr) {
            return;
        }
        if (process->regs_ == nullptr) {
            DFXLOGE("process regs is nullptr");
            return;
        }
        faultStack->CollectRegistersBlock(process->regs_, unwinder->GetMaps());
    }
}

void Printer::PrintThreadFaultStackByConfig(std::shared_ptr<DfxThread> thread)
{
    if (DfxConfig::GetConfig().displayFaultStack) {
        if (thread == nullptr) {
            return;
        }
        auto faultStack = thread->GetFaultStack();
        if (faultStack == nullptr) {
            return;
        }
        faultStack->Print();
    }
}

void Printer::PrintLongInformation(const std::string& info)
{
    constexpr size_t step = 1024;
    for (size_t i = 0; i < info.size(); i += step) {
        DfxRingBufferWrapper::GetInstance().AppendMsg(info.substr(i, step));
    }
}
} // namespace HiviewDFX
} // namespace OHOS
