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

#include "crash_exception.h"
#include "dfx_unwind_async_thread.h"
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_frame_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_logger.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_ptrace.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_symbols.h"
#include "dfx_thread.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#ifdef PARSE_LOCK_OWNER
#include "lock_parser.h"
#endif
#include "process_dumper.h"
#include "printer.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
void PrintTidKernelStack(pid_t tid)
{
    std::string kernelStack;
    if (DfxGetKernelStack(tid, kernelStack) == 0) {
        DFXLOG_INFO("tid(%d) kernel stack %s", tid, kernelStack.c_str());
    }
}
}

DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                    std::shared_ptr<Unwinder> unwinder, pid_t vmPid)
{
    DFX_TRACE_SCOPED("UnwindProcess");
    bool ret = false;
    if (process == nullptr || unwinder == nullptr) {
        DFXLOG_WARN("%s::process or unwinder is not initialized.", __func__);
        return ret;
    }

    SetCrashProcInfo(process->processInfo_.processName, process->processInfo_.pid,
                     process->processInfo_.uid);
    UnwindKeyThread(request, process, unwinder, vmPid);

    // dumpt -p -t will not unwind other thread
    if (ProcessDumper::GetInstance().IsCrash() || request->siginfo.si_value.sival_int == 0) {
        UnwindOtherThread(process, unwinder, vmPid);
    }

    ret = true;
    if (ProcessDumper::GetInstance().IsCrash()) {
        if (request->dumpMode == SPLIT_MODE) {
            if (process->vmThread_ == nullptr) {
                DFXLOG_WARN("%s::unwind vm thread is not initialized.", __func__);
            } else {
                Printer::PrintThreadFaultStackByConfig(process, process->vmThread_, unwinder);
            }
        } else {
            if (process->keyThread_ == nullptr) {
                DFXLOG_WARN("%s::unwind key thread is not initialized.", __func__);
            } else {
                pid_t nsTid = process->keyThread_->threadInfo_.nsTid;
                process->keyThread_->threadInfo_.nsTid = vmPid; // read registers from vm process
                Printer::PrintThreadFaultStackByConfig(process, process->keyThread_, unwinder);
                process->keyThread_->threadInfo_.nsTid = nsTid;
            }
        }
        Printer::PrintProcessMapsByConfig(unwinder->GetMaps());
        Printer::PrintThreadOpenFiles(process);
    }

    if (isVmProcAttach) {
        DfxPtrace::Detach(vmPid);
    }

    return ret;
}

void DfxUnwindRemote::UnwindKeyThread(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                      std::shared_ptr<Unwinder> unwinder, pid_t vmPid)
{
    std::shared_ptr<DfxThread> unwThread = process->keyThread_;
    if (ProcessDumper::GetInstance().IsCrash() && (process->vmThread_ != nullptr)) {
        unwThread = process->vmThread_;
    }
    if (unwThread == nullptr) {
        DFXLOG_WARN("%s::unwind thread is not initialized.", __func__);
        return;
    }
    if (request == nullptr) {
        DFXLOG_WARN("%s::request is not initialized.", __func__);
        return;
    }
    unwinder->SetIsJitCrashFlag(ProcessDumper::GetInstance().IsCrash());
    auto unwindAsyncThread = std::make_shared<DfxUnwindAsyncThread>(unwThread, unwinder, request->stackId);
    if ((vmPid != 0)) {
        if (DfxPtrace::Attach(vmPid, PTRACE_ATTATCH_KEY_THREAD_TIMEOUT)) {
            isVmProcAttach = true;
            if (unwThread->GetThreadRegs() != nullptr) {
                unwindAsyncThread->UnwindStack(vmPid);
            } else {
                PrintTidKernelStack(unwThread->threadInfo_.nsTid);
            }
        }
    } else {
        unwindAsyncThread->UnwindStack();
    }

    std::string fatalMsg = process->GetFatalMessage() + unwindAsyncThread->tip;
    if (!unwindAsyncThread->tip.empty()) {
        if (ProcessDumper::GetInstance().IsCrash()) {
            ReportCrashException(process->processInfo_.processName, process->processInfo_.pid,
                                 process->processInfo_.uid, CrashExceptionCode::CRASH_UNWIND_ESTACK);
        }
    }
    process->SetFatalMessage(fatalMsg);
    Printer::PrintDumpHeader(request, process, unwinder);
    Printer::PrintThreadHeaderByConfig(process->keyThread_, true);
    Printer::PrintThreadBacktraceByConfig(unwThread, true);
    if (ProcessDumper::GetInstance().IsCrash()) {
        // Registers of unwThread has been changed, we should print regs from request context.
        process->regs_ = DfxRegs::CreateFromUcontext(request->context);
        Printer::PrintRegsByConfig(process->regs_);
    }
}

void DfxUnwindRemote::UnwindOtherThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<Unwinder> unwinder,
    pid_t vmPid)
{
    if (!DfxConfig::GetConfig().dumpOtherThreads) {
        return;
    }

    if (vmPid != 0 && !isVmProcAttach) {
        return;
    }
    unwinder->SetIsJitCrashFlag(false);
    size_t index = 0;
    for (auto &thread : process->GetOtherThreads()) {
        if ((index == 0) && ProcessDumper::GetInstance().IsCrash()) {
            Printer::PrintOtherThreadHeaderByConfig();
        }

        if (isVmProcAttach || thread->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT)) {
            Printer::PrintThreadHeaderByConfig(thread, false);
            auto regs = thread->GetThreadRegs();
            unwinder->SetRegs(regs);
            bool withRegs = regs != nullptr;
            pid_t tid = thread->threadInfo_.nsTid;
            if (isVmProcAttach && !withRegs) {
                PrintTidKernelStack(tid);
                continue;
            }
            DFXLOG_INFO("%s, unwind tid(%d) start", __func__, tid);
            auto pid = (vmPid != 0 && isVmProcAttach) ? vmPid : tid;
            DFX_TRACE_START("OtherThreadUnwindRemote:%d", tid);
            bool ret = unwinder->UnwindRemote(pid, withRegs, DfxConfig::GetConfig().maxFrameNums);
            DFX_TRACE_FINISH();
#ifdef PARSE_LOCK_OWNER
            DFX_TRACE_START("OtherThreadGetFrames:%d", tid);
            thread->SetFrames(unwinder->GetFrames());
            DFX_TRACE_FINISH();
            LockParser::ParseLockInfo(unwinder, tmpPid, tid);
#else
            thread->Detach();
            DFX_TRACE_START("OtherThreadGetFrames:%d", tid);
            thread->SetFrames(unwinder->GetFrames());
            DFX_TRACE_FINISH();
#endif
            if (ProcessDumper::GetInstance().IsCrash()) {
                ReportUnwinderException(unwinder->GetLastErrorCode());
            }
            DFXLOG_INFO("%s, unwind tid(%d) finish ret(%d).", __func__, tid, ret);
            Printer::PrintThreadBacktraceByConfig(thread, false);
        }
        index++;
    }
}

bool DfxUnwindRemote::InitTargetKeyThreadRegs(std::shared_ptr<ProcessDumpRequest> request,
    std::shared_ptr<DfxProcess> process)
{
    auto regs = DfxRegs::CreateFromUcontext(request->context);
    if (regs == nullptr) {
        return false;
    }
    process->keyThread_->SetThreadRegs(regs);
    return true;
}

void DfxUnwindRemote::InitOtherThreadRegs(std::shared_ptr<DfxProcess> process)
{
    if (!DfxConfig::GetConfig().dumpOtherThreads) {
        return;
    }

    for (auto &thread : process->GetOtherThreads()) {
        if (thread->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT)) {
            thread->SetThreadRegs(DfxRegs::CreateRemoteRegs(thread->threadInfo_.nsTid));
        }
    }
}

bool DfxUnwindRemote::InitProcessAllThreadRegs(std::shared_ptr<ProcessDumpRequest> request,
    std::shared_ptr<DfxProcess> process)
{
    if (!InitTargetKeyThreadRegs(request, process)) {
        DFXLOG_ERROR("%s", "get key thread regs fail");
        return false;
    }
    InitOtherThreadRegs(process);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
