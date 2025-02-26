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
#ifndef is_ohos_lite
#include "parameter.h"
#include "parameters.h"
#endif // !is_ohos_lite
#include "process_dumper.h"
#include "printer.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
void GetThreadKernelStack(std::shared_ptr<DfxThread> thread)
{
    std::string threadKernelStack;
    pid_t tid = thread->threadInfo_.nsTid;
    DfxThreadStack threadStack;
    if (DfxGetKernelStack(tid, threadKernelStack) == 0 && FormatThreadKernelStack(threadKernelStack, threadStack)) {
        DFXLOGI("Failed to get tid(%{public}d) user stack, try kernel", tid);
#ifndef is_ohos_lite
        if (OHOS::system::GetParameter("const.logsystem.versiontype", "false") == "beta") {
            DFXLOGI("%{public}s", threadKernelStack.c_str());
        }
#endif
        thread->SetParseSymbolNecessity(false);
        thread->SetFrames(threadStack.frames);
    }
}
}

DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

void DfxUnwindRemote::ParseSymbol(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                  std::shared_ptr<Unwinder> unwinder)
{
    if (request == nullptr || process == nullptr || unwinder == nullptr) {
        return;
    }

    std::shared_ptr<DfxThread> unwThread = process->keyThread_;
    DFX_TRACE_START("ParseSymbol keyThread:%d", unwThread->threadInfo_.nsTid);
    unwThread->ParseSymbol(unwinder);
    DFX_TRACE_FINISH();
    if (ProcessDumper::GetInstance().IsCrash() || request->siginfo.si_value.sival_int == 0) {
        for (auto &thread : process->GetOtherThreads()) {
            DFX_TRACE_START("ParseSymbol otherThread:%d", thread->threadInfo_.nsTid);
            thread->ParseSymbol(unwinder);
            DFX_TRACE_FINISH();
        }
    }
}

void DfxUnwindRemote::PrintUnwindResultInfo(std::shared_ptr<ProcessDumpRequest> request,
                                            std::shared_ptr<DfxProcess> process,
                                            std::shared_ptr<Unwinder> unwinder,
                                            pid_t vmPid)
{
    if (request == nullptr || process == nullptr || unwinder == nullptr) {
        return;
    }

    // print key thread
    Printer::PrintDumpHeader(request, process, unwinder);
    Printer::PrintThreadHeaderByConfig(process->keyThread_, true);
    Printer::PrintThreadBacktraceByConfig(process->keyThread_, true);
    if (ProcessDumper::GetInstance().IsCrash()) {
        Printer::PrintRegsByConfig(process->regs_);
        Printer::PrintLongInformation(process->extraCrashInfo);
    }

    // print other thread
    if (ProcessDumper::GetInstance().IsCrash() || request->siginfo.si_value.sival_int == 0) {
        size_t index = 0;
        for (auto &thread : process->GetOtherThreads()) {
            if ((index == 0) && ProcessDumper::GetInstance().IsCrash()) {
                Printer::PrintOtherThreadHeaderByConfig();
            }
            Printer::PrintThreadHeaderByConfig(thread, false);
            Printer::PrintThreadBacktraceByConfig(thread, false);
            index++;
        }
    }

    if (ProcessDumper::GetInstance().IsCrash()) {
        if (process->keyThread_ == nullptr) {
            DFXLOGW("%{public}s::unwind key thread is not initialized.", __func__);
        } else {
            pid_t nsTid = process->keyThread_->threadInfo_.nsTid;
            process->keyThread_->threadInfo_.nsTid = vmPid; // read registers from vm process
            Printer::PrintThreadFaultStackByConfig(process->keyThread_);
            process->keyThread_->threadInfo_.nsTid = nsTid;
        }
        Printer::PrintProcessMapsByConfig(unwinder->GetMaps());
        Printer::PrintLongInformation(process->openFiles);
    }
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                    std::shared_ptr<Unwinder> unwinder, pid_t vmPid)
{
    DFX_TRACE_SCOPED("UnwindProcess");
    if (process == nullptr || unwinder == nullptr) {
        DFXLOGW("%{public}s::process or unwinder is not initialized.", __func__);
        return false;
    }

    unwinder->EnableFillFrames(false);
    SetCrashProcInfo(process->processInfo_.processName, process->processInfo_.pid,
                     process->processInfo_.uid);
    int unwCnt = UnwindKeyThread(request, process, unwinder, vmPid) ? 1 : 0;

    // dumpt -p -t will not unwind other thread
    if (ProcessDumper::GetInstance().IsCrash() || request->siginfo.si_value.sival_int == 0) {
        unwCnt += UnwindOtherThread(process, unwinder, vmPid);
    }

    if (ProcessDumper::GetInstance().IsCrash() && process->keyThread_ != nullptr) {
        pid_t nsTid = process->keyThread_->threadInfo_.nsTid;
        process->keyThread_->threadInfo_.nsTid = vmPid; // read registers from vm process
        Printer::CollectThreadFaultStackByConfig(process, process->keyThread_, unwinder);
        process->keyThread_->threadInfo_.nsTid = nsTid;
    }
    DFXLOGI("success unwind thread cnt is %{public}d", unwCnt);
    return unwCnt > 0;
}

bool DfxUnwindRemote::UnwindKeyThread(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                      std::shared_ptr<Unwinder> unwinder, pid_t vmPid)
{
    bool result = false;
    std::shared_ptr<DfxThread> unwThread = process->keyThread_;
    if (unwThread == nullptr) {
        DFXLOGW("%{public}s::unwind thread is not initialized.", __func__);
        return false;
    }
    if (request == nullptr) {
        DFXLOGW("%{public}s::request is not initialized.", __func__);
        return false;
    }
    unwinder->SetIsJitCrashFlag(ProcessDumper::GetInstance().IsCrash());
    auto unwindAsyncThread = std::make_shared<DfxUnwindAsyncThread>(unwThread, unwinder, request->stackId);
    if ((vmPid != 0)) {
        isVmProcAttach = true;
        if (unwThread->GetThreadRegs() != nullptr) {
            result = unwindAsyncThread->UnwindStack(vmPid);
        } else {
            GetThreadKernelStack(unwThread);
        }
    } else {
        result = unwindAsyncThread->UnwindStack();
    }

    if (!unwindAsyncThread->unwindFailTip.empty()) {
        ReportCrashException(process->processInfo_.processName, process->processInfo_.pid,
                             process->processInfo_.uid, CrashExceptionCode::CRASH_UNWIND_ESTACK);
        process->extraCrashInfo += ("ExtraCrashInfo(Unwindstack):\n" + unwindAsyncThread->unwindFailTip);
    }
    if (ProcessDumper::GetInstance().IsCrash()) {
        // Registers of unwThread has been changed, we should print regs from request context.
        process->regs_ = DfxRegs::CreateFromUcontext(request->context);
    }
    return result;
}

int DfxUnwindRemote::UnwindOtherThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<Unwinder> unwinder,
    pid_t vmPid)
{
    if ((!DfxConfig::GetConfig().dumpOtherThreads) || (vmPid != 0 && !isVmProcAttach)) {
        return 0;
    }
    unwinder->SetIsJitCrashFlag(false);
    int unwCnt = 0;
    for (auto &thread : process->GetOtherThreads()) {
        if (isVmProcAttach || thread->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT)) {
            auto regs = thread->GetThreadRegs();
            unwinder->SetRegs(regs);
            bool withRegs = regs != nullptr;
            pid_t tid = thread->threadInfo_.nsTid;
            DFXLOGD("%{public}s, unwind tid(%{public}d) start", __func__, tid);
            if (isVmProcAttach && !withRegs) {
                GetThreadKernelStack(thread);
                continue;
            }
            auto pid = (vmPid != 0 && isVmProcAttach) ? vmPid : tid;
            DFX_TRACE_START("OtherThreadUnwindRemote:%d", tid);
            bool ret = unwinder->UnwindRemote(pid, withRegs, DfxConfig::GetConfig().maxFrameNums);
            DFX_TRACE_FINISH();
            DFX_TRACE_START("OtherThreadGetFrames:%d", tid);
            thread->SetFrames(unwinder->GetFrames());
            DFX_TRACE_FINISH();
#ifdef PARSE_LOCK_OWNER
            LockParser::ParseLockInfo(unwinder, pid, tid);
#endif
            if (ProcessDumper::GetInstance().IsCrash()) {
                ReportUnwinderException(unwinder->GetLastErrorCode());
            }
            if (!ret) {
                DFXLOGW("%{public}s, unwind tid(%{public}d) finish ret(%{public}d).", __func__, tid, ret);
            } else {
                unwCnt++;
            }
        }
    }
    return unwCnt;
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
        DFXLOGE("get key thread regs fail");
        return false;
    }
    InitOtherThreadRegs(process);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
